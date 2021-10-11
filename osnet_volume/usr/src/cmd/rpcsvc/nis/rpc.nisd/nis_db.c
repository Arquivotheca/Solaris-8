/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nis_db.c	1.39	99/06/03 SMI"

/*
 * Ported from
 *	"@(#)nis_db.c 1.42 91/03/21 Copyr 1990 Sun Micro";
 *
 * This module contains the glue routines between the actual database
 * code and the NIS+ server. Presumably they are the routines that should
 * be exported in the shared library, but they may be at too high a level.
 */

#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <rpcsvc/nis_db.h>
#include "nis_proc.h"

static nis_error __create_table(char *, nis_object *);

extern bool_t xdr_nis_object();
extern bool_t xdr_nis_name();
extern bool_t xdr_nis_oid();
extern bool_t xdr_objdata(XDR *, objdata*);
extern bool_t xdr_entry_col(XDR*, entry_col *);
extern char *relative_name();
extern db_result *__db_add_entry_nosync(char *, int, nis_attr *, entry_obj *);
extern db_result *__db_remove_entry_nosync(char *, int, nis_attr *);
extern db_result *__db_add_entry_nolog(char *, int, nis_attr *, entry_obj *);

typedef struct table_list_entry {
	char			*table;
	struct table_list_entry	*next;
} table_list_entry_t;

static table_list_entry_t	*table_list = 0;

static int			mark_for_sync(char *);

NIS_HASH_TABLE *table_cache = NULL;

/*
 * Special abbreviated XDR string which knows that the namep parameter (mainly
 * owner and group) has a trailing end which matches the last 'n' characters
 * in the domainname part.  It makes use of those common characters to
 * encode/decode this information.  We append an integer string to the
 * name to be encoded which denotes the place in the domainname from where the
 * common string starts.  For example, if the name was "foo.my.domain." and the
 * domainname was "my.domain.", the name would be encoded as "foo.10" because
 * the length of the common part "my.domain." is 10.
 */
static bool_t
xdr_nis_name_abbrev(
	XDR		*xdrs,
	nis_name	*namep,
	nis_name	domainname)	/* domainname field from the table */
{
	size_t	name_len, dom_len, min_len;
	char 	buf[NIS_MAXNAMELEN];
	char 	*name;
	char	*lenstr, *tmp;
	int	i;

	switch (xdrs->x_op) {
	case XDR_ENCODE:
		/* Get the start of the common part */
		name = *namep;
		name_len = strlen(name);
		if (name_len == 0)
			return (xdr_nis_name(xdrs, namep));
		dom_len = strlen(domainname);
		min_len = (name_len < dom_len) ? name_len : dom_len;
		for (i = 1; i <= min_len; i++) {
			if (name[name_len - i] != domainname[dom_len - i])
				break;
		}
		i--;
		memcpy(buf, name, name_len - i);
		sprintf(buf + name_len - i, ".%d", dom_len - i);
		tmp = buf;
		return (xdr_nis_name(xdrs, &tmp));

	case XDR_DECODE:
		tmp = buf;
		if (!xdr_nis_name(xdrs, &tmp))
		    return (FALSE);
		if ((buf[0] == NULL) || buf[strlen(buf) - 1] == '.') {
			/* It is either a FQN or a NULL string */
			if (*namep) {
				strcpy(*namep, buf);
				return (TRUE);
			} else {
				if ((*namep = strdup(buf)) == NULL)
					return (FALSE);
				else
					return (TRUE);
			}
		}
		/* Now concoct the new name */
		if ((lenstr = strrchr(buf, '.')) == NULL) {
			/* something went wrong here */
			syslog(LOG_ERR,
				"xdr_nis_name_abbrev: no dot found in %s", buf);
			return (FALSE);
		}
		i = atoi(lenstr + 1);
		strcpy(lenstr, domainname + i);
		if (*namep) {
			strcpy(*namep, buf);
		} else {
			if ((*namep = strdup(buf)) == NULL)
				return (FALSE);
		}
		return (TRUE);

	default:
		return (xdr_nis_name(xdrs, namep));
	}
}

/*
 * special XDR for fetus object.  We create the actual object from the
 * "forming" object plus the table object.  We create this special object to
 * save the following components of the nis_object:
 *	zo_name and zo_domain: replaced by just the length field of 0.  We had
 *		to keep the length field for backward compatibility.  If we
 *		ever change the object format, we should fix this.
 *	zo_owner and zo_group: we condensed it by abbreviating the common part
 *		shared between the table object and the entry object
 *	en_type: Avoided altogether
 *	zo_type and other en_data: Avoided altogether.
 *
 * XXX: If the definition of nis_object ever changes, this should be changed.
 */
static bool_t
xdr_nis_fetus_object(
	XDR		*xdrs,
	nis_object	*objp,	/* Entry object */
	nis_object	*tobj)	/* Table object */
{
	u_int	size;

	if (xdrs->x_op == XDR_FREE)
		return (xdr_nis_object(xdrs, objp));
	if (!xdr_nis_oid(xdrs, &objp->zo_oid))
		return (FALSE);

	/*
	 * While encoding of zo_name, we put 0 in the length field, while for
	 * decoding, we get the name from the table object.
	 */
	if (xdrs->x_op == XDR_ENCODE) {
		size = 0;
		if (!xdr_u_int(xdrs, &size))
			return (FALSE);
	} else {
		if (!xdr_u_int(xdrs, &size))
			return (FALSE);
		if (size == 0) {	/* shrinked format */
			/* get the name from the table object */
			if ((objp->zo_name = strdup(tobj->zo_name)) == NULL)
				return (FALSE);
		} else {
			/*
			 * We are opening up the xdr_string implementation here
			 * because we called xdr_u_int() earlier.
			 */
			if ((objp->zo_name = (char *)malloc(size + 1)) == NULL)
				return (FALSE);
			if (!xdr_opaque(xdrs, objp->zo_name, size))
				return (FALSE);
		}
	}

	/*
	 * We use the xdr_nis_name_abbrev() function for both owner
	 * and group which constructs the name from the domain name.
	 */
	if (!xdr_nis_name_abbrev(xdrs, &objp->zo_owner, tobj->zo_domain))
		return (FALSE);
	if (!xdr_nis_name_abbrev(xdrs, &objp->zo_group, tobj->zo_domain))
		return (FALSE);

	/*
	 * While encoding of zo_domain, we put 0 in the length field, while for
	 * decoding, we get the name from the table object.  Same as above for
	 * the name.  Could have used a function instead.
	 */
	if (xdrs->x_op == XDR_ENCODE) {
		size = 0;
		if (!xdr_u_int(xdrs, &size))
			return (FALSE);
	} else {
		if (!xdr_u_int(xdrs, &size))
			return (FALSE);
		if (size == 0) {	/* shrinked format */
			/* get the name from the table object */
			if ((objp->zo_domain = strdup(tobj->zo_domain)) == NULL)
				return (FALSE);
		} else {
			/*
			 * We are opening up the xdr_string implementation here
			 * because we called xdr_u_int() earlier.
			 */
			if ((objp->zo_domain = (char *)malloc(size + 1))
				== NULL)
				return (FALSE);
			if (!xdr_opaque(xdrs, objp->zo_domain, size))
				return (FALSE);
		}
	}

	if (!xdr_u_int(xdrs, &objp->zo_access))
		return (FALSE);
	if (!xdr_u_int(xdrs, &objp->zo_ttl))
		return (FALSE);

	/*
	 * We know that this is an entry object, so we'll save all the entry_obj
	 * space because we can recreate it later.
	 */
	if (xdrs->x_op == XDR_ENCODE)
		return (TRUE);
	/* Now for the DECODE case, just handcraft the entries and ignore XDR */
	objp->zo_data.zo_type = NIS_ENTRY_OBJ;
	if ((objp->zo_data.objdata_u.en_data.en_type =
		strdup(tobj->zo_data.objdata_u.ta_data.ta_type)) == NULL)
		return (FALSE);
	objp->zo_data.objdata_u.en_data.en_cols.en_cols_val = NULL;
	objp->zo_data.objdata_u.en_data.en_cols.en_cols_len = 0;
	return (TRUE);
}

/*
 * Free resources associated with a db_result structure
 */
static void
free_db_result(db_result *dr)
{
	int	i;

	if (dr == 0)
		return;

	if (dr->status != DB_SUCCESS) {
		/* Can't have valid objects */
		free(dr);
		return;
	}

	for (i = 0; i < dr->objects.objects_len; i++)
		free_entry(dr->objects.objects_val[i]);
	free(dr->objects.objects_val);
	free(dr);
}

/*
 * SYSTEM DEPENDENT
 *
 * This function makes the table name "legal" for the underlying file system.
 */
void
__make_legal(char *s)
{
	while (*s) {
		if (isupper(*s))
			*s = tolower(*s);
		s++;
	}
}

/*
 * internal_table_name()
 *
 * This function removes the local domain part from a fully qualified name
 * to create the internal table name for an object. These tables are
 * stored in /var/nis/<hostname>
 */
char *
internal_table_name(nis_name name, char *res)
{
	char		*s, *t;
	int		i, j;


	if (res == NULL)
		return (NULL);
	/* pointer at the first character of the table name */
	s = relative_name(name);

	/*
	 * If s == NULL then either this is a request for a lookup
	 * in our parents namespace (ILLEGAL), or we're the root
	 * server and this is a lookup in our namespace.
	 */
	if (s) {
		strcpy(res, nis_data(s));
		free((void *)s);
	} else if (nis_dir_cmp(name, __nis_rpc_domain())  == SAME_NAME) {
		strcpy(res, nis_data("ROOT_DIR"));
	} else {
		return (NULL);
	}

	t = strrchr(res, '/');
	if (t)
		t++; /* Point past the slash */
	/* Strip off the quotes if they were used here. */
	if (t[0] == '"') {
		/* Check for simply a quoted quote. */
		if (t[1] != '"') {
			j = strlen(t);
			/* shift string left by one */
			for (i = 0; i < j; i++)
				t[i] = t[i+1];
			t[j-2] = '\0'; /* Trounce trailing dquote */
		}
	}
	/*
	 * OK so now we have the unique name for the table.
	 * At this point we can fix it up to match local
	 * file system conventions if we so desire. Since it
	 * is only used in this form by _this_ server we can
	 * mangle it any way we want, as long as we are consistent
	 * about it. :-)
	 */
	__make_legal(res);
	return (res);
}

static void
free_nis_db_result(nis_db_result *res)
{
	/* We do not free obj here because it is cached in table_cache */
	XFREE(res);
}

static nis_error
map_db_status_to_nis_status(db_status dstatus)
{
	switch (dstatus) {
	case DB_SUCCESS:
		return (NIS_SUCCESS);
	case DB_NOTFOUND:
		return (NIS_NOTFOUND);
	case DB_BADTABLE:
		return (NIS_NOSUCHTABLE);
	case DB_MEMORY_LIMIT:
		return (NIS_NOMEMORY);
	case DB_STORAGE_LIMIT:
		return (NIS_NOFILESPACE);
	case DB_NOTUNIQUE:
	case DB_BADQUERY:
		return (NIS_BADREQUEST);
	case DB_BADOBJECT:
		return (NIS_BADOBJECT);
	case DB_INTERNAL_ERROR:
	default:
		return (NIS_SYSTEMERROR);
	}
}

/*
 * This function converts the internal format entries of the DB into
 * a list of nis_objects that the server understands. The object returned may
 * be destroyed with nis_destroy_object();
 *
 * Notes : When listing directories the list function expects to see entry
 *	   objects and this function will mangle regular objects into entry
 *	   objects. The entry has one column which contains the binary
 *	   equivalent of what the type would have been if it hadn't been
 *	   mangled.
 *
 *	   When dumping directories we need the objects intact so we set mangle
 *	   to false and return the real objects.
 */
static obj_list *
cvt2object(
	nis_name	tablename,	/* table which has these entries */
	entry_obj	*ep[],
	u_int		num,
	int		*got,
	int		mangle)	/* Make non-entry objects into psuedo entries */
{
	register obj_list	*oa; 		/* object array 	*/
	nis_object		*tmp;		/* Temporary object(s) 	*/
	XDR			xdrs; 		/* temporary xdr stream */
	int			status,		/* XDR op status 	*/
				curr_obj,	/* Current Object 	*/
				i, j, mc;
	entry_obj		*eo; 		/* tmp, makes life easier */
	entry_col		*ec;
	unsigned long		etype;		/* for fake entries */
	struct table_item 	*te;		/* Table cache entry	*/
	struct nis_object	*tobj;		/* Table nis_object 	*/

	*got = 0; /* Number of objects decoded */
	te = (struct table_item *) nis_find_item(tablename, table_cache);
	if (te == NULL) {
		/* Do a db_lookup() so that cache is populated */
		nis_db_result *dbres;

		if (((dbres = db_lookup(tablename)) == NULL) ||
		    (dbres->status != NIS_SUCCESS))
			tobj = NULL;
		else
			tobj = dbres->obj;
		/* dbres is freed automatically during cleanup */
	} else {
		tobj = te->ibobj;
	}
	oa = (obj_list *)XCALLOC(num, sizeof (obj_list));
	if (oa == NULL)
		return (NULL);

	curr_obj = 0;
	for (i = 0; i < num; i++) {
		if (! ep[i]) {
			syslog(LOG_ERR,
			    "cvt2object: NULL Object in database, ignored.");
			continue;
		}
		ec = ep[i]->en_cols.en_cols_val;
		mc = ep[i]->en_cols.en_cols_len;
		/*
		 * Set up a memory stream pointing at the first column value
		 * which contains the XDR encoded NIS+ object.  The second
		 * column contains the name of the NIS+ object.
		 */
		xdrmem_create(&xdrs, ec->ENVAL, ec->ENLEN, XDR_DECODE);
		tmp = (nis_object *)XCALLOC(1, sizeof (nis_object));
		if (tmp == NULL) {
			/* I'll return with the current list of objects */
			return (oa);
		}
		/*
		 * Decode it into the object structure.  If some fields
		 * are NULL, fill in appropriate values from the table
		 * nis_object structure.
		 *
		 * If the entry object has type "IN_DIRECTORY" then it
		 * is a NIS+ directory we are listing else it is
		 * an ENTRY Object.  For ENTRY objects, we call our
		 * special xdr_nis_fetus_object() which knows how to
		 * reconstruct the entry object from the given info.
		 * XXX: _any_ other value we are hosed.  If it is 0 or a
		 * NULL string, it denotes that it is an entry object.
		 */
		if (((ep[i]->en_type == 0) || (ep[i]->en_type[0] == 0)) && tobj)
			status = xdr_nis_fetus_object(&xdrs, tmp, tobj);
		else
			status = xdr_nis_object(&xdrs, tmp);
		/*
		 * POLICY : What to do about undecodeable objects ?
		 * ANSWER : Leave it blank and continue. (soft failure)
		 */
		if (! status) {
			syslog(LOG_ERR,
		    "cvt2object: Corrupted object ('%s') in database %s",
					ec[1].ENVAL, tablename);
			XFREE(tmp);
			continue;
		}

		/*
		 * If the entry object has type 0 or "IN_TABLE" then it
		 * is an entry object.  If it has type "IN_DIRECTORY" then it
		 * is a NIS+ directory that we are listing.
		 * XXX: _any_ other value we are hosed.
		 */
		if ((ep[i]->en_type == 0) || (ep[i]->en_type[0] == 0) ||
		    strcmp(ep[i]->en_type, "IN_TABLE") == 0) {
			if (__type_of(tmp) != NIS_ENTRY_OBJ) {
				syslog(LOG_ERR,
	"cvt2object: Corrupt database, entry expected for %s", ec[1].ENVAL);
				xdr_free(xdr_nis_object, (char *)tmp);
				XFREE(tmp);
				continue;
			}
			/*
			 * Set the column fields appropriately. Copy all the
			 * col entry pointers to the new entry object.  We are
			 * mucking around with the list returned by
			 * db_list_entries - UNCLEAN, UNCLEAN!
			 */
			eo = &(tmp->EN_data);
			eo->en_cols.en_cols_len = mc - 1;
			eo->en_cols.en_cols_val = XMALLOC((mc - 1) *
							(sizeof (entry_col)));
			if (eo->en_cols.en_cols_val == NULL) {
				xdr_free(xdr_nis_object, (char *)tmp);
				XFREE(tmp);
				return (oa);
			}
			for (j = 1; j < mc; j++) {
				eo->en_cols.en_cols_val[j-1] =
					ep[i]->en_cols.en_cols_val[j];
			}
			/* We set len to 1, so that other cols are not freed */
			ep[i]->en_cols.en_cols_len = 1;
		} else if (mangle) {
			/* Convert this dir object into a "fake" entry object */
			etype = htonl(__type_of(tmp)); /* save the old type */
			/* first free the object specific data */
			xdr_free(xdr_objdata, (char *)&tmp->zo_data);
			memset(&tmp->zo_data, 0, sizeof (objdata));
			/* Now fake a entry object */
			__type_of(tmp) = NIS_ENTRY_OBJ;
			eo = &(tmp->EN_data);
			eo->en_type = strdup("NIS object"); /* special type */
			if (eo->en_type == NULL) {
				xdr_free(xdr_nis_object, (char *)tmp);
				return (oa);
			}
			ec = (entry_col *) XMALLOC(sizeof (entry_col));
			if (ec == NULL) {
				xdr_free(xdr_nis_object, (char *)tmp);
				return (oa);
			}
			eo->en_cols.en_cols_len = 1;
			eo->en_cols.en_cols_val = ec;
			ec[0].ec_flags = EN_BINARY + EN_XDR;
			ec[0].ENVAL = XMALLOC(4);
			if (ec[0].ENVAL == NULL) {
				xdr_free(xdr_nis_object, (char *)tmp);
				return (oa);
			}
			ec[0].ENLEN = 4;
			memcpy(ec[0].ENVAL, (char *) &etype, 4);
		}
		oa[curr_obj++].o = tmp;
		(*got)++; /* Add one to the total */
	}

	return	(oa);
}

/*
 * The following routines implement the database operations on the namespace.
 * The database only understands tables, so it is up these routines to
 * "fake out" the database and to build a table that defines the namespace
 * that we are serving. The format of this table is fixed.  It consists of
 * a table with two columns, the first being the object (XDR encoded)
 * and the second being the "name" of the object.
 *
 * Note : All of these routines assume they are passed "full" NIS+ names
 * Note : The fake entry structure is the way it is to be compatible with
 *	the cvt2object() function above.
 *
 * Lookup NIS+ objects in the namespace that are stored as entries in a
 * table by the same name as the directory being served. These entries
 * have two columns, column 0 is the object data and column 1 is the
 * object name.
 */
nis_db_result *
db_lookup(nis_name name)
{
	int		status;
	nis_attr	attr;
	db_result	*dbres;
	register entry_col *ec;
	XDR		xdrs;
	nis_db_result	*res;
	char		*table; 	/* The table path name	*/
	char		tblbuf[NIS_MAXNAMELEN * 2]; /* table path buf */
	struct table_item *te;		/* Table cache entry	*/

	if (verbose)
		syslog(LOG_INFO, "db_lookup: Looking for %s", name);

	res = (nis_db_result *) XCALLOC(1, sizeof (nis_db_result));
	/*
	 * XXX: we do not check for return value here because there would be no
	 * easy way to return the error condition without mallocing something.
	 */
	add_cleanup(free_nis_db_result, (void *)(res), "db_lookup result");

	if (!table_cache) {
		table_cache = (NIS_HASH_TABLE *)
			XCALLOC(1, sizeof (NIS_HASH_TABLE));
		if (!table_cache) {
			syslog(LOG_ERR, "db_lookup: out of memory");
			res->status = NIS_NOMEMORY;
			return (res);
		}
	}
	/*
	 * Now we check the cache to see if we have it cached locally.
	 */
	te = (struct table_item *) nis_find_item(name, table_cache);
	if (te) {
		if (verbose)
			syslog(LOG_INFO, "db_lookup: table cache hit");
		res->obj = te->ibobj;
		res->status = NIS_SUCCESS;
		return (res);
	}

	table = internal_table_name(nis_domain_of(name), tblbuf);
	/* This happens when we're looking for directory objects. */
	if (! table) {
		res->status = NIS_NOSUCHTABLE;
		return (res);
	}
	attr.zattr_ndx = "name";
	attr.ZAVAL = nis_leaf_of(name);
	attr.ZALEN = strlen(attr.ZAVAL)+1;

	if (verbose)
		syslog(LOG_INFO, "db_lookup: looking up %s in table %s",
						attr.ZAVAL, table);
	__start_clock(1);
	dbres = db_list_entries(table, 1, &attr);
	res->ticks = __stop_clock(1);
	if (dbres == NULL)
		res->status = NIS_NOMEMORY;
	else
		res->status = map_db_status_to_nis_status(dbres->status);
	if (res->status == NIS_SUCCESS) {
		/* ASSERT(dbres->objects.objects_len == 1); */

		/*
		 * convert from XDR format that the DB returns to
		 * the nis_object format
		 */
		ec = dbres->objects.objects_val[0]->en_cols.en_cols_val;

		xdrmem_create(&xdrs, ec->ENVAL, ec->ENLEN, XDR_DECODE);
		res->obj = (nis_object *)XCALLOC(1, sizeof (nis_object));
		if (!(res->obj))
			res->status = NIS_NOMEMORY;
		else {
			status = xdr_nis_object(&xdrs, res->obj);
			if (! status) {
				syslog(LOG_ERR, "db_lookup: Corrupt object %s",
						name);
				XFREE(res->obj);
				res->obj = NULL;
				res->status = NIS_SYSTEMERROR;
			}
		}
	}

	if (verbose && dbres)
		syslog(LOG_INFO, "db_lookup: exit status is %d", dbres->status);

	if (res->obj) {
		if ((__type_of(res->obj) == NIS_TABLE_OBJ) &&
			strstr(name, "org_dir")) {
			/*
			 * Cache the table objects in the "org_dir"
			 * dir.  We want to cache only the "org_dir" tables
			 * instead of caching all zillions of tables.
			 */
			te = (struct table_item *)XCALLOC(1, sizeof (*te));
			if (te) {
				te->ti_item.name = (nis_name) strdup(name);
				if (te->ti_item.name == NULL) {
					add_cleanup(nis_destroy_object,
					    (void *)(res->obj),
					    "db_lookup result");
					free(te);
				} else {
					te->ibobj = res->obj;
					nis_insert_item((NIS_HASH_ITEM *)te,
						table_cache);
					if (verbose)
						syslog(LOG_INFO,
						"Added %s to the table cache",
							name);
				}
			} else {
				add_cleanup(nis_destroy_object,
				    (void *)(res->obj), "db_lookup result");
			}
		} else {
			add_cleanup(nis_destroy_object,
				    (void *)(res->obj), "db_lookup result");
		}
	}

	free_db_result(dbres);
	return (res);
}

/*
 * __db_add()
 *
 * The internal version of db_add, this one doesn't add an entry into
 * the log.  This function converts the nis_object into a DB style object
 * and then adds it to the database.
 */
nis_error
__db_add(
	nis_name	name,
	nis_object	*obj,
	int		modop)	 /* Modify operation flag */
{
	nis_attr	attr;
	int		i;
	db_result	*dbres;
	entry_col	ecols[2];
	entry_obj	entry;
	char		*table;
	u_char		*buf;
	nis_error	res;
	XDR		xdrs;
	char		tblbuf[NIS_MAXNAMELEN * 2];

	if (verbose)
		syslog(LOG_INFO, "__db_add: attempting to %s %s",
			modop? "modify" : "add", name);

	if (CHILDPROC) {
		syslog(LOG_INFO,
			"__db_add: child process attempting an add/modify.");
		return (NIS_TRYAGAIN);
	}

	if ((__type_of(obj) == NIS_TABLE_OBJ) && (!modop)) {
		if ((res = __create_table(name, obj)) != NIS_SUCCESS) {
			syslog(LOG_ERR,
	    "__db_add: Unable to create database for NIS+ table %s: %s.",
				    name, nis_sperrno(res));
			return (res);
		}
	}

	table = internal_table_name(nis_domain_of(name), tblbuf);
	if (! table)
		return (NIS_BADNAME);

	i = xdr_sizeof(xdr_nis_object, obj);
	buf = __get_xdr_buf(i);
	xdrmem_create(&xdrs, (char *) buf, i, XDR_ENCODE);
	i = xdr_nis_object(&xdrs, obj);
	if (! i) {
		syslog(LOG_ERR, "__db_add: cannot encode object %s", name);
		return (NIS_SYSTEMERROR);
	}

	/* Now we cons up an entry for this object in the name space */
	entry.en_type			= "IN_DIRECTORY";
	entry.en_cols.en_cols_len	= 2;
	entry.en_cols.en_cols_val	= ecols;
	ecols[0].ec_flags		= EN_XDR + EN_MODIFIED;
	ecols[0].ENVAL			= (char *) buf;
	ecols[0].ENLEN			= xdr_getpos(&xdrs);
	ecols[1].ec_flags		= EN_MODIFIED;
	ecols[1].ENVAL			= nis_leaf_of(name);
	ecols[1].ENLEN			= strlen(ecols[1].ENVAL)+1;
	attr.zattr_ndx = "name";
	attr.ZAVAL = ecols[1].ENVAL;
	attr.ZALEN = ecols[1].ENLEN;
	dbres = db_add_entry(table, 1, &attr, &entry);

	if (dbres == NULL)
		res = NIS_NOMEMORY;
	else
		res = map_db_status_to_nis_status(dbres->status);
	switch (res) {
	    case NIS_NOMEMORY:
	    case NIS_NOFILESPACE:
		break;	/* these are expected */
	    case NIS_SUCCESS:
		if (modop) {
			/* Flush various cached objects */
			switch __type_of(obj) {
			    case NIS_TABLE_OBJ:
				flush_tablecache(name);
				break;
			    case NIS_DIRECTORY_OBJ:
				flush_dircache(name, (directory_obj *)NULL);
				break;
			    case NIS_GROUP_OBJ:
				flush_groupcache(name);
				break;
			    default:
				break;
			}
		}
		(void) mark_for_sync(table);
		break;
	    default:
		syslog(LOG_ERR, "__db_add: unexpected database result %d",
								dbres->status);
		break;
	}

	free_db_result(dbres);
	if (res == NIS_SUCCESS) {
		multival_invalidate(obj);
	}
	return (res);
}

/*
 * db_add()
 *
 * External wrapper for the real db_add function. This one creates a
 * transaction log entry. The internal one skips the log transaction.
 */
nis_db_result *
db_add(
	nis_name	name,
	nis_object	*obj,
	int		mod_op)
{
	static nis_db_result res;
	log_entry le;	/* A log entry */

	memset((char *)&res, 0, sizeof (res));
	memset((char *)&le, 0, sizeof (le));
	if (verbose) {
		if (mod_op == 0)
			syslog(LOG_INFO, "db_add: Adding object %s", name);
		else
			syslog(LOG_INFO, "db_add: Modifying object %s", name);
	}

	le.le_time = obj->zo_oid.mtime;
	if (mod_op)
		le.le_type = MOD_NAME_NEW;
	else
		le.le_type = ADD_NAME;
	le.le_name = name;
	le.le_object = *obj;
	add_update(&le);
	__start_clock(1);
	res.status = __db_add(name, obj, mod_op);
	res.ticks = __stop_clock(1);
	if (verbose || (res.status != NIS_SUCCESS))
		syslog(LOG_INFO, "db_add: returning %s",
			nis_sperrno(res.status));
	return (&res);
}

nis_error
__db_remove(
	nis_name	name,		/* Name of object to remove	*/
	nis_object	*obj)		/* Its type (for deleting tables) */
{
	nis_attr	attr;
	char		*table;
	db_result	*dbres;
	nis_error	res;
	char		tblbuf[NIS_MAXNAMELEN * 2];

	if (CHILDPROC) {
		syslog(LOG_INFO,
			"__db_remove: non-parent process attempting a remove.");
		return (NIS_TRYAGAIN);
	}

	if (__type_of(obj) == NIS_TABLE_OBJ) {
		table = internal_table_name(name, tblbuf);
		if (! table)
			return (NIS_BADNAME);

		/* First make sure the table is empty */
		dbres = db_first_entry(table, 0, NULL);
		if (dbres == 0 || dbres->status == DB_MEMORY_LIMIT) {
			free_db_result(dbres);
			return (NIS_NOMEMORY);
		} else if (dbres->status == DB_SUCCESS) {
			free(dbres->nextinfo.db_next_desc_val); /* cookie */
			free_db_result(dbres);
			return (NIS_NOTEMPTY);
		} else if (dbres->status == DB_NOTFOUND) {
			if ((res = db_destroy(name)) != NIS_SUCCESS)
			    syslog(LOG_WARNING,
				"__db_remove: Unable to destroy table %s: %s.",
				    table, nis_sperrno(res));
		} else {
			/*
			 * POLICY : What should we do, remove the object?
			 *	    or abort() because the database is
			 *	    inconsistent.
			 * ANSWER : Notify the system operator, and continue
			 *	    to remove the table object.
			 */
			syslog(LOG_ERR,
			    "__db_remove: table %s not in dictionary (err=%d)",
			    table, dbres->status);
			if ((res = db_destroy(name)) != NIS_SUCCESS)
			    syslog(LOG_WARNING,
				"__db_remove: Unable to destroy table %s: %s.",
				    table, nis_sperrno(res));
		}
		free_db_result(dbres);
	}

	table = internal_table_name(nis_domain_of(name), tblbuf);
	if (! table)
		return (NIS_BADNAME);

	attr.zattr_ndx = "name";
	attr.ZAVAL = nis_leaf_of(name);
	attr.ZALEN = strlen(attr.ZAVAL)+1;
	dbres = db_remove_entry(table, 1, &attr);

	if (dbres == NULL)
		res = NIS_NOMEMORY;
	else
		res = map_db_status_to_nis_status(dbres->status);
	switch (res) {
	    case NIS_NOMEMORY:
	    case NIS_NOFILESPACE:
		break;	/* these are expected */
	    case NIS_SUCCESS:
		/* Flush various cached objects */
		switch __type_of(obj) {
		    case NIS_TABLE_OBJ:
			flush_tablecache(name);
			break;
		    case NIS_DIRECTORY_OBJ:
			flush_dircache(name, (directory_obj *)NULL);
			break;
		    case NIS_GROUP_OBJ:
			flush_groupcache(name);
			break;
		    default:
			break;
		}
		(void) mark_for_sync(table);
		break;
	    default:
		syslog(LOG_ERR,
			"__db_remove: unexpected result from database %d",
								dbres->status);
		break;
	}

	free_db_result(dbres);
	if (res == NIS_SUCCESS) {
		multival_invalidate(obj);
	}
	return (res);
}

nis_db_result *
db_remove(
	nis_name	name,		/* Name of object to remove */
	nis_object	*obj,		/* Its type (for deleting tables) */
	u_long		xid_time)	/* Time of "this" transaction */
{
	static nis_db_result res;
	log_entry le;			/* A log entry */

	memset((char *)&res, 0, sizeof (res));
	memset((char *)&le, 0, sizeof (le));
	if (verbose)
		syslog(LOG_INFO, "db_remove: removing %s from the namespace",
									name);
	le.le_time = xid_time;
	le.le_type = REM_NAME;
	le.le_name = name;
	le.le_object = *obj;
	add_update(&le);
	__start_clock(1);
	res.status = __db_remove(name, obj);
	res.ticks = __stop_clock(1);
	if (verbose || (res.status != NIS_SUCCESS))
		syslog(LOG_INFO, "db_remove: returning %s",
						nis_sperrno(res.status));
	return (&res);
}


/*
 * db_list(table, numattrs, attrs)
 *
 * function to call the database list function. When called we know that
 * object is "readable" by the principal.
 *
 */

static nis_db_list_result *
db_list_x(
	nis_name	name,	/* Table we are listing 	*/
	int		na,	/* Number of attributes 	*/
	nis_attr	*a,	/* An array of attributes 	*/
	u_long		flags)
{
	int		i;
	nis_error	err;
	char		*table; /* internal table name */
	char		tblbuf[NIS_MAXNAMELEN * 2]; /* table path buf */
	db_result	*dbres;
	nis_db_list_result *res;
	int		got;
	int		nm = 0;
	nis_object	*o;
	nis_object	*tobj;
	nis_db_result	*tres;

	res = (nis_db_list_result *)XCALLOC(1, sizeof (nis_db_list_result));
	if ((flags & FN_NORAGS) == 0)
		add_cleanup(free_db_list, (void *)res, "db_list result");
	got = 0;
	table =  internal_table_name(name, tblbuf);
	if (! table) {
		res->status = NIS_BADNAME;
		return (res);
	}

	if (verbose)
		syslog(LOG_INFO, "db_list: listing %s", table);

	__start_clock(1);

	tres = db_lookup(name);
	if (tres == NULL) {
		res->status = NIS_NOMEMORY;
		res->numo = 0;
		res->objs = NULL;
		return (res);
	}
	if (tres->status == NIS_SUCCESS) {
		tobj = tres->obj;
	} else if (tres->status == NIS_NOSUCHTABLE) {
		/* this happens on top directory in server's database */
		tobj = NULL;
	} else {
		res->status = tres->status;
		res->numo = 0;
		res->objs = NULL;
		return (res);
	}

	/* check for multival searches */
	if (tobj) {
		err = multival_attr(tobj, a, &na, &nm);
		if (err != NIS_SUCCESS) {
			res->ticks = __stop_clock(1);
			res->status = err;
			return (res);
		}
	}

	/*
	 *  If we have regular attributes or if we have no attributes
	 *  at all (na == 0 && nm == 0), search the normal way.
	 */
	if (na || nm == 0) {
		dbres = db_list_entries(table, na, a);
		res->ticks = __stop_clock(1);
		/* Process the entries into "objects" */
		if (dbres == NULL)
			res->status = NIS_NOMEMORY;
		else
			res->status =
				map_db_status_to_nis_status(dbres->status);
		switch (res->status) {
		    case NIS_NOMEMORY:
			break;
		    case NIS_SUCCESS:
			/* Convert from database format to NIS+ object format */
			res->objs = cvt2object(name, dbres->objects.objects_val,
					dbres->objects.objects_len, &got, 1);
			if (got > 0) {
				res->numo = got;
				res->status = NIS_SUCCESS;
			} else {
				res->numo = 0;
				res->status = NIS_NOTFOUND;
				res->objs = NULL;
			}
			break;
		    case NIS_NOTFOUND:
			res->numo = 0;
			res->objs = NULL;
			break;
		    default:
			strcpy(tblbuf, "[");
			for (i = 0; i < na; i++) {
				if (i != 0)
					strcat(tblbuf, ",");
				strcat(tblbuf, a[i].zattr_ndx);
				strcat(tblbuf, "=");
				strncat(tblbuf,
					a[i].zattr_val.zattr_val_val,
					a[i].zattr_val.zattr_val_len);
			}
			strcat(tblbuf, "],");
			strcat(tblbuf, name);
			syslog(LOG_ERR,
				"Database search failed on %s, status = %d",
				tblbuf, dbres->status);
			break;
		}
		if (verbose)
			syslog(LOG_INFO,
				"db_list: returning status = %d, entries = %d",
				res->status, got);
		free_db_result(dbres);
	}

	/*
	 *  If we have multival attributes and regular attributes,
	 *  filter out the objects gotten above that don't match
	 *  on the multival columns.  If there were no regular
	 *  attributes, then list based only on the multival columns.
	 */
	if (nm) {
		if (na) {
			for (i = 0; i < res->numo; i++) {
				o = res->objs[i].o;
				if (multival_filter(tobj, nm, a + na, o)) {
					nis_destroy_object(o);
					res->objs[i].o = NULL;
				}
			}
		} else {
			/* na = 0 */
			multival_list(tobj, nm, a, res);
		}
	}

	return (res);
}

nis_db_list_result *
db_list(
	nis_name	name,	/* Table we are listing 	*/
	int		na,	/* Number of attributes 	*/
	nis_attr	*a)	/* An array of attributes 	*/
{
	return (db_list_x(name, na, a, 0));
}

nis_db_list_result *
db_list_flags(
	nis_name	name,	/* Table we are listing 	*/
	int		na,	/* Number of attributes 	*/
	nis_attr	*a,	/* An array of attributes 	*/
	u_long		flags)
{
	return (db_list_x(name, na, a, flags));
}

/*
 * This function has to release all of the components that were allocated
 * by db_list above.
 */
void
free_db_list(nis_db_list_result	*list)
{
	int	i;

	if (list == NULL)
		return;
	for (i = 0; i < list->numo; i++) {
		if (list->objs[i].o)
			nis_destroy_object(list->objs[i].o);
	}
	if ((list->objs) && (list->numo))
		XFREE(list->objs); /* Free the entire array */
	free(list);
}

/*
 * nis_fn_result *db_firstib(name)
 *
 * Get the "first" entry from a table. Note this function returns an opaque
 * "cookie" that has what ever state the underlying database needs to
 * find the next entry in a chain of entries. Since it is specific to the
 * underlying database it is opaque to this interface.
 */
nis_fn_result *
db_firstib(
	nis_name	name,	/* Table name		*/
	int		na,	/* Number of attributes */
	nis_attr	*a,	/* Attribute list	*/
	int		flags,	/* Mangle objects into entries */
	char		*table)
{
	db_result	*dbres;
	obj_list	*olist;
	int		got;
	char		tblbuf[NIS_MAXNAMELEN * 2];
	nis_fn_result	*res;

	res = (nis_fn_result *)XCALLOC(1, sizeof (nis_fn_result));
	if ((flags & FN_NORAGS) == 0)
		add_cleanup((void (*)())XFREE, (char *)res, "fn (first) res");
	if (! table)
		table = internal_table_name(name, tblbuf);
	if (! table) {
		res->status = NIS_BADNAME;
		return (res);
	}

	if (verbose)
		syslog(LOG_INFO, "db_firstib: Fetching first entry from %s",
							table);
	__start_clock(1);
	dbres = db_first_entry(table, na, a);
	res->ticks = __stop_clock(1);
	if (dbres == NULL)
		res->status = NIS_NOMEMORY;
	else
		res->status = map_db_status_to_nis_status(dbres->status);
	switch (res->status) {
	    case NIS_NOMEMORY:
	    case NIS_NOTFOUND:
		break;	/* expected */
	    case NIS_BADREQUEST:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
				"db_firstib: Table: '%s', Bad Attribute",
				name);
		break;
	    case NIS_NOSUCHTABLE:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
				"db_firstib: Missing table '%s'", name);
		break;
	    case NIS_SUCCESS:
		/* ASSERT(dbres->objects.objects_len == 1); */
		/* Convert the entry into a NIS+ object */
		olist = cvt2object(name, dbres->objects.objects_val, 1, &got,
						(FN_MANGLE & flags));
		if ((olist == NULL) || (got > 1)) {
			syslog(LOG_ERR, "db_firstib: Database is corrupt.");
			res->status = NIS_SYSTEMERROR;
		} else {
			res->obj = olist->o; /* Now that's nice and clear! */
			if ((flags & FN_NORAGS) == 0)
				add_cleanup(nis_destroy_object,
					(char *)res->obj, "firstib object.");
			XFREE(olist); /* free list struct but not obj in it */
		}
		/* Now clone the nextinfo cookie */
		res->cookie.n_len = dbres->nextinfo.db_next_desc_len;
		res->cookie.n_bytes = (char *) XMALLOC(res->cookie.n_len);
		memcpy(res->cookie.n_bytes, dbres->nextinfo.db_next_desc_val,
					    dbres->nextinfo.db_next_desc_len);
		free(dbres->nextinfo.db_next_desc_val);
		dbres->nextinfo.db_next_desc_val = 0;
		if (res->obj == NULL) {
			syslog(LOG_ERR, "db_firstib: Object not found.");
			res->status = NIS_SYSTEMERROR;
		}
		break;

	    default:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
				"db_firstib: Unexpected database result %d.",
								dbres->status);
		break;
	}
	/* Don't need this anymore */
	free_db_result(dbres);
	if (verbose)
		syslog(LOG_INFO, "db_firstib: returning status of %s",
						nis_sperrno(res->status));
	return (res); /* Return it finally */
}

/*
 * nis_fn_result *db_nextib(name, cookie)
 *
 * Get the "next" entry from a table.
 */
nis_fn_result *
db_nextib(
	nis_name	name,
	netobj		*cookie,
	int		flags,
	char		*table)
{
	nis_fn_result 	*res;
	db_result	*dbres;
	obj_list	*olist;
	int		got;
	char		tblbuf[NIS_MAXNAMELEN * 2];

	res = (nis_fn_result *)XCALLOC(1, sizeof (nis_fn_result));
	if ((flags & FN_NORAGS) == 0)
		add_cleanup((void (*)())XFREE, (char *) res, "fn (next) res");
	if (! table)
		table = internal_table_name(name, tblbuf);
	if (! table) {
		res->status = NIS_BADNAME;
		return (res);
	}
	if (verbose)
		syslog(LOG_INFO, "db_nextib: Fetching entry from %s", table);
	__start_clock(1);
	dbres = db_next_entry(table, (db_next_desc *)cookie);
	res->ticks = __stop_clock(1);
	if (dbres == NULL)
		res->status = NIS_NOMEMORY;
	else
		res->status = map_db_status_to_nis_status(dbres->status);
	switch (res->status) {
	    case NIS_NOMEMORY:
	    case NIS_NOTFOUND:
		break;
	    case NIS_BADREQUEST:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
		    "db_nextib: Bad Attribute in Table: '%s'",
						name);
		break;
	    case NIS_NOSUCHTABLE:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
		    "db_nextib: Missing table object '%s'",
							name);
		break;
	    case NIS_SUCCESS:
		/* ASSERT(dbres->objects.objects_len == 1); */
		olist = cvt2object(name, dbres->objects.objects_val, 1, &got,
						(flags & FN_MANGLE));
		if ((olist == NULL) || (got > 1)) {
			syslog(LOG_ERR, "db_nextib: Database is corrupt.");
			res->status = NIS_SYSTEMERROR;
		} else {
			res->obj = olist->o;
			if ((flags & FN_NORAGS) == 0)
				add_cleanup(nis_destroy_object,
					(char *)res->obj, "nextib object");
			XFREE(olist);
		}
		/* Now clone the nextinfo cookie */
		res->cookie.n_len = dbres->nextinfo.db_next_desc_len;
		res->cookie.n_bytes = (char *) XMALLOC(res->cookie.n_len);
		memcpy(res->cookie.n_bytes, dbres->nextinfo.db_next_desc_val,
					    dbres->nextinfo.db_next_desc_len);
		free(dbres->nextinfo.db_next_desc_val);
		if (res->obj == NULL)
			res->status = NIS_SYSTEMERROR;
		break;
	    default:
		if ((flags & FN_NOERROR) == 0)
			syslog(LOG_ERR,
		    "db_nextib: Unexpected database result %d",
						dbres->status);
		break;
	}
	/* Don't need this anymore */
	free_db_result(dbres);
	if (verbose)
		syslog(LOG_INFO, "db_nextib: returning status of %s",
						nis_sperrno(res->status));
	return (res); /* Return it finally */
}

/*
 * db_flush()
 *
 * Flush any pending "next" entries from a list returned by firstib.
 */
void
db_flush(
	nis_name	name,		/* table name */
	netobj		*cookie)	/* The next_desc */
{
	char	*table;
	db_result *dbres;
	char	tblbuf[NIS_MAXNAMELEN * 2];

	table = internal_table_name(name, tblbuf);
	if (! table)
		return;
	if (verbose)
		syslog(LOG_INFO, "db_flush: Flushing queued entries from %s",
								table);
	dbres = db_reset_next_entry(table, (db_next_desc *)cookie);
	if (dbres && dbres->status != DB_SUCCESS) {
		syslog(LOG_ERR, "Unable to flush '%s'", table);
	}
	free_db_result(dbres);
}

/*
 * Add an entry to a table, we assume we've already sanitized the
 * data.
 */
static nis_error
__db_addib_x(
	nis_name	name,		/* Name of the table. */
	int		numattr,	/* Number of attributes */
	nis_attr	*attrs,		/* array of attributes */
	nis_object	*obj,		/* Entry to add to the table */
	int		skiplog,	/* if true, don't log action */
	int		nosync)
{
	entry_obj	eo; 		/* our "fake" entry */
	entry_col	*oec, 		/* our objects columns  */
			*ec; 		/* our objects columns  */
	int		i, mc, bufsize;	/* counters		*/
	db_result	*dbres;
	XDR		xdrs;		/* XDR stream		*/
	u_char		*buf;
	char		*table;
	nis_error	res;
	struct table_item 	*te;	/* Table cache entry	*/
	struct nis_object	*tobj;	/* Table nis_object 	*/
	char		tblbuf[NIS_MAXNAMELEN * 2]; /* table path buf */

	/* Get the number of columns and the pointer to their data */
	mc = obj->EN_data.en_cols.en_cols_len;
	oec = obj->EN_data.en_cols.en_cols_val;

	ec = __get_entry_col(mc+1); /* get some temp storage */

	if (ec == NULL) {
		return (NIS_NOMEMORY);
	}

	table = internal_table_name(name, tblbuf);
	if (! table)
		return (NIS_BADNAME);
	if (verbose)
		syslog(LOG_INFO, "__db_addib: Adding an entry to table %s",
							table);

	te = (struct table_item *) nis_find_item(name, table_cache);
	if (te == NULL) {
		/* Do a db_lookup() so that cache is populated */
		nis_db_result *dbres;

		if (((dbres = db_lookup(name)) == NULL) ||
		    (dbres->status != NIS_SUCCESS)) {
			syslog(LOG_ERR,
			"__db_addib: could not find table object for %s",
				name);
			return (NIS_NOSUCHTABLE);	/* XXX: Error number */
		}
		tobj = dbres->obj;
		/* dbres is freed automatically during cleanup */
	} else
		tobj = te->ibobj;

	/* Build up our temporary entry object */
	eo.en_type = NULL; /* used by cvt2obj - denotes non IN_DIRECTORY */
	eo.en_cols.en_cols_len = mc + 1;
	eo.en_cols.en_cols_val = ec;

	/* Copy the entry value pointers, offset by 1 */
	for (i = 0; i < mc; i++)
		ec[i+1] = oec[i];

	/*
	 * To prevent the XDR function from making a copy of
	 * the entry columns, we set the columns structure to
	 * 0 (ie no column data)
	 */
	obj->EN_data.en_cols.en_cols_len  = 0;	  /* Neuter it	*/
	obj->EN_data.en_cols.en_cols_val  = NULL; /* Neuter it	*/

	/* Make a fetus object from a FULL object */
	bufsize = xdr_sizeof(xdr_nis_object, obj);
	buf = __get_xdr_buf(bufsize);
	if (buf == NULL) {
		syslog(LOG_ERR, "__db_addib: out of memory!");
		return (NIS_NOMEMORY);
	}

	xdrmem_create(&xdrs, (char *) buf, bufsize, XDR_ENCODE);
	if (! xdr_nis_fetus_object(&xdrs, obj, tobj)) {
		syslog(LOG_ERR, "__db_addib: Failure to encode entry.");
		return (NIS_SYSTEMERROR);
	}
	/* Un-neuter it so that it can be properly freed */
	obj->EN_data.en_cols.en_cols_len  = mc;
	obj->EN_data.en_cols.en_cols_val  = oec;
	ec[0].ENVAL    = (char *) buf;	/* Point to encoded one	*/
	ec[0].ENLEN    = xdr_getpos(&xdrs);
	ec[0].ec_flags = EN_BINARY+EN_XDR;

	if (skiplog)
		dbres = __db_add_entry_nolog(table, numattr, attrs, &eo);
	else if (nosync)
		dbres = __db_add_entry_nosync(table, numattr, attrs, &eo);
	else
		dbres = db_add_entry(table, numattr, attrs, &eo);

	if (dbres == NULL)
		res = NIS_NOMEMORY;
	else
		res = map_db_status_to_nis_status(dbres->status);
	switch (res) {
	    case NIS_NOMEMORY:
	    case NIS_NOFILESPACE:
	    case NIS_SUCCESS:
		(void) mark_for_sync(table);
		break;
	    default:
		syslog(LOG_ERR, "__db_addib: Unexpected database result %d.",
						dbres->status);
		break;
	}
	if (verbose)
		syslog(LOG_INFO, "__db_addib: done. (%d)", res);
	free_db_result(dbres);
	if (res == NIS_SUCCESS) {
		multival_invalidate(obj);
	}
	return (res);
}

nis_error
__db_addib(
	nis_name	name,		/* Name of the table. */
	int		numattr,	/* Number of attributes */
	nis_attr	*attrs,		/* array of attributes */
	nis_object	*obj)		/* Entry to add to the table */
{
	return (__db_addib_x(name, numattr, attrs, obj, 0, 0));
}

nis_error
__db_addib_nolog(
	nis_name	name,		/* Name of the table. */
	int		numattr,	/* Number of attributes */
	nis_attr	*attrs,		/* array of attributes */
	nis_object	*obj)		/* Entry to add to the table */
{
	return (__db_addib_x(name, numattr, attrs, obj, 1, 0));
}

nis_error
__db_addib_nosync(
	nis_name	name,		/* Name of the table. */
	int		numattr,	/* Number of attributes */
	nis_attr	*attrs,		/* array of attributes */
	nis_object	*obj)		/* Entry to add to the table */
{
	return (__db_addib_x(name, numattr, attrs, obj, 0, 1));
}

/*
 * Add an entry to a table, we assume we've already sanitized the
 * data.
 */
nis_db_result *
db_addib(
	nis_name	name,		/* Name of the table.	*/
	int		numattr,	/* Number of attributes	*/
	nis_attr	*attrs,		/* array of attributes	*/
	nis_object	*obj,		/* Entry to add to the table */
	nis_object	*tobj)		/* Table to add it too.	*/
{
	nis_attr	*attr_list;
	int		i, mc, na;
	table_col	*tc;
	entry_col	*ec;
	nis_db_result	*res;
	log_entry	le;

	/*
	 * First we create a fully specified entry list, with a
	 * set of attribute/values to go with it.
	 */
	res = (nis_db_result *)XCALLOC(1, sizeof (nis_db_result));
	add_cleanup((void (*)())XFREE, (char *)res, "db_addib result");
	memset((char *)&le, 0, sizeof (le));
	mc = tobj->TA_data.ta_cols.ta_cols_len;
	tc = tobj->TA_data.ta_cols.ta_cols_val;
	ec = obj->EN_data.en_cols.en_cols_val;
	attr_list = __get_attrs(mc);
	if (attr_list == NULL) {
		res->status = NIS_NOMEMORY;
		return (res);
	}
	for (i = 0, na = 0; i < mc; i++) {
		if ((tc[i].tc_flags & TA_SEARCHABLE) != 0) {
			attr_list[na].zattr_ndx = tc[i].tc_name;
			attr_list[na].ZAVAL = ec[i].ENVAL;
			attr_list[na].ZALEN = ec[i].ENLEN;
			na++;
		}
	}
	le.le_name = name;
	le.le_object = *obj;
	le.le_attrs.le_attrs_len = na;
	le.le_attrs.le_attrs_val = attr_list;
	le.le_type = ADD_IBASE;
	le.le_time = obj->zo_oid.mtime;
	add_update(&le);
	__start_clock(1);
	res->status = __db_addib(name, na, attr_list, obj);
	res->ticks = __stop_clock(1);
	if (res->status != NIS_SUCCESS)
		syslog(LOG_ERR, "db_addib: unable to add entry to %s", name);
	return (res);
}

nis_error
__db_remib_x(
	nis_name	name,
	int		nattr,
	nis_attr	*attrs,
	int		nosync)
{
	nis_error 	res;
	db_result	*dbres;
	char		*table;
	char		tblbuf[NIS_MAXNAMELEN * 2];

	if (CHILDPROC) {
		syslog(LOG_INFO,
			"__db_remib: non-parent process attempting an remove.");
		return (NIS_TRYAGAIN);
	}

	table = internal_table_name(name, tblbuf);
	if (! table)
		return (NIS_BADNAME);

	if (verbose)
		syslog(LOG_INFO,
			"__db_remib: Removing an entry from table %s", table);

	if (nosync)
		dbres = __db_remove_entry_nosync(table, nattr, attrs);
	else
		dbres = db_remove_entry(table, nattr, attrs);

	if (dbres == NULL)
		res = NIS_NOMEMORY;
	else
		res = map_db_status_to_nis_status(dbres->status);
	switch (res) {
	case NIS_NOMEMORY:
	case NIS_NOFILESPACE:
	case NIS_SUCCESS:
		(void) mark_for_sync(table);
		break;
	case NIS_NOTFOUND:
		/* this occurs when applying log updates */
		res = NIS_SUCCESS;
		break;
	default:
		syslog(LOG_ERR, "__db_remib: Unexpected database result %d.",
								dbres->status);
		break;
	}
	if (verbose)
		syslog(LOG_INFO, "__db_remib: done. (%d)", res);
	free_db_result(dbres);
	if (res == NIS_SUCCESS) {
		nis_db_result *tres;

		tres = db_lookup(name);
		multival_invalidate(tres->obj);
	}
	return (res);
}

nis_error
__db_remib(
	nis_name	name,
	int		nattr,
	nis_attr	*attrs)
{
	return (__db_remib_x(name, nattr, attrs, 0));
}

nis_error
__db_remib_nosync(
	nis_name	name,
	int		nattr,
	nis_attr	*attrs)
{
	return (__db_remib_x(name, nattr, attrs, 1));
}

/*
 * The remove operation. Since it can remove multiple entries we pass it
 * the list for the log entries.
 */
nis_db_result *
db_remib(
	nis_name	name,
	int		nattrs,
	nis_attr	*attrs,
	obj_list	*olist,
	int		numo,
	nis_object	*tobj,
	u_long		xid_time)
{
	nis_attr	*attr_list;
	int		i, j, mc, na;
	table_col	*tc;
	entry_col	*ec;
	nis_db_result	*res;
	log_entry	le;
	int		*amap;

	/*
	 * First we create a fully specified entry list, with a
	 * set of attribute/values to go with it.
	 */
	res = (nis_db_result *)XCALLOC(1, sizeof (nis_db_result));
	add_cleanup((void (*)())XFREE, (char *) res, "db_remib result");
	memset((char *)&le, 0, sizeof (le));
	mc = tobj->TA_data.ta_cols.ta_cols_len;
	tc = tobj->TA_data.ta_cols.ta_cols_val;
	na = 0; /* Actual attrs */
	attr_list = __get_attrs(mc);
	/* Cheat and allocate a "static" array */
	amap = (int *)__get_xdr_buf(sizeof (int) * mc);

	if (attr_list == NULL) {
		res->status = NIS_NOMEMORY;
		return (res);
	}
	for (i = 0, na = 0; i < mc; i++) {
		if ((tc[i].tc_flags & TA_SEARCHABLE) != 0) {
			attr_list[na].zattr_ndx = tc[i].tc_name;
			amap[na] = i;
			na++;
		}
	}

	/* Add log updates for all of the entries that will be removed */
	for (i = 0; i < numo; i++) {
		ec = olist[i].o->EN_data.en_cols.en_cols_val;
		for (j = 0; j < na; j++) {
			attr_list[j].ZAVAL = ec[amap[j]].ENVAL;
			attr_list[j].ZALEN = ec[amap[j]].ENLEN;
		}
		le.le_name = name;
		le.le_object = *(olist[i].o);
		le.le_attrs.le_attrs_len = na;
		le.le_attrs.le_attrs_val = attr_list;
		le.le_type = REM_IBASE;
		le.le_time = xid_time;
		add_update(&le);
	}
	__start_clock(1);
	res->status = __db_remib(name, nattrs, attrs);
	res->ticks = __stop_clock(1);
	if (res->status != NIS_SUCCESS)
		syslog(LOG_ERR, "db_remib: unable to remove entry from %s",
			name);
	return (res);
}


/*
 * __create_table()
 *
 * Create the underlying database table that supports the current database.
 */
static nis_error
__create_table(
	char		*table,
	nis_object	*obj)
{
	table_obj	fake, *t;	/* Fake table object here. */
	table_col	*tc = NULL; 	/* Fake columns data */
	int		i;

	t = &(obj->TA_data);
	tc = __get_table_col(t->ta_cols.ta_cols_len+1);
	if (tc == NULL)
		return (NIS_NOMEMORY);

	for (i = 0; i < t->ta_cols.ta_cols_len; i++) {
		if (t->ta_cols.ta_cols_val[i].tc_flags & TA_SEARCHABLE)
			break;
	}
	if (i == t->ta_cols.ta_cols_len) {
		if (verbose)
			syslog(LOG_INFO,
		"Cannot create table: %s: no searchable columns.", table);
		return (NIS_BADOBJECT);
	}

	/* Copy the important parts of the table structure over */
	fake = obj->TA_data;
	/* Now shift the columns right by 1 */
	for (i = 0; i < fake.ta_cols.ta_cols_len; i++) {
		tc[i+1] = fake.ta_cols.ta_cols_val[i];
	}
	tc[0].tc_name   = NULL; /* NO name for the entry column */
	tc[0].tc_flags  = TA_XDR+TA_BINARY;
	tc[0].tc_rights = 0; /* only the server sees them */
	fake.ta_cols.ta_cols_len++; /* Add one to this */
	fake.ta_cols.ta_cols_val = tc; /* fixup this */
	return (db_create(table, &fake));
}

/*
 * do_checkpoint
 *
 * This function checkpoints the table named, or all tables
 * that the server has if no table is named.
 *
 * NB: This can be time consuming so the server should fork
 *	a readonly child to handle requests while we're out here.
 *
 * This function returns NIS_SUCCESS if all tables were
 * checkpointed and return NIS_PARTIAL otherwise.
 */
static cp_result *
do_checkpoint(
	nis_name	name)	/* name of directory to checkpoint */
{
	int			status;
	cp_result		*res;
	char			*table;
	static cp_result mem_err = {NIS_NOMEMORY, 0, 0};
	char			tblbuf[NIS_MAXNAMELEN * 2];

	res = (cp_result *) XCALLOC(1, sizeof (cp_result));
	if (! res)
		return (&mem_err);
	add_cleanup((void (*)())XFREE, (char *) res, "do_chkpt result");

	if (CHILDPROC) {
		syslog(LOG_ERR, "Child process requested to checkpoint!");
		res->cp_status = NIS_NOT_ME;
		return (res);
	}
	if (name == NULL)
		table = NULL;
	else
		table = internal_table_name(name, tblbuf);
	if (verbose)
		syslog(LOG_INFO, "Checkpointing table '%s'",
			table ? table : "(all)");

	__start_clock(1);
	status = db_checkpoint(table);
	res->cp_dticks = __stop_clock(1);
	if (status != DB_SUCCESS) {
		syslog(LOG_ERR,
		"db_checkpoint: Unable to checkpoint table '%s' because %s",
			(table ? table : "(all)"),
			nis_sperrno(map_db_status_to_nis_status(status)));
		res->cp_status = NIS_PARTIAL;
	} else
		res->cp_status = NIS_SUCCESS;

	return (res);
}

/*
 * do_checkpoint_dir
 *
 * This function checkpoints all of the tables in a named directory
 * and the directory itself.
 * This is accomplished by iterating over the directory and then
 * checkpointing anything that looks like a table.
 *
 * NB: This can be time consuming so the server should have forked
 *	a read only child to handle requests while we're out here.
 *
 * This function returns returns NIS_SUCCESS if all tables were
 * checkpointed and returne NIS_PARTIAL if only some of them were.
 */
static cp_result *
do_checkpoint_dir(
	nis_name	name)	/* name of directory to checkpoint */
{
	int			status, err;
	cp_result		*res;
	char			*table;
	char			tblname[NIS_MAXNAMELEN];
	netobj			cookie;
	nis_fn_result		*fnr;
	static cp_result mem_err = {NIS_NOMEMORY, 0, 0};
	char			tblbuf[NIS_MAXNAMELEN * 2];

	if (! name)
		return (NULL); /* name is required */

	res = (cp_result *) XCALLOC(1, sizeof (cp_result));
	if (! res)
		return (&mem_err);

	add_cleanup((void (*)())XFREE, (char *) res, "do_chkpt result");

	if (verbose)
		syslog(LOG_INFO, "Checkpointing directory '%s'", name);

	err = 0;
	fnr = db_firstib(name, 0, NULL, FN_NORAGS, NULL);
	cookie = fnr->cookie;
	res->cp_dticks = fnr->ticks;
	/* First, do directory itself. */
	if (fnr->status == NIS_NOTFOUND || fnr->status == NIS_SUCCESS) {
		table = internal_table_name(name, tblbuf);
		__start_clock(1);
		if (verbose)
			syslog(LOG_INFO, "Checkpointing directory '%s'",
				table);
		status = db_checkpoint(table);
		res->cp_dticks += __stop_clock(1);
		if (status != DB_SUCCESS) {
			syslog(LOG_ERR,
			"db_checkpoint: Unable to checkpoint '%s' because %s",
			(name ? name : "(null)"),
			nis_sperrno(map_db_status_to_nis_status(status)));
			err++;
		}
	}

	/* Do each table or directory within directory. */
	while (fnr->status == NIS_SUCCESS) {
		if (__type_of(fnr->obj) == NIS_TABLE_OBJ ||
		    __type_of(fnr->obj) == NIS_DIRECTORY_OBJ) {
			sprintf(tblname, "%s.%s", fnr->obj->zo_name,
						fnr->obj->zo_domain);
			table = internal_table_name(tblname, tblbuf);
			__start_clock(1);
			if (verbose)
				syslog(LOG_INFO, "Checkpointing table '%s'",
					table);
			status = db_checkpoint(table);
			res->cp_dticks += __stop_clock(1);
			/*
			 * We ignore DB_BADTABLE errors because we might
			 * not serve the contents of the directory.
			 */
			if (status != DB_SUCCESS && status != DB_BADTABLE) {
				syslog(LOG_ERR,
			"db_checkpoint: Unable to checkpoint '%s' because %s",
			(tblname ? tblname : "(null)"),
			nis_sperrno(map_db_status_to_nis_status(status)));
				err++;
			}
		}
		nis_destroy_object(fnr->obj);
		XFREE(fnr);
		/* note the call to nextib frees the old cookie! */
		fnr = db_nextib(name, &cookie, FN_NORAGS, NULL);
		cookie = fnr->cookie;
	}
	XFREE(fnr);
	if (err)
		res->cp_status = NIS_PARTIAL;
	else
		res->cp_status = NIS_SUCCESS;
	return (res);
}

/*
 * nis_checkpoint_svc maintains a list of directories to be
 * checkpointed, call do_checkpoint_dir on items on that list.
 * Otherwise, call do_checkpoint(NULL) to checkpoint entire database.
 */

int
checkpoint_db()
{
	ping_item	*cp, *nxt;
	cp_result	*res;

	if (CHILDPROC) {
		syslog(LOG_ERR, "Child process requested to checkpoint!");
		return (0);
	}

	cp = (ping_item *)(checkpoint_list.first);

	/* No directories specified; checkpoint entire database. */

	if (cp == NULL || checkpoint_all) {
		res = do_checkpoint(NULL);
		clear_checkpoint_list();
		checkpoint_all = 0;
		/* res is on cleanup list; no need to free. */
		return (1);
	}

	for (cp; cp; cp = nxt) {
		nxt = (ping_item *)(cp->item.nxt_item);
		res = do_checkpoint_dir(cp->item.name);
		if (res->cp_status != NIS_SUCCESS) {
			syslog(LOG_WARNING,
		"checkpoint_db: unable to completely checkpoint %s because %s",
				(cp->item.name ? cp->item.name : "(null)"),
				nis_sperrno(res->cp_status));
		}
		/* ignore status.  Can try checkpoint later if not complete. */
		(void) nis_remove_item(cp->item.name, &checkpoint_list);
		XFREE(cp->item.name);
		if (cp->obj)
			nis_destroy_object(cp->obj);
		XFREE(cp);
	}
	return (1);
}

int
checkpoint_table(char *name)
{
	char *table;
	db_status status;
	char tblbuf[NIS_MAXNAMELEN * 2];

	table = internal_table_name(name, tblbuf);
	status = db_checkpoint(table);
	if (status != DB_SUCCESS) {
		syslog(LOG_ERR,
		"checkpoint_table: Unable to checkpoint table %s because %s",
			(table ? table : "(null)"),
			nis_sperrno(map_db_status_to_nis_status(status)));
		return (0);
	}
	return (1);
}

/* Remove all items from checkpoint list. */
int
clear_checkpoint_list()
{
	ping_item	*cp, *nxt;

	for (cp = (ping_item *)(checkpoint_list.first); cp; cp = nxt) {
		nxt = (ping_item *)(cp->item.nxt_item);
		(void) nis_remove_item(cp->item.name, &checkpoint_list);
		XFREE(cp->item.name);
		if (cp->obj)
			nis_destroy_object(cp->obj);
		XFREE(cp);
	}
	return (1);
}

/*
 * Returns NIS_SUCCESS if table exists; NIS_NOSUCHTABLE if it does not exist.
 * Otherwise, return error returned by database.
 */
nis_error
db_find_table(nis_name name)
{
	char tblbuf[NIS_MAXNAMELEN * 2];
	char *table = internal_table_name(name, tblbuf);
	db_status dstatus;

	if (!table) {
		return (NIS_BADNAME);
	}

	dstatus = db_table_exists(table);
	return (map_db_status_to_nis_status(dstatus));
}


nis_error
db_create(nis_name name, table_obj *obj)
{
	char tblbuf[NIS_MAXNAMELEN * 2];
	char *table = internal_table_name(name, tblbuf);
	db_status dstatus;

	if (! table) {
		return (NIS_BADNAME);
	}

	dstatus = db_create_table(table, obj);
	if (dstatus == DB_SUCCESS)
		(void) mark_for_sync(table);
	return (map_db_status_to_nis_status(dstatus));
}

nis_error
db_destroy(nis_name name)
{
	char tblbuf[NIS_MAXNAMELEN * 2];
	char *table = internal_table_name(name, tblbuf);
	db_status dstatus;

	if (! table) {
		return (NIS_BADNAME);
	}

	dstatus = db_destroy_table(table);
	if (dstatus == DB_SUCCESS)
		(void) mark_for_sync(table);
	return (map_db_status_to_nis_status(dstatus));
}

static int
mark_for_sync(char *table) {

	table_list_entry_t	*te;
	int			ret = 0;

	for (te = table_list; te != 0; te = te->next) {
		if (strcmp(te->table, table) == 0)
			break;
	}
	if (te == 0 && (te = malloc(sizeof (table_list_entry_t)+
			strlen(table)+1)) != 0) {
		te->table = (char *)((u_long)te + sizeof (*te));
		(void) strcpy(te->table, table);
		te->next = table_list;
		table_list = te;
	} else if (te == 0) {
		syslog(LOG_WARNING,
		"mark_for_sync: could not add sync entry for \"%s\"\n", table);
		ret = ENOMEM;
	}

	return (ret);
}

int
nis_db_sync_log(void) {

	table_list_entry_t	*te;
	int			ret = 0;
	db_status		stat;

	while ((te = table_list) != 0) {
		if ((stat = db_sync_log(te->table)) != DB_SUCCESS) {
			if (verbose)
				syslog(LOG_INFO,
				"nis_db_sync_log: error %d syncing \"%s\"\n",
				stat, te->table);
			ret++;
		}
		table_list = te->next;
		free(te);
	}

	return (ret);
}
