/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)nisplus_dd.c	1.27	99/03/30 SMI"

/*
 * This module provides the primitive routines for manipulating NIS+ tables.
 */

#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/systeminfo.h>
#include <rpcsvc/nis.h>
#include <dd_impl.h>

#define	CASE_INSENSITIVE(obj, col) \
	((obj)->TA_data.ta_cols.ta_cols_val[col].tc_flags & TA_CASE)

#define	COLUMN_NAME(obj, col) ((obj)->TA_data.ta_cols.ta_cols_val[col].tc_name)

/* ARGSUSED */
static void
set_tbl_nis_error(nis_error err, int *tbl_err)
{

	if (tbl_err != NULL)
#ifdef	NEW_DEBUG_ARCH_XXXX
		*tbl_err = TBL_NISPLUS_ERROR + err;
#else
		switch (err) {
		case NIS_SUCCESS:
			/* FALLTHRU */
		case NIS_S_SUCCESS:
			*tbl_err = TBL_SUCCESS;
			break;
		case NIS_NOTFOUND:
			/* FALLTHRU */
		case NIS_S_NOTFOUND:
			*tbl_err = TBL_NO_ENTRY;
			break;
		case NIS_PERMISSION:
			/* FALLTHRU */
		case NIS_NOTOWNER:
			*tbl_err = TBL_NO_ACCESS;
			break;
		case NIS_NOSUCHTABLE:
			*tbl_err = TBL_NO_TABLE;
			break;
		case NIS_NAMEEXISTS:
			*tbl_err = TBL_ENTRY_EXISTS;
			break;
		case NIS_NAMEUNREACHABLE:
			*tbl_err = TBL_BAD_DOMAIN;
			break;
		case NIS_NOMEMORY:
			*tbl_err = TBL_NO_MEMORY;
			break;
		case NIS_UNAVAIL:
			/* FALLTHRU */
		case NIS_TRYAGAIN:
			*tbl_err = TBL_IS_BUSY;
			break;
		case NIS_MODFAIL:
			*tbl_err = TBL_WRITE_ERROR;
			break;
		case NIS_SYSTEMERROR:
			/* FALLTHRU */
		default:
			*tbl_err = TBL_NISPLUS_ERROR;
			break;
		}
#endif	/* NEW_DEBUG_ARCH_XXXX */
}

/*
 * Function to construct column search attributes for NIS+.  They are of the
 * form [colname=colval,colname=colval...]
 */
static void
add_srch_attr(char *buf, char *colname, char *colval)
{
	int len;

	if ((len = strlen(buf)) == 0)
		(void) strcpy(buf, "[");
	else
		buf[(len - 1)] = ',';
	(void) strcat(buf, colname);
	(void) strcat(buf, "=");
	(void) strcat(buf, colval);
	(void) strcat(buf, "]");
}

/*
 * Function construct a NIS+ indexed name from the column search args, table
 * name and domain.  It looks like
 * [colname=colval,colname=colval...]table.domain.
 */
static void
build_nis_name(char *search, char *table, char *domain, char *buff)
{
	char *tdom;

	buff[0] = '\0';
	if ((search != NULL) && (strlen(search) != 0)) {
		(void) strcat(buff, search);
		(void) strcat(buff, ",");
	}
	(void) strcat(buff, table);
	if ((domain != NULL) && (strlen(domain) != 0))
		tdom = domain;
	else
		tdom = nis_local_directory();
	if (tdom != (char *)NULL) {
		(void) strcat(buff, ".");
		if (strcmp(tdom, "."))
			(void) strcat(buff, tdom);
	}
}

/*
 * Function to piece together the complete table name.  If the name passed
 * contains a '.' assume that it's meant to force us into a very specific
 * location, so just spit it back.
 */
static void
build_tbl_name(char *name, char *prefix, char *suffix, char *buf)
{
	if (strchr(name, '.') == NULL)
		(void) sprintf(buf, "%s%s%s", prefix, name, suffix);
	else
		(void) strcpy(buf, name);
}

/*
 * Function to construct a NIS+ user principal name from username and domain.
 * Must be user with a valid DES cred table entry in the specified domain.
 * The "root" users should already be mapped to a host & domain name.
 * Returns an error status code: Invalid user or no cred entry.
 *
 * !!!WARNING!!! The buff parameter must be a buffer NIS_MAXNAMELEN+1 big.
 */
static int
do_nis_user_name(
	char *user,		/* Unqualified user name */
	char *domain,		/* Domain user defined in */
	char *buff)		/* Returned user NIS name */
{
	char  tbuf[NIS_MAXNAMELEN+1];
	char *tdom, *tp;
	nis_result *tres;
	nis_error tstat;
	int   len;

	/* Move user name into NIS name buffer.  Handle special cases */
	buff[0] = '\0';
	if (user == (char *)NULL)
		return (TBL_NO_USER);
	if (! (strcmp(user, "root"))) {
		(void) sysinfo((int)SI_HOSTNAME, buff,
		    (long)(MAXHOSTNAMELEN+1));
		if ((tp = strchr(buff, '.')) != (char *)NULL)
			*tp = '\0';
	} else
		(void) strcpy(buff, user);

	/* Add domain name and trailing period to NIS name buffer */
	if ((domain != NULL) && (strlen(domain) != 0))
		tdom = domain;
	else
		tdom = nis_local_directory();
	if (tdom != (char *)NULL) {
		(void) strcat(buff, ".");
		(void) strcat(buff, tdom);
		len = (int)strlen(buff) - 1;
		if (buff[len] != '.')
			(void) strcat(buff, ".");
	}

	/* Look up the NIS name in the specified domain's cred table */
	if ((tdom = strchr(buff, '.')) == (char *)NULL)
		tdom = "";
	(void) sprintf(tbuf, "[cname=%s,auth_type=DES],cred.org_dir%s",
	    buff, tdom);
	tres = nis_list(tbuf, FOLLOW_LINKS, NULL, NULL);
	tstat = tres->status;
	nis_freeresult(tres);
	if ((tstat != NIS_SUCCESS) && (tstat != NIS_S_SUCCESS))
		return (TBL_NO_CRED);

	/* Valid NIS+ user */
	return (0);
}

/*
 * Function to construct a NIS+ group name from groupname and domain.
 * Must be a valid NIS+ group in the specified domain.
 *
 * !!!WARNING!!! The buff parameter must be a buffer NIS_MAXNAMELEN+1 big.
 */
static int
do_nis_group_name(
	char *group,		/* Unqualified group name */
	char *domain,		/* Domain group is defined in */
	char *buff)		/* Returned group NIS name */
{
	char  tbuf[NIS_MAXNAMELEN+1];
	char *tdom;
	nis_result *tres;
	nis_error tstat;
	int   len;

	/* Copy group name into NIS group name buffer */
	buff[0] = '\0';
	if (group == (char *)NULL)
		return (-1);
	(void) strcpy(tbuf, group);
	(void) strcat(tbuf, ".groups_dir");

	/* Add domain name and trailing dot to NIS group name buffer */
	if ((domain != NULL) && (strlen(domain) != 0))
		tdom = domain;
	else
		tdom = nis_local_directory();
	if (tdom != (char *)NULL) {
		(void) strcat(tbuf, ".");
		(void) strcat(tbuf, tdom);
		len = (int)strlen(tbuf) - 1;
		if (tbuf[len] != '.')
			(void) strcat(tbuf, ".");
	}

	/* Look up group name in the specified domain */
	tres = nis_lookup(tbuf, FOLLOW_LINKS);
	tstat = tres->status;
	nis_freeresult(tres);
	if ((tstat != NIS_SUCCESS) && (tstat != NIS_S_SUCCESS))
		return (-1);

	/* Valid NIS+ group. Build NIS name without "groups_dir" token */
	(void) strcpy(buff, group);
	if (tdom != (char *)NULL) {
		(void) strcat(buff, ".");
		(void) strcat(buff, tdom);
		len = (int)strlen(buff) - 1;
		if (buff[len] != '.')
			(void) strcat(buff, ".");
	}
	return (0);
}

/*
 * Function to lookup any fully qualified NIS name in the namespace.
 */
static int
do_nis_lookup(char *name, nis_result **resp, int *tbl_err)
{
	*resp = nis_lookup(name, FOLLOW_LINKS);
	if (NIS_RES_STATUS(*resp) != NIS_SUCCESS &&
	    NIS_RES_STATUS(*resp) != NIS_S_SUCCESS) {
		set_tbl_nis_error(NIS_RES_STATUS(*resp), tbl_err);
		return (-1);
	}
	return (0);
}

/*
 * Comparison function for qsort(), normal NIS+ table formats, where first
 * column is primary key.
 */
int
_dd_compare_nisplus_col0(nis_object *aobj, nis_object *bobj)
{
	if (ENTRY_VAL(aobj, 0) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 0) == NULL)
		return (1);
	return (strcmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(bobj, 0)));
}

/*
 * Comparison function for qsort(), normal NIS+ table formats, where first
 * column is primary key, and case-insensitive.
 */
int
_dd_compare_nisplus_col0_ci(nis_object *aobj, nis_object *bobj)
{
	if (ENTRY_VAL(aobj, 0) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 0) == NULL)
		return (1);
	return (strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(bobj, 0)));
}

/*
 * Comparison function for qsort(), where second column is primary key,
 * and case-insensitive.
 */
int
_dd_compare_nisplus_col1_ci(nis_object *aobj, nis_object *bobj)
{
	if (ENTRY_VAL(aobj, 1) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 1) == NULL)
		return (1);
	return (strcasecmp(ENTRY_VAL(aobj, 1), ENTRY_VAL(bobj, 1)));
}

/*
 * Comparison function for qsort(), where third column is primary key.
 */
int
_dd_compare_nisplus_col2(nis_object *aobj, nis_object *bobj)
{
	if (ENTRY_VAL(aobj, 2) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 2) == NULL)
		return (1);
	return (strcmp(ENTRY_VAL(aobj, 2), ENTRY_VAL(bobj, 2)));
}

/*
 * Comparison function for qsort(), where third column is primary key,
 * and case-insensitive.
 */
int
_dd_compare_nisplus_col2_ci(nis_object *aobj, nis_object *bobj)
{
	if (ENTRY_VAL(aobj, 2) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 2) == NULL)
		return (1);
	return (strcasecmp(ENTRY_VAL(aobj, 2), ENTRY_VAL(bobj, 2)));
}

/*
 * Comparison function for qsort().  This function ensures that the primary
 * entry (the entry whose name == cname) is always before alias entries, which
 * allows optimization in the coalesce loop in _list_dd_nisplus.
 */

int
_dd_compare_nisplus_aliased(nis_object *aobj, nis_object *bobj)
{
	int s;

	if (ENTRY_VAL(aobj, 0) == NULL)
		return (-1);
	else if (ENTRY_VAL(bobj, 0) == NULL)
		return (1);
	s = strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(bobj, 0));
	if (s == 0) {
		if (!strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(aobj, 1)))
			return (-1);
		else if (!strcasecmp(ENTRY_VAL(bobj, 0), ENTRY_VAL(bobj, 1)))
			return (1);
		else
			return (strcasecmp(ENTRY_VAL(aobj, 1),
			    ENTRY_VAL(bobj, 1)));
	}
	return (s);
}

/*
 * Comparison function for qsort().  This function is a modified version of
 * _dd_compare_nisplus_aliased to deal with the fact that the "key" for
 * the NIS+ services table is really a combination of 3 columns.
 */

int
_dd_compare_nisplus_services(nis_object *aobj, nis_object *bobj)
{
	int s;

	if ((s = strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(bobj, 0))) != 0)
		return (s);
	if ((s = strcasecmp(ENTRY_VAL(aobj, 2), ENTRY_VAL(bobj, 2))) != 0)
		return (s);
	if ((s = strcasecmp(ENTRY_VAL(aobj, 3), ENTRY_VAL(bobj, 3))) != 0)
		return (s);
	if (!strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(aobj, 1)))
		return (-1);
	else if (!strcasecmp(ENTRY_VAL(bobj, 0), ENTRY_VAL(bobj, 1)))
		return (1);
	return (0);
}

/*
 * Comparison function for qsort() for the dhcptab.  This special function
 * ensures that all the symbol entries appear before the macro entries.
 */

int
_dd_compare_nisplus_dhcptab(nis_object *aobj, nis_object *bobj)
{
	register char a, b;
	int as, bs;

	if ((ENTRY_VAL(aobj, 0) == NULL) || (ENTRY_VAL(aobj, 1) == NULL))
		return (-1);
	else if ((ENTRY_VAL(bobj, 0) == NULL) || (ENTRY_VAL(bobj, 1) == NULL))
		return (1);

	a = *(char *)ENTRY_VAL(aobj, 1);
	b = *(char *)ENTRY_VAL(bobj, 1);

	as = (a == DT_DHCP_SYMBOL);
	bs = (b == DT_DHCP_SYMBOL);

	if (as == bs) {
		return (strcasecmp(ENTRY_VAL(aobj, 0), ENTRY_VAL(bobj, 0)));
	} else if (!as) {
		return (1);
	} else if (!bs) {
		return (-1);
	}
	return (0);
}

/*
 * Function to list the contents of a NIS+ table. Caller is responsible for
 * managing Tbl. Nothing is added to the Tbl if a failure occurs, nor
 * is anything deleted.
 */
int
_list_dd_nisplus(char *name, char *domain, int *tbl_err, Tbl *tbl,
    struct tbl_trans_data *ttp, char **args)
{
	int match_flag = 0;
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	nis_object *eobj, *tobj, *nobj;
	nis_result *tres, *eres;
	int cn, aa, alias_search = 0;
	int i, j, k;
	Row *rp;
	int (*null_fun)() = NULL;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name(NULL, zbuf, domain, zname);
	if (do_nis_lookup(zname, &tres, tbl_err) != 0)
		return (TBL_FAILURE);
	tobj = NIS_RES_OBJECT(tres);
	zbuf[0] = '\0';

	/*
	 * Set up alias search first.  We allow lookups of individual
	 * entries by either their canonical name or an alias if the
	 * table has searchable aliases.  We have to do one lookup on
	 * the alias to get the canonical name, then another on the
	 * canonical name to get all the aliases since each consumes
	 * a separate NIS+ object.
	 */
	aa = tfp->cfmts[tfp->alias_col].m_argno;
	if ((tfp->alias_col >= 0) &&
	    (tfp->cfmts[tfp->alias_col].flags & COL_KEY) &&
	    (args[aa] != NULL) && strlen(args[aa])) {
		++match_flag;
		alias_search = 1;
		add_srch_attr(zbuf, COLUMN_NAME(tobj, tfp->alias_col),
		    args[aa]);
	}

	/*
	 * Handle other search criteria which may have been specified.
	 */
	for (cn = 0; cn < tfp->cols; ++cn) {
		if ((i = tfp->cfmts[cn].m_argno) == -1)
			continue;
		if (i == aa)
			continue;
		if ((tfp->cfmts[cn].flags & COL_KEY) &&
		    (args[i] != NULL) && strlen(args[i])) {
			++match_flag;
			add_srch_attr(zbuf, COLUMN_NAME(tobj, cn), args[i]);
		}
	}
	/*
	 * Construct the name for the table that we found.
	 */
	for (;;) {
		build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain, zname);
#ifdef PERF_DEBUG
timestamp("Started nis_list", "", TS_NOELAPSED);
#endif
		/*
		 * Try to hide the fact that replica updates take a while so
		 * users can get to entries right away without bogus errors
		 * in the interim. Strategy is to try to let things happen
		 * normally, but if the lookup fails and we're trying to
		 * find a specific entry, no harm in trying again with a
		 * forced lookup to the master, since they'd otherwise
		 * just be getting a big ol' error message.
		 */
		eres = nis_list(zname, FOLLOW_LINKS, null_fun, (void *) NULL);
		if ((NIS_RES_STATUS(eres) != NIS_NOTFOUND) &&
		    (NIS_RES_STATUS(eres) != NIS_SUCCESS) &&
		    (NIS_RES_STATUS(eres) != NIS_S_SUCCESS)) {
			set_tbl_nis_error(NIS_RES_STATUS(eres), tbl_err);
			nis_freeresult(eres);
			nis_freeresult(tres);
			return (TBL_FAILURE);
		} else if ((NIS_RES_STATUS(eres) == NIS_NOTFOUND) &&
		    match_flag) {
			nis_freeresult(eres);
			eres = nis_list(zname, FOLLOW_LINKS|MASTER_ONLY,
			    null_fun, (void *)NULL);
			if ((NIS_RES_STATUS(eres) != NIS_SUCCESS) &&
			    (NIS_RES_STATUS(eres) != NIS_S_SUCCESS)) {
				set_tbl_nis_error(NIS_RES_STATUS(eres),
				    tbl_err);
				nis_freeresult(eres);
				nis_freeresult(tres);
				return (TBL_FAILURE);
			}
		}
#ifdef PERF_DEBUG
timestamp("Completed nis_list", "", TS_NOELAPSED);
#endif
		/*
		 * If we're doing a match list & the first search was on an
		 * alias, now search on the real thing.  Alias column is
		 * excluded as we want to search on canonical keys to get
		 * all the aliases.
		 */
		if (match_flag && alias_search) {
			zbuf[0] = '\0';
			for (i = 0; i < tfp->cols; ++i) {
				if ((tfp->cfmts[i].flags & COL_KEY) &&
				    (i != tfp->alias_col)) {
					add_srch_attr(zbuf,
					    COLUMN_NAME(tobj, i),
					    ENTRY_VAL(NIS_RES_OBJECT(eres), i));
				}
			}
			nis_freeresult(eres);
			alias_search = 0; /* loop around again */
			continue;
		}
		break; /* typically, we'll just break out of the loop */
	}

	/*
	 * Sort the returned entries.  This is necessary for entries which
	 * consume multiple objects to be coalesced later on, and it's just
	 * plain nice otherwise to deal with big tables in a logically sorted
	 * order.
	 */
	eobj = NIS_RES_OBJECT(eres);
	qsort(eobj, NIS_RES_NUMOBJ(eres), sizeof (nis_object),
	    tfp->sort_function);
#ifdef PERF_DEBUG
timestamp("Completed quicksort", "", TS_NOELAPSED);
#endif
	i = 0;

	/*
	 * Now stuff the data we found into the output structure, one
	 * row at a time.
	 */
	while (i < NIS_RES_NUMOBJ(eres)) {
		if ((rp = _dd_new_row()) == NULL) {
			if (tbl_err == NULL)
				*tbl_err = TBL_NO_MEMORY;
			nis_freeresult(eres);
			nis_freeresult(tres);
			return (TBL_FAILURE);
		}
		/*
		 * Set the value of each output column.
		 */
		for (k = 0; k < tfp->cols; ++k) {
			if (k == tfp->alias_col)
				continue;
			if (_dd_set_col_val(rp, tfp->cfmts[k].argno,
			    ENTRY_VAL(eobj, k), ttp->alias_sep)) {
				if (tbl_err == NULL)
					*tbl_err = TBL_NO_MEMORY;
				nis_freeresult(eres);
				nis_freeresult(tres);
				_dd_free_row(rp);
				return (TBL_FAILURE);
			}
		}

		/*
		 * Loop here coalesces the multiple entry objects used
		 * by a table like hosts into a single row in the output.
		 * The qsort comparison function ensured that the primary
		 * entry (cname == name) was first, so for each of these
		 * we just have to append the alias to the alias argument.
		 */
		for ((nobj = eobj + 1), (j = i + 1); ((tfp->alias_col >= 0) &&
		    (j < NIS_RES_NUMOBJ(eres))); ++j, ++nobj) {
			for (k = 0; k < tfp->cols; ++k) {
				if ((tfp->cfmts[k].flags & COL_KEY) &&
				    (k != tfp->alias_col)) {
					if (CASE_INSENSITIVE(tobj, k) &&
					    ENTRY_VAL(eobj, k) &&
					    ENTRY_VAL(nobj, k) &&
					    strcasecmp(ENTRY_VAL(eobj, k),
					    ENTRY_VAL(nobj, k)))
						break;
					else if (ENTRY_VAL(eobj, k) &&
					    ENTRY_VAL(nobj, k) &&
					    strcmp(ENTRY_VAL(eobj, k),
					    ENTRY_VAL(nobj, k)))
						break;
				}
			}
			if (k != tfp->cols)
				break;

			for (k = 0; k < tfp->cols; ++k) {
				if (k == tfp->comment_col)
					continue;
				else if ((tfp->cfmts[k].flags & COL_KEY) &&
				    (k != tfp->alias_col))
					    continue;
				else if (_dd_set_col_val(rp,
				    tfp->cfmts[k].argno, ENTRY_VAL(nobj, k),
				    ttp->alias_sep)) {
					if (tbl_err == NULL)
						*tbl_err = TBL_NO_MEMORY;
					nis_freeresult(eres);
					nis_freeresult(tres);
					_dd_free_row(rp);
					return (TBL_FAILURE);
				}
			}
		}
		/*
		 * Finally have a row put together, add it to the end of the
		 * table. Note that this is the first place where the tbl
		 * structure is modified.
		 */
		if (_dd_append_row(tbl, rp) != 0) {
			if (tbl_err == NULL)
				*tbl_err = TBL_NO_MEMORY;
			nis_freeresult(eres);
			nis_freeresult(tres);
			_dd_free_row(rp);
			return (TBL_FAILURE);
		}
		i = j;
		eobj = nobj;
	}

	nis_freeresult(eres);
	nis_freeresult(tres);
#ifdef PERF_DEBUG
timestamp("Completed coalesce & arg return", "", TS_NOELAPSED);
#endif
	return (TBL_SUCCESS);
}

/*
 * Set the value of an NIS+ table column. Caller must clean up.
 */
static void
set_col_val(nis_object *e, int i, char *v)
{
	ENTRY_VAL(e, i) = strdup(v);
	ENTRY_LEN(e, i) = strlen(v) + 1;
}

/*
 * Nuke a NIS+ table column value.
 */
static void
free_col_val(nis_object *e, int i)
{
	if (ENTRY_VAL(e, i) != NULL)
		(void) free(ENTRY_VAL(e, i));
	ENTRY_LEN(e, i) = 0;
}

/*
 * Function to add an entry to a NIS+ table.
 */
int
_add_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp, char **args)
{
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	nis_result *tres, *eres;
	nis_object nobj, *eobj = &nobj, *tobj;
	int (*null_fun)() = NULL;
	int i, j;
	char *ap = NULL, *as = NULL;
	int alias_search = 0, alias_cnt = 0, strip_comment = 0;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.  Need it in order
	 * to set up the queries for duplicate detection, as it contains
	 * the column names essential to nis_list calls.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name(NULL, zbuf, domain, zname);
	if (do_nis_lookup(zname, &tres, tbl_err) != 0)
		return (TBL_FAILURE);
	tobj = NIS_RES_OBJECT(tres);
	zbuf[0] = '\0';

	/*
	 * For each column which is supposed to be unique, do an nis_list()
	 * to see if an entry exists with the particular column set to that
	 * value.
	 */
	for (i = 0; i < tfp->cols; ++i) {
		if ((tfp->cfmts[i].flags & COL_UNIQUE) &&
		    (args[tfp->cfmts[i].argno] != NULL) &&
		    strlen(args[tfp->cfmts[i].argno])) {
			zbuf[0] = '\0';
			add_srch_attr(zbuf, COLUMN_NAME(tobj, i),
			    args[tfp->cfmts[i].argno]);
			build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain,
			    zname);
			eres = nis_list(zname, FOLLOW_LINKS|MASTER_ONLY,
			    null_fun, (void *)NULL);
			if ((NIS_RES_STATUS(eres) == NIS_SUCCESS) ||
			    (NIS_RES_STATUS(eres) == NIS_S_SUCCESS)) {
				nis_freeresult(eres);
				nis_freeresult(tres);
				if (tbl_err != NULL)
					*tbl_err = TBL_ENTRY_EXISTS;
				return (TBL_FAILURE);
			} else
				nis_freeresult(eres);
		}
	}

	/*
	 * No duplicates, so make up the entry and add it.
	 */
	build_nis_name(NULL, tobj->zo_name, tobj->zo_domain, zname);
	/*
	 * If this table is "aliased" and we have aliases, then
	 * set flags so we can deal with this later.
	 */
	if (tfp->alias_col >= 0) {
		alias_search = 1;
		ap = args[tfp->cfmts[tfp->alias_col].argno];
	}
	do {
		if (alias_search) {
			if (alias_cnt == 0) {
				if (ttp->type == TBL_HOSTS) {
					as = args[1];
				} else {
					as = args[0];
				}
			} else {
				as = strtok(ap, " ");
				if (ap != NULL)
					ap = NULL;
				++strip_comment;
			}
			if (as == NULL) {
				alias_search = 0;
				continue;
			}
			++alias_cnt;
		}
		/*
		 * Initialize object
		 */
		(void) memset(eobj, 0, sizeof (nis_object));
		eobj->zo_owner = nis_local_principal();
		eobj->zo_access = DEFAULT_RIGHTS;
		eobj->zo_group = tobj->zo_group;
		eobj->zo_domain = tobj->zo_domain;
		eobj->zo_ttl = tobj->zo_ttl;
		eobj->zo_data.zo_type = ENTRY_OBJ;
		eobj->EN_data.en_type = tobj->TA_data.ta_type;
		eobj->EN_data.en_cols.en_cols_val = (entry_col *)
		    calloc(tobj->TA_data.ta_maxcol, sizeof (entry_col));
		eobj->EN_data.en_cols.en_cols_len = tobj->TA_data.ta_maxcol;

		zbuf[0] = '\0';
		/*
		 * Walk argument list and stuff each into the
		 * appropriate column.
		 */
		for (i = 0; i < tfp->cols; ++i) {
			j = tfp->cfmts[i].argno;
			if (i == tfp->alias_col) {
				set_col_val(eobj, i, as);
			/* Only put one copy of the comment in to save space */
			} else if ((i == tfp->comment_col) && strip_comment) {
				continue;
			} else if (args[j] != NULL) {
				set_col_val(eobj, i, args[j]);
			}
		}
		eres = nis_add_entry(zname, eobj, 0);
		if (NIS_RES_STATUS(eres) != NIS_SUCCESS) {
			set_tbl_nis_error(NIS_RES_STATUS(eres), tbl_err);
			nis_freeresult(eres);
			nis_freeresult(tres);
			for (i = 0; i < tfp->cols; ++i)
				free_col_val(eobj, i);
			(void) free(eobj->EN_data.en_cols.en_cols_val);
			return (TBL_FAILURE);
		}
		for (i = 0; i < tfp->cols; ++i)
			free_col_val(eobj, i);
		(void) free(eobj->EN_data.en_cols.en_cols_val);
	} while (alias_search);

	nis_freeresult(eres);
	nis_freeresult(tres);
	return (TBL_SUCCESS);
}

/*
 * Function to remove an entry from a NIS+ table
 */
int
_rm_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp, char **args)
{
	int match_flag = 0;
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	nis_object *tobj;
	nis_result *tres, *eres;
	int cn, aa, alias_search = 0;
	int i;
	int (*null_fun)() = NULL;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name(NULL, zbuf, domain, zname);
	if (do_nis_lookup(zname, &tres, tbl_err) != 0)
		return (TBL_FAILURE);
	tobj = NIS_RES_OBJECT(tres);
	zbuf[0] = '\0';

	/*
	 * Set up alias search first.  We allow removals of individual
	 * entries by either their canonical name or an alias if the
	 * table has searchable aliases.  We have to do one lookup on
	 * the alias to get the canonical name, then remove on the
	 * canonical name to get all the aliases since each consumes
	 * a separate NIS+ object.
	 */
	aa = tfp->cfmts[tfp->alias_col].m_argno;
	if ((tfp->alias_col >= 0) &&
	    (tfp->cfmts[tfp->alias_col].flags & COL_KEY) &&
	    (args[aa] != NULL) && strlen(args[aa])) {
		++match_flag;
		alias_search = 1;
		add_srch_attr(zbuf, COLUMN_NAME(tobj, tfp->alias_col),
		    args[aa]);
	}

	/*
	 * Handle other search criteria which may have been specified.
	 */
	for (cn = 0; cn < tfp->cols; ++cn) {
		if ((i = tfp->cfmts[cn].m_argno) == -1)
			continue;
		if (i == aa)
			continue;
		if ((tfp->cfmts[cn].flags & COL_KEY) &&
		    (args[i] != NULL) && strlen(args[i])) {
			++match_flag;
			add_srch_attr(zbuf, COLUMN_NAME(tobj, cn), args[i]);
		}
	}
	build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain, zname);
	/*
	 * If this is an "aliased" table (multiple entries equal one of
	 * our Rows), we have to do a lookup to find the canonical name
	 * which links these entries together and reconstruct the indexed
	 * name so we can do the right thing below.
	 */
	if (alias_search) {
		eres = nis_list(zname, FOLLOW_LINKS|MASTER_ONLY, null_fun,
		    (void *) NULL);
		if ((NIS_RES_STATUS(eres) != NIS_SUCCESS) &&
		    (NIS_RES_STATUS(eres) != NIS_S_SUCCESS)) {
			set_tbl_nis_error(NIS_RES_STATUS(eres), tbl_err);
			nis_freeresult(tres);
			nis_freeresult(eres);
			return (TBL_FAILURE);
		} else {
			zbuf[0] = '\0';
			for (i = 0; i < tfp->cols; ++i) {
				if ((tfp->cfmts[i].flags & COL_KEY) &&
				    (i != tfp->alias_col)) {
					add_srch_attr(zbuf,
					    COLUMN_NAME(tobj, i),
					    ENTRY_VAL(NIS_RES_OBJECT(eres), i));
				}
			}
		}
		nis_freeresult(eres);
		build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain, zname);
	}
	/*
	 * Now do the remove
	 */
	eres = nis_remove_entry(zname, NULL, REM_MULTIPLE);
	if (NIS_RES_STATUS(eres) != NIS_SUCCESS) {
		set_tbl_nis_error(NIS_RES_STATUS(eres), tbl_err);
		nis_freeresult(tres);
		nis_freeresult(eres);
		return (TBL_FAILURE);
	}
	nis_freeresult(eres);
	nis_freeresult(tres);
	return (TBL_SUCCESS);
}

/* Define a'la ENTRY_VAL... */
#define	ENTRY_FLAGS(obj, col) \
	(obj)->EN_data.en_cols.en_cols_val[col].ec_flags

/*
 * Function to modify an entry in a NIS+ table.
 */
int
_mod_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp, char **args)
{
	int match_flag = 0;
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char tname[NIS_MAXNAMELEN], zname[NIS_MAXNAMELEN],
	    zname2[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN], zbuf2[NIS_MAXNAMELEN];
	nis_object *eobj, *tobj, cobj;
	nis_result *tres, *eres, *fres;
	int cn, aa, alias_search = 0, mods = 0;
	int i, j, k;
	int (*null_fun)() = NULL;
	char **ab = NULL, **at = NULL, *ap, *as;
	int ac = 0;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name(NULL, zbuf, domain, tname);
	if (do_nis_lookup(tname, &tres, tbl_err) != 0)
		return (TBL_FAILURE);
	tobj = NIS_RES_OBJECT(tres);
	zbuf[0] = '\0';
	build_nis_name(NULL, tobj->zo_name, tobj->zo_domain, tname);
	/*
	 * Set up alias search first.  We allow modifies of individual
	 * entries by either their canonical name or an alias if the
	 * table has searchable aliases.  We have to do one lookup on
	 * the alias to get the canonical name, then another on the
	 * canonical name to get all the aliases since each consumes
	 * a separate NIS+ object.
	 */
	aa = tfp->cfmts[tfp->alias_col].m_argno;
	if ((tfp->alias_col >= 0) &&
	    (tfp->cfmts[tfp->alias_col].flags & COL_KEY) &&
	    (args[aa] != NULL) && strlen(args[aa])) {
		++match_flag;
		alias_search = 1;
		add_srch_attr(zbuf, COLUMN_NAME(tobj, tfp->alias_col),
		    args[aa]);
	}

	/*
	 * Handle other search criteria which may have been specified.
	 */
	for (cn = 0; cn < tfp->cols; ++cn) {
		if ((i = tfp->cfmts[cn].m_argno) == -1)
			continue;
		if (i == aa)
			continue;
		if ((tfp->cfmts[cn].flags & COL_KEY) &&
		    (args[i] != NULL) && strlen(args[i])) {
			++match_flag;
			add_srch_attr(zbuf, COLUMN_NAME(tobj, cn), args[i]);
		}
	}
	if (!match_flag) {
		if (tbl_err != NULL)
			*tbl_err = TBL_MATCH_CRITERIA_BAD;
		nis_freeresult(tres);
		return (TBL_FAILURE);
	}

	/*
	 * Construct the name for the table that we found.
	 */
	for (;;) {
		build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain, zname);
		eres = nis_list(zname, FOLLOW_LINKS|MASTER_ONLY, null_fun,
		    (void *) NULL);
		if ((NIS_RES_STATUS(eres) != NIS_SUCCESS) &&
		    (NIS_RES_STATUS(eres) != NIS_S_SUCCESS)) {
			set_tbl_nis_error(NIS_RES_STATUS(eres), tbl_err);
			nis_freeresult(eres);
			nis_freeresult(tres);
			return (TBL_FAILURE);
		}
		/*
		 * If we're doing a match list & the first search was on an
		 * alias, now search on the real thing.  Alias column is
		 * excluded as we want to search on canonical keys to get
		 * all the aliases.
		 */
		if (match_flag && alias_search) {
			zbuf[0] = '\0';
			for (i = 0; i < tfp->cols; ++i) {
				if ((tfp->cfmts[i].flags & COL_KEY) &&
				    (i != tfp->alias_col)) {
					add_srch_attr(zbuf,
					    COLUMN_NAME(tobj, i),
					    ENTRY_VAL(NIS_RES_OBJECT(eres), i));
				}
			}
			nis_freeresult(eres);
			alias_search = 0;	/* loop around */
			continue;
		}
		break;	/* typically we break out of the loop */
	}

	/*
	 * Sort 'em so we know they're in order first.
	 */
	eobj = NIS_RES_OBJECT(eres);
	qsort(eobj, NIS_RES_NUMOBJ(eres), sizeof (nis_object),
	    tfp->sort_function);

	if (tfp->alias_col >= 0) {
		/*
		 * If this is a table which has alias columns, break up
		 * the alias argument into the individual tokens.
		 */
		ac = 1;
		at = (char **)calloc(ac, sizeof (char **));
		/*
		 * Stick an alias for the cname at the beginning
		 */
		if (ttp->type == TBL_HOSTS)
			at[0] = strdup(args[ttp->search_args + 1]);
		else
			at[0] = strdup(args[ttp->search_args]);
		ap = args[tfp->cfmts[tfp->alias_col].argno + ttp->search_args];
		if ((ap != NULL) && strlen(ap)) {
			for (as = strtok(ap, " "); as != NULL;
			    ap = NULL, as = strtok(ap, " ")) {
				at = (char **)realloc(at,
				    (++ac * sizeof (char **)));
				at[ac - 1] = strdup(as);
			}
		}
		/*
		 * If we actually have aliases specified, go through
		 * the existing objects and try to match them up.  This
		 * way we minimize the number of database modifications.
		 */
		if (ac > 1) {
			ab = (char **)calloc(ac, sizeof (char **));
			for (j = 0, eobj = NIS_RES_OBJECT(eres);
			    j < NIS_RES_NUMOBJ(eres); ++j, ++eobj) {
				for (k = 0; k < ac; ++k) {
					if (at[k] != NULL && strcasecmp(at[k],
					    ENTRY_VAL(eobj,
					    tfp->alias_col)) == 0)
						break;
				}
				if (k != ac) {
					ab[j] = at[k];
					at[k] = NULL;
				}
			}
			/*
			 * Now merge in any leftovers into unused slots.
			 */
			for (k = 0; k < ac; ++k) {
				if (at[k] == NULL)
					continue;
				for (j = 0; j < ac; ++j) {
					if (ab[j] == NULL)
						break;
				}
				if (j != ac) {
					ab[j] = at[k];
					at[k] = NULL;
				}
			}
			for (j = 0; j < ac; ++j) {
				if (at[j] != NULL)
					(void) free(at[j]);
			}
			(void) free(at);
		} else
			ab = at;
	}

	/*
	 * For each object we retrieved, modify if any data has changed.
	 */
	for (j = 0, eobj = NIS_RES_OBJECT(eres); j < NIS_RES_NUMOBJ(eres);
	    ++j, ++eobj) {
		/*
		 * If we're on an aliased table and we're supposed to delete
		 * some aliases, then remove the remaining objects.
		 */
		if (ac && !(j < ac)) {
			fres = nis_remove_entry(tname, eobj, 0);
			if (NIS_RES_STATUS(fres) != NIS_SUCCESS) {
				set_tbl_nis_error(NIS_RES_STATUS(fres),
				    tbl_err);
				for (i = 0; i < ac; ++i) {
					if (ab[i] != NULL)
						(void) free(ab[i]);
				}
				(void) free(ab);
				nis_freeresult(fres);
				nis_freeresult(eres);
				nis_freeresult(tres);
				return (TBL_FAILURE);
			}
			continue;
		}
		/*
		 * Now compare each column against new data and change values
		 * where necessary.  Otherwise leave it alone.
		 */
		zbuf[0] = '\0';
		(void) memset((char *)&cobj, 0, sizeof (cobj));
		for (cn = 0; cn < tfp->cols; ++cn) {
			/*
			 * Build indexed name as we go for modify_entry to
			 * use later on.
			 */
			if ((tobj->TA_data.ta_cols.ta_cols_val[cn].tc_flags &
			    TA_SEARCHABLE)) {
				add_srch_attr(zbuf, COLUMN_NAME(tobj, cn),
				    ENTRY_VAL(eobj, cn));
			}
			i = tfp->cfmts[cn].argno + ttp->search_args;
			if (cn == tfp->alias_col)
				args[i] = ab[j];
			if (args[i] != NULL) {
				if ((tfp->cfmts[cn].flags & COL_CASEI) &&
				    args[i] && ENTRY_VAL(eobj, cn) &&
				    strcasecmp(args[i], ENTRY_VAL(eobj,
				    cn)) == 0)
					continue;
				else if (args[i] && ENTRY_VAL(eobj, cn) &&
				    strcmp(args[i], ENTRY_VAL(eobj, cn)) == 0)
					continue;
				/*
				 * If it's a key which is supposed to be unique,
				 * check for existing duplication of new value.
				 */
				if ((tfp->cfmts[cn].flags &
				    (COL_KEY | COL_UNIQUE)) ==
				    (COL_KEY | COL_UNIQUE)) {
					zbuf2[0] = '\0';
					add_srch_attr(zbuf2,
					    COLUMN_NAME(tobj, cn), args[i]);
					build_nis_name(zbuf2, tobj->zo_name,
					    tobj->zo_domain, zname2);
					fres = nis_list(zname2,
					    FOLLOW_LINKS|MASTER_ONLY,
					    null_fun, (void *) NULL);
					if ((NIS_RES_STATUS(fres) ==
					    NIS_SUCCESS) ||
					    (NIS_RES_STATUS(fres) ==
					    NIS_S_SUCCESS)) {
						if (tbl_err != NULL)
							*tbl_err =
							    TBL_ENTRY_EXISTS;
						for (i = 0; i < ac; ++i) {
							if (ab[i] != NULL)
								(void) free(
								ab[i]);
						}
						(void) free(ab);
						nis_freeresult(fres);
						nis_freeresult(eres);
						nis_freeresult(tres);
						return (TBL_FAILURE);
					} else {
						nis_freeresult(fres);
					}
				/*
				 * Make sure we only put a non-null
				 * comment on the primary entry in an alias
				 * situation.  This relies on cname as col. 0,
				 * and comments always being a higher-numbered
				 * column than alias.
				 */
				} else if ((cn == tfp->comment_col) &&
				    (ac > 1) && strlen(args[i]) != 0 &&
				    strcasecmp(ENTRY_VAL(eobj, 0),
				    ENTRY_VAL(eobj, tfp->alias_col)) == 0)
					continue;
				/*
				 * Copy object & replace column.
				 */
				if (!mods)
					cobj = *eobj;
				set_col_val(&cobj, cn, args[i]);
				ENTRY_FLAGS(&cobj, cn) |= EN_MODIFIED;
				++mods;
			/*
			 * Make sure to clear columns, too.
			 */
			} else if (ENTRY_LEN(eobj, cn) > 1) {
				if (!mods)
					cobj = *eobj;
				set_col_val(&cobj, cn, "");
				ENTRY_FLAGS(&cobj, cn) |= EN_MODIFIED;
				++mods;
			}
		}

		/*
		 * If anything changed, make the modification.
		 */
		if (mods) {
			build_nis_name(zbuf, tobj->zo_name, tobj->zo_domain,
			    zname);
			fres = nis_modify_entry(zname, &cobj, 0);

			/* clean up the data we duplicated above */
			for (cn = 0; cn < tfp->cols; ++cn) {
				if (ENTRY_FLAGS(&cobj, cn) & EN_MODIFIED)
					free_col_val(&cobj, cn);
			}

			if (NIS_RES_STATUS(fres) != NIS_SUCCESS) {
				set_tbl_nis_error(NIS_RES_STATUS(fres),
				    tbl_err);
				for (i = 0; i < ac; ++i) {
					if (ab[i] != NULL)
						(void) free(ab[i]);
				}
				(void) free(ab);
				nis_freeresult(fres);
				nis_freeresult(eres);
				nis_freeresult(tres);
				return (TBL_FAILURE);
			}
		}
	}

	nis_freeresult(eres);

	/*
	 * Finally, if we have aliases left over to add, add each.
	 */
	if (j < ac) {
		/*
		 * Initialize object.
		 */
		(void) memset((char *)&cobj, 0, sizeof (nis_object));
		eobj = &cobj;
		eobj->zo_owner = nis_local_principal();
		eobj->zo_access = DEFAULT_RIGHTS;
		eobj->zo_group = tobj->zo_group;
		eobj->zo_domain = tobj->zo_domain;
		eobj->zo_ttl = tobj->zo_ttl;
		eobj->zo_data.zo_type = ENTRY_OBJ;
		eobj->EN_data.en_type = tobj->TA_data.ta_type;
		eobj->EN_data.en_cols.en_cols_val = (entry_col *)
		    calloc(tobj->TA_data.ta_maxcol, sizeof (entry_col));
		eobj->EN_data.en_cols.en_cols_len = tobj->TA_data.ta_maxcol;

		zbuf[0] = '\0';
		/*
		 * Walk argument list and stuff each into the
		 * appropriate column.
		 */
		for (i = 0; i < tfp->cols; ++i) {
			k = tfp->cfmts[i].argno + ttp->search_args;
			if ((i == tfp->alias_col) || (i == tfp->comment_col)) {
			/* Skip alias & comment columns; we'll get it later. */
				continue;
			} else if (args[k] != NULL) {
				set_col_val(eobj, i, args[k]);
			}
		}
		/*
		 * Now add one entry for each alias we have left.
		 */
		for (k = j; k < ac; ++k) {
			set_col_val(eobj, tfp->alias_col, ab[k]);
			/*
			 * Make sure to put comment on only the primary entry.
			 */
			if (!strcasecmp(ab[k], ENTRY_VAL(eobj, 0))) {
				set_col_val(eobj, tfp->comment_col,
				    args[tfp->cfmts[tfp->comment_col].argno +
				    ttp->search_args]);
			} else {
				set_col_val(eobj, tfp->comment_col, "");
			}
			eres = nis_add_entry(tname, eobj, 0);
			if (NIS_RES_STATUS(eres) != NIS_SUCCESS) {
				set_tbl_nis_error(NIS_RES_STATUS(eres),
				    tbl_err);
				for (i = 0; i < eobj->TA_data.ta_maxcol; i++)
					free_col_val(eobj, i);
				(void) free(eobj->EN_data.en_cols.en_cols_val);
				for (i = 0; i < ac; ++i) {
					if (ab[i] != NULL)
						(void) free(ab[i]);
				}
				(void) free(ab);
				nis_freeresult(eres);
				nis_freeresult(tres);
				return (TBL_FAILURE);
			}
			nis_freeresult(eres);
		}
		for (i = 0; i < eobj->TA_data.ta_maxcol; i++)
			free_col_val(eobj, i);
		(void) free(eobj->EN_data.en_cols.en_cols_val);
	}

	for (i = 0; i < ac; ++i) {
		if (ab[i] != NULL)
			(void) free(ab[i]);
	}
	(void) free(ab);

	nis_freeresult(tres);
	return (TBL_SUCCESS);
}

/*
 * Function to create a new NIS+ table
 */
int
_make_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp, struct tbl_make_data *tmp, char *user,
    char *group)
{
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	char ubuff[NIS_MAXNAMELEN], gbuff[NIS_MAXNAMELEN];
	nis_object  nobj;
	table_obj *tobj;
	nis_result *tres;
	u_long rights = 0;
	char *user_nam;
	char *user_dom;
	char *group_nam;
	char *group_dom;
	char *tp;
	int status;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name((char *)NULL, zbuf, domain, zname);
	/*
	 * Construct owner principal name and check existence.
	 * Set all access rights for owner.
	 * If user not specified, get process's real userid.
	 */
	ubuff[0] = '\0';
	user_nam = user;
	user_dom = (char *)NULL;
	if (user_nam == (char *)NULL)
		user_nam = (char *)nis_local_principal();
	if (user_nam != (char *)NULL) {
		strcpy(zbuf, user_nam);
		if ((tp = strchr(zbuf, '.')) != (char *)NULL) {
			*tp++ = '\0';
			user_nam = zbuf;
			user_dom = tp;
		}
	}
	if ((status = do_nis_user_name(user_nam, user_dom, ubuff)) != 0) {
		if (tbl_err != NULL)
			*tbl_err = status;
		return (TBL_FAILURE);
	}
	rights = DEFAULT_RIGHTS;
	/*
	 * Construct group name if specified and check existense.
	 * If valid group, set all access rights for it.
	 * If group not specified, get default group from NIS_GROUP env var.
	 */
	gbuff[0] = '\0';
	group_nam = group;
	group_dom = (char *)NULL;
	if (group_nam == (char *)NULL)
		group_nam = (char *)nis_local_group();
	if ((group_nam != (char *)NULL) && ((int)strlen(group_nam) > 0)) {
		strcpy(zbuf, group_nam);
		if ((tp = strchr(zbuf, '.')) != (char *)NULL) {
			*tp++ = '\0';
			group_nam = zbuf;
			group_dom = tp;
		}
		if ((do_nis_group_name(group_nam, group_dom, gbuff)) != 0) {
			if (tbl_err != NULL)
				*tbl_err = TBL_NO_GROUP;
			return (TBL_FAILURE);
		}
		rights |= ((NIS_READ_ACC + NIS_MODIFY_ACC + NIS_CREATE_ACC +
		    NIS_DESTROY_ACC) << 8);
	}
	/*
	 * Construct NIS+ table object from ground up.
	 */
	nobj.zo_name = zname;
	nobj.zo_domain = domain;
	nobj.zo_owner = ubuff;
	nobj.zo_group = gbuff;
	nobj.zo_access = rights;
	nobj.zo_ttl = 12 * 60 * 60;
	nobj.zo_data.zo_type = TABLE_OBJ;
	tobj = &nobj.zo_data.objdata_u.ta_data;
	tobj->ta_type = tmp->ta_type;
	tobj->ta_path = tmp->ta_path;
	tobj->ta_maxcol = tmp->ta_maxcol;
	tobj->ta_sep = tmp->ta_sep;
	tobj->ta_cols.ta_cols_len = tmp->ta_maxcol;
	tobj->ta_cols.ta_cols_val = &tmp->col_info[0];
	/*
	 * Simply add the new table object.  Process errors.
	 */
	tres = nis_add(zname, &nobj);
	set_tbl_nis_error(NIS_RES_STATUS(tres), tbl_err);
	if (NIS_RES_STATUS(tres) != NIS_SUCCESS &&
	    NIS_RES_STATUS(tres) != NIS_S_SUCCESS)
		status = TBL_FAILURE;
	else
		status = TBL_SUCCESS;

	nis_freeresult(tres);
	return (status);
}

/*
 * Function to delete a NIS+ table
 */

int
_del_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp)
{
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	nis_result *tres;
	int status = TBL_SUCCESS;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name((char *)NULL, zbuf, domain, zname);
	/*
	 * Check if table exists.
	 */
	tres = nis_lookup(zname, FOLLOW_LINKS);
	set_tbl_nis_error(NIS_RES_STATUS(tres), tbl_err);
	if (NIS_RES_STATUS(tres) != NIS_SUCCESS &&
	    NIS_RES_STATUS(tres) != NIS_S_SUCCESS) {
		nis_freeresult(tres);
		return (TBL_FAILURE);
	}
	nis_freeresult(tres);

	/*
	 * Must purge a table before it can be removed.
	 */
	(void) sprintf(zbuf, "[],%s", zname);
	tres = nis_remove_entry(zbuf, (nis_object *)NULL, REM_MULTIPLE);
	set_tbl_nis_error(NIS_RES_STATUS(tres), tbl_err);
	if (NIS_RES_STATUS(tres) != NIS_SUCCESS &&
	    NIS_RES_STATUS(tres) != NIS_NOTFOUND) {
		nis_freeresult(tres);
		return (TBL_FAILURE);
	}
	nis_freeresult(tres);

	/*
	 * Simply remove object by name.  Process error cases.
	 */
	tres = nis_remove(zname, (nis_object *)NULL);
	set_tbl_nis_error(NIS_RES_STATUS(tres), tbl_err);
	if (NIS_RES_STATUS(tres) != NIS_SUCCESS &&
	    NIS_RES_STATUS(tres) != NIS_S_SUCCESS)
		status = TBL_FAILURE;
	else
		status = TBL_SUCCESS;

	nis_freeresult(tres);
	return (status);
}

/*
 * Function to verify existence of a NIS+ table
 */

int
_stat_dd_nisplus(char *name, char *domain, int *tbl_err,
    struct tbl_trans_data *ttp, Tbl_stat **tbl_stpp)
{
	Tbl_stat *tbl_st;
	struct tbl_fmt *tfp = &ttp->fmts[TBL_NS_NISPLUS];
	char zname[NIS_MAXNAMELEN], zbuf[NIS_MAXNAMELEN];
	nis_object *nobj;
	nis_result *tres;

	if ((name == NULL) || !strlen(name))
		name = tfp->name;
	/*
	 * Get the table object using expand name magic.
	 */
	build_tbl_name(name, tfp->prefix, tfp->suffix, zbuf);
	build_nis_name((char *)NULL, zbuf, domain, zname);
	/*
	 * Simply lookup object by name.  Process error cases.
	 */
	tres = nis_lookup(zname, FOLLOW_LINKS);
	set_tbl_nis_error(NIS_RES_STATUS(tres), tbl_err);
	if (NIS_RES_STATUS(tres) != NIS_SUCCESS &&
	    NIS_RES_STATUS(tres) != NIS_S_SUCCESS) {
		nis_freeresult(tres);
		return (TBL_FAILURE);
	}

	nobj = NIS_RES_OBJECT(tres);

	/*
	 * Construct table stat structure.
	 */
	if ((tbl_st = (Tbl_stat *)malloc(sizeof (struct tbl_stat))) ==
	    (Tbl_stat *)NULL) {
		if (tbl_err != NULL)
			*tbl_err = TBL_NO_MEMORY;
	}
	tbl_st->name = strdup(name);
	tbl_st->ns = TBL_NS_NISPLUS;
	tbl_st->perm.nis.mode = nobj->zo_access;
	if (nobj->zo_owner != (char *)NULL)
		tbl_st->perm.nis.owner_user = strdup(nobj->zo_owner);
	if (nobj->zo_group != (char *)NULL)
		tbl_st->perm.nis.owner_group = strdup(nobj->zo_group);
	tbl_st->atime = (time_t)0;
	tbl_st->mtime = (time_t)0;
	nis_freeresult(tres);

	/*
	 * Return pointer to table stat structure.
	 */
	*tbl_stpp = tbl_st;
	return (TBL_SUCCESS);
}

/*
 * Function to return the list of per-network tables.
 */
char **
_dd_ls_nisplus(int *tbl_err, const char *pathp)
{
	char **outlist = NULL;
	int outcnt = 0;
	nis_result *nres, *tres;
	nis_object *nobj, *tobj;
	char nbuf[NIS_MAXNAMELEN];
	int i;

	/*
	 * List the directory to get all the names
	 */
	nbuf[0] = '\0';
	if (pathp != NULL) {
		sprintf(nbuf, "org_dir.%s", pathp);
	}
	nres = nis_list(nbuf, FOLLOW_LINKS, NULL, NULL);
	if ((NIS_RES_STATUS(nres) != NIS_SUCCESS) &&
	    (NIS_RES_STATUS(nres) != NIS_S_SUCCESS)) {
		*tbl_err = TBL_NO_ENTRY;
		return (NULL);
	}

	nobj = NIS_RES_OBJECT(nres);

	for (i = 0; i < NIS_RES_NUMOBJ(nres); ++i) {
		/*
		 * If it's a table or link with the right characters in
		 * its name, look it up
		 */
		if ((strspn(nobj[i].zo_name, "0123456789_") ==
		    strlen(nobj[i].zo_name)) &&
		    ((ENTRY_LEN(&nobj[i], 0) == TABLE_OBJ) ||
		    (ENTRY_LEN(&nobj[i], 0) == LINK_OBJ))) {
			sprintf(nbuf, "%s.%s", nobj[i].zo_name,
			    nobj[i].zo_domain);
			tres = nis_lookup(nbuf, FOLLOW_LINKS);
			if ((NIS_RES_STATUS(tres) != NIS_SUCCESS) &&
			    (NIS_RES_STATUS(tres) != NIS_S_SUCCESS)) {
				/* Clean up memory allocated */
				while (--outcnt >= 0) {
					free(outlist[outcnt]);
				}
				free(outlist);
				nis_freeresult(tres);
				nis_freeresult(nres);
				*tbl_err = TBL_NO_ENTRY;
				return (NULL);
			}

			tobj = NIS_RES_OBJECT(tres);

			/* If table type is correct, append to output */
			if ((tobj->zo_data.zo_type == TABLE_OBJ) &&
			    (strcmp(tobj->TA_data.ta_type,
			    "dhcp_ip_tbl") == 0)) {
				++outcnt;
				outlist = realloc(outlist,
				    sizeof (char *) * outcnt);
				if (outlist == NULL) {
					nis_freeresult(tres);
					nis_freeresult(nres);
					*tbl_err = TBL_NO_MEMORY;
					return (NULL);
				}
				outlist[outcnt - 1] =
				    strdup(nobj[i].zo_name);
				if (outlist[outcnt - 1] == NULL) {
					--outcnt;
					while (--outcnt >= 0) {
						free(outlist[outcnt]);
					}
					nis_freeresult(tres);
					nis_freeresult(nres);
					*tbl_err = TBL_NO_MEMORY;
					return (NULL);
				}
			}
			nis_freeresult(tres);
		}
	}

	nis_freeresult(nres);

	/* Terminate list with a null entry */
	outlist = realloc(outlist, sizeof (char *) * ++outcnt);
	if (outlist == NULL) {
		*tbl_err = TBL_NO_MEMORY;
		return (NULL);
	}
	outlist[outcnt - 1] = NULL;
	return (outlist);
}
