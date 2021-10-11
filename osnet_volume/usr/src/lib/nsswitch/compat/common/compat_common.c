/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 *
 * Common code and structures used by name-service-switch "compat" backends.
 *
 * Most of the code in the "compat" backend is a perverted form of code from
 * the "files" backend;  this file is no exception.
 */

#pragma ident	"@(#)compat_common.c	1.15	99/10/11 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "compat_common.h"

/*
 * This should be in a header.
 */

extern int yp_get_default_domain(char **domain);

/*
 * Routines to manage list of "-" users for get{pw, sp, gr}ent().  Current
 *   implementation is completely moronic; we use a linked list.  But then
 *   that's what it's always done in 4.x...
 */

struct setofstrings {
	char			*name;
	struct setofstrings	*next;
	/*
	 * === Should get smart and malloc the string and pointer as one
	 *	object rather than two.
	 */
};
typedef struct setofstrings	*strset_t;

static void
strset_free(ssp)
	strset_t	*ssp;
{
	strset_t	cur, nxt;

	for (cur = *ssp;  cur != 0;  cur = nxt) {
		nxt = cur->next;
		free(cur->name);
		free(cur);
	}
	*ssp = 0;
}

static boolean_t
strset_add(ssp, nam)
	strset_t	*ssp;
	const char	*nam;
{
	strset_t	new;

	if (0 == (new = (strset_t) malloc(sizeof (*new)))) {
		return (B_FALSE);
	}
	if (0 == (new->name = malloc(strlen(nam) + 1))) {
		free(new);
		return (B_FALSE);
	}
	strcpy(new->name, nam);
	new->next = *ssp;
	*ssp = new;
	return (B_TRUE);
}

static boolean_t
strset_in(ssp, nam)
	const strset_t	*ssp;
	const char	*nam;
{
	strset_t	cur;

	for (cur = *ssp;  cur != 0;  cur = cur->next) {
		if (strcmp(cur->name, nam) == 0) {
			return (B_TRUE);
		}
	}
	return (B_FALSE);
}


struct compat_backend {
	compat_backend_op_t	*ops;
	int			n_ops;
	const char		*filename;
	FILE			*f;
	int			minbuf;
	char			*buf;
	int			linelen;	/* <== Explain use, lifetime */

	nss_db_initf_t		db_initf;
	nss_db_root_t		*db_rootp;	/* Shared between instances */
	nss_getent_t		db_context;	/* Per-instance enumeration */

	compat_get_name		getnamef;
	compat_merge_func	mergef;

	/* We wouldn't need all this hokey state stuff if we */
	/*   used another thread to implement a coroutine... */
	enum {
		GETENT_FILE,
		GETENT_NETGROUP,
		GETENT_ALL,
		GETENT_DONE
	}			state;
	strset_t		minuses;

	int			permit_netgroups;
	const char		*yp_domain;
	nss_backend_t		*getnetgrent_backend;
	char			*netgr_buffer;
};


/*
 * Lookup and enumeration routines for +@group and -@group.
 *
 * This code knows a lot more about lib/libc/port/gen/getnetgrent.c than
 *   is really healthy.  The set/get/end routines below duplicate code
 *   from that file, but keep the state information per-backend-instance
 *   instead of just per-process.
 */

extern void _nss_initf_netgroup(nss_db_params_t *);
/*
 * Should really share the db_root in getnetgrent.c in order to get the
 *   resource-management quotas right, but this will have to do.
 */
static DEFINE_NSS_DB_ROOT(netgr_db_root);

static boolean_t
netgr_in(compat_backend_ptr_t be, const char *group, const char *user)
{
	if (be->yp_domain == 0) {
		if (yp_get_default_domain((char **)&be->yp_domain) != 0) {
			return (B_FALSE);
		}
	}
	return (innetgr(group, 0, user, be->yp_domain));
}

static boolean_t
netgr_all_in(compat_backend_ptr_t be, const char *group)
{
	/*
	 * 4.x does this;  ours not to reason why...
	 */
	return (netgr_in(be, group, "*"));
}

static void
netgr_set(be, netgroup)
	compat_backend_ptr_t	be;
	const char		*netgroup;
{
	/*
	 * ===> Need comment to explain that this first "if" is optimizing
	 *	for the same-netgroup-as-last-time case
	 */
	if (be->getnetgrent_backend != 0 &&
	    NSS_INVOKE_DBOP(be->getnetgrent_backend,
			    NSS_DBOP_SETENT,
			    (void *) netgroup) != NSS_SUCCESS) {
		NSS_INVOKE_DBOP(be->getnetgrent_backend, NSS_DBOP_DESTRUCTOR,
				0);
		be->getnetgrent_backend = 0;
	}
	if (be->getnetgrent_backend == 0) {
		struct nss_setnetgrent_args	args;

		args.netgroup	= netgroup;
		args.iterator	= 0;
		nss_search(&netgr_db_root, _nss_initf_netgroup,
			NSS_DBOP_NETGROUP_SET, &args);
		be->getnetgrent_backend = args.iterator;
	}
}

static boolean_t
netgr_next_u(be, up)
	compat_backend_ptr_t	be;
	char			**up;
{
	if (be->netgr_buffer == 0 &&
	    (be->netgr_buffer = malloc(NSS_BUFLEN_NETGROUP)) == 0) {
		/* Out of memory */
		return (B_FALSE);
	}

	do {
		struct nss_getnetgrent_args	args;

		args.buffer	= be->netgr_buffer;
		args.buflen	= NSS_BUFLEN_NETGROUP;
		args.status	= NSS_NETGR_NO;

		if (be->getnetgrent_backend != 0) {
			NSS_INVOKE_DBOP(be->getnetgrent_backend,
					NSS_DBOP_GETENT, &args);
		}

		if (args.status == NSS_NETGR_FOUND) {
			*up	  = args.retp[NSS_NETGR_USER];
		} else {
			return (B_FALSE);
		}
	} while (*up == 0);
	return (B_TRUE);
}

static void
netgr_end(be)
	compat_backend_ptr_t	be;
{
	if (be->getnetgrent_backend != 0) {
		NSS_INVOKE_DBOP(be->getnetgrent_backend,
				NSS_DBOP_DESTRUCTOR, 0);
		be->getnetgrent_backend = 0;
	}
	if (be->netgr_buffer != 0) {
		free(be->netgr_buffer);
		be->netgr_buffer = 0;
	}
}


#define	MAXFIELDS 9	/* Sufficient for passwd (7), shadow (9), group (4) */

static nss_status_t
do_merge(be, args, instr, linelen)
	compat_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	const char		*instr;
	int			linelen;
{
	char			*fields[MAXFIELDS];
	int			i;
	int			overrides;
	const char		*p;
	const char		*end = instr + linelen;
	nss_status_t		res;

	/*
	 * Potential optimization:  only perform the field-splitting nonsense
	 *   once per input line (at present, "+" and "+@netgroup" entries
	 *   will cause us to do this multiple times in getent() requests).
	 */

	for (i = 0;  i < MAXFIELDS;  i++) {
		fields[i] = 0;
	}
	for (p = instr, overrides = 0, i = 0; /* no test */; i++) {
		const char	*q = memchr(p, ':', end - p);
		const char	*r = (q == 0) ? end : q;
		ssize_t		len = r - p;

		if (len > 0) {
			char	*s = malloc(len + 1);
			if (s == 0) {
				overrides = -1;	/* Indicates "you lose" */
				break;
			}
			memcpy(s, p, len);
			s[len] = '\0';
			fields[i] = s;
			overrides++;
		}
		if (q == 0) {
			/* End of line */
			break;
		} else {
			/* Skip the colon at (*q) */
			p = q + 1;
		}
	}
	if (overrides == 1) {
		/* No real overrides, return (*args) intact */
		res = NSS_SUCCESS;
	} else if (overrides > 1) {
		/*
		 * The zero'th field is always nonempty (+/-...), but at least
		 *   one other field was also nonempty, i.e. wants to override
		 */
		switch ((*be->mergef)(be, args, (const char **)fields)) {
		    case NSS_STR_PARSE_SUCCESS:
			args->returnval	= args->buf.result;
			args->erange	= 0;
			res = NSS_SUCCESS;
			break;
		    case NSS_STR_PARSE_ERANGE:
			args->returnval	= 0;
			args->erange	= 1;
			res = NSS_NOTFOUND;
			break;
		    case NSS_STR_PARSE_PARSE:
			args->returnval	= 0;
			args->erange	= 0;
/* ===> Very likely the wrong thing to do... */
			res = NSS_NOTFOUND;
			break;
		}
	} else {
		args->returnval	= 0;
		args->erange	= 0;
		res = NSS_UNAVAIL;	/* ==> Right? */
	}

	for (i = 0;  i < MAXFIELDS;  i++) {
		if (fields[i] != 0) {
			free(fields[i]);
		}
	}

	return (res);
}


/*ARGSUSED*/
nss_status_t
_nss_compat_setent(be, dummy)
	compat_backend_ptr_t	be;
	void			*dummy;
{
	if (be->f == 0) {
		if (be->filename == 0) {
			/* Backend isn't initialized properly? */
			return (NSS_UNAVAIL);
		}
		if ((be->f = fopen(be->filename, "r")) == 0) {
			return (NSS_UNAVAIL);
		}
	} else {
		rewind(be->f);
	}
	strset_free(&be->minuses);
	/* ===> ??? nss_endent(be->db_rootp, be->db_initf, &be->db_context); */
	be->state = GETENT_FILE;
	/* ===> ??  netgroup stuff? */
	return (NSS_SUCCESS);
}

/*ARGSUSED*/
nss_status_t
_nss_compat_endent(be, dummy)
	compat_backend_ptr_t	be;
	void			*dummy;
{
	if (be->f != 0) {
		fclose(be->f);
		be->f = 0;
	}
	if (be->buf != 0) {
		free(be->buf);
		be->buf = 0;
	}
	nss_endent(be->db_rootp, be->db_initf, &be->db_context);

	be->state = GETENT_FILE; /* Probably superfluous but comforting */
	strset_free(&be->minuses);
	netgr_end(be);

	/*
	 * Question: from the point of view of resource-freeing vs. time to
	 *   start up again, how much should we do in endent() and how much
	 *   in the destructor?
	 */
	return (NSS_SUCCESS);
}

/*ARGSUSED*/
nss_status_t
_nss_compat_destr(be, dummy)
	compat_backend_ptr_t	be;
	void			*dummy;
{
	if (be != 0) {
		if (be->f != 0) {
			_nss_compat_endent(be, 0);
		}
		nss_delete(be->db_rootp);
		nss_delete(&netgr_db_root);
		free(be);
	}
	return (NSS_SUCCESS);	/* In case anyone is dumb enough to check */
}

static int
read_line(f, buffer, buflen)
	FILE			*f;
	char			*buffer;
	int			buflen;
{
	/*CONSTCOND*/
	while (1) {
		int	linelen;

		if (fgets(buffer, buflen, f) == 0) {
			/* End of file */
			return (-1);
		}
		linelen = strlen(buffer);
		/* linelen >= 1 (since fgets didn't return 0) */

		if (buffer[linelen - 1] == '\n') {
			/*
			 * ===> The code below that calls read_line() doesn't
			 *	play by the rules;  it assumes in places that
			 *	the line is null-terminated.  For now we'll
			 *	humour it.
			 */
			buffer[--linelen] = '\0';
			return (linelen);
		}
		if (feof(f)) {
			/* Line is last line in file, and has no newline */
			return (linelen);
		}
		/* Line too long for buffer;  toss it and loop for next line */
		/* ===== should syslog() in cases where previous code did */
		while (fgets(buffer, buflen, f) != 0 &&
		    buffer[strlen(buffer) - 1] != '\n') {
			;
		}
	}
}

nss_status_t
_nss_compat_XY_all(be, args, check, op_num)
	compat_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	compat_XY_check_func	check;
	nss_dbop_t		op_num;
{
	nss_status_t		res;
	int			parsestat;

	if (be->buf == 0 &&
	    (be->buf = malloc(be->minbuf)) == 0) {
		return (NSS_UNAVAIL); /* really panic, malloc failed */
	}

	if ((res = _nss_compat_setent(be, 0)) != NSS_SUCCESS) {
		return (res);
	}

	res = NSS_NOTFOUND;

	/*CONSTCOND*/
	while (1) {
		int		linelen;
		char		*instr	= be->buf;
		char		*colon;

		linelen = read_line(be->f, instr, be->minbuf);
		if (linelen < 0) {
			/* End of file */
			args->returnval = 0;
			args->erange    = 0;
			break;
		}

		if (instr[0] != '+' && instr[0] != '-') {
			/* Simple, wholesome, God-fearing entry */
			args->returnval = 0;
			parsestat = (*args->str2ent)(instr, linelen,
						    args->buf.result,
						    args->buf.buffer,
						    args->buf.buflen);
			if (parsestat == NSS_STR_PARSE_SUCCESS) {
				args->returnval = args->buf.result;
				if ((*check)(args) != 0) {
					res = NSS_SUCCESS;
					break;
				}

/* ===> Check the Dani logic here... */

			} else if (parsestat == NSS_STR_PARSE_ERANGE) {
				args->erange = 1;
				res = NSS_NOTFOUND;
				break;
				/* should we just skip this one long line ? */
			} /* else if (parsestat == NSS_STR_PARSE_PARSE) */
				/* don't care ! */

/* ==> ?? */		continue;
		}

		/*
		 * Process "+", "+name", "+@netgroup", "-name" or "-@netgroup"
		 *
		 * Possible optimization:  remember whether we've already done
		 *   the nss_search and, if so, whether we found the relevant
		 *   entry.
		 *
		 * Some other possible optimizations are ignored in the name
		 *   of uniform code;  e.g. when doing a lookup by name we
		 *   we could sometimes eliminate the nss_search(), but this
		 *   doesn't work for lookups by other keys (uid or gid).
		 */

		args->returnval = 0;
		nss_search(be->db_rootp, be->db_initf, op_num, args);
		if (args->returnval == 0) {
			/* ==> ?? Should treat ERANGE differently? */
			continue;
		}

		if ((colon = strchr(instr, ':')) != 0) {
			/* Make life easy in code below;  terminate field */
			*colon = '\0';
		}

		if (instr[1] == '@') {
			/* "+@netgroup" or "-@netgroup" */
			if (!be->permit_netgroups ||
			    !netgr_in(be, instr + 2, (*be->getnamef)(args))) {
				continue;
			}
		} else if (instr[1] == '\0') {
			/* "+" or (illegal) "-" */
			if (instr[0] == '-') {
				continue;
			}
		} else {
			/* "+name" or "-name" */
			if (strcmp(instr + 1, (*be->getnamef)(args)) != 0) {
				continue;
			}
		}

		if (instr[0] == '-') {
			args->returnval	= 0;
			args->erange	= 0;
			res = NSS_NOTFOUND;
		} else {	/* '+' */
			if (colon != 0) {
				/* Restore the one we smashed above */
				*colon = ':';
			}
			res = do_merge(be, args, instr, linelen);
		}
		break;
	}

	/*
	 * stayopen is set to 0 by default in order to close the opened
	 * file.  Some applications may break if it is set to 1.
	 */
	if (!args->stayopen) {
		(void) _nss_compat_endent(be, 0);
	}

	return (res);
}

nss_status_t
_nss_compat_getent(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*args = (nss_XbyY_args_t *) a;
	nss_status_t		res;
	char			*colon = 0; /* <=== need comment re lifetime */

	if (be->f == 0) {
		if ((res = _nss_compat_setent(be, 0)) != NSS_SUCCESS) {
			return (res);
		}
	}

	if (be->buf == 0 &&
	    (be->buf = malloc(be->minbuf)) == 0) {
		return (NSS_UNAVAIL); /* really panic, malloc failed */
	}

	/*CONSTCOND*/
	while (1) {
		char		*instr	= be->buf;
		int		linelen;
		char		*name;	/* === Need more distinctive label */
		const char	*savename;

		/*
		 * In the code below...
		 *    break	means "I found one, I think" (i.e. goto the
		 *		code after the end of the switch statement),
		 *    continue	means "Next candidate"
		 *		(i.e. loop around to the switch statement),
		 *    return	means "I'm quite sure" (either Yes or No).
		 */
		switch (be->state) {

		    case GETENT_DONE:
			args->returnval	= 0;
			args->erange	= 0;
			return (NSS_NOTFOUND);

		    case GETENT_FILE:
			linelen = read_line(be->f, instr, be->minbuf);
			if (linelen < 0) {
				/* End of file */
				be->state = GETENT_DONE;
				continue;
			}
			if ((colon = strchr(instr, ':')) != 0) {
				*colon = '\0';
			}
			if (instr[0] == '-') {
				if (instr[1] != '@') {
					strset_add(&be->minuses, instr + 1);
				} else if (be->permit_netgroups) {
					netgr_set(be, instr + 2);
					while (netgr_next_u(be, &name)) {
						strset_add(&be->minuses,
							name);
					}
					netgr_end(be);
				} /* Else (silently) ignore the entry */
				continue;
			} else if (instr[0] != '+') {
				int	parsestat;
				/*
				 * Normal entry, no +/- nonsense
				 */
				if (colon != 0) {
					*colon = ':';
				}
				args->returnval = 0;
				parsestat = (*args->str2ent)(instr, linelen,
							args->buf.result,
							args->buf.buffer,
							args->buf.buflen);
				if (parsestat == NSS_STR_PARSE_SUCCESS) {
					args->returnval = args->buf.result;
					return (NSS_SUCCESS);
				}
				/* ==> ?? Treat ERANGE differently ?? */
				if (parsestat == NSS_STR_PARSE_ERANGE) {
					args->returnval = 0;
					args->erange = 1;
					return (NSS_NOTFOUND);
				}
				/* Skip the offending entry, get next */
				continue;
			} else if (instr[1] == '\0') {
				/* Plain "+" */
				nss_setent(be->db_rootp, be->db_initf,
					&be->db_context);
				be->state = GETENT_ALL;
				be->linelen = linelen;
				continue;
			} else if (instr[1] == '@') {
				/* "+@netgroup" */
				netgr_set(be, instr + 2);
				be->state = GETENT_NETGROUP;
				be->linelen = linelen;
				continue;
			} else {
				/* "+name" */
				name = instr + 1;
				break;
			}
			/* NOTREACHED */

		    case GETENT_ALL:
			linelen = be->linelen;
			args->returnval = 0;
			nss_getent(be->db_rootp, be->db_initf,
				&be->db_context, args);
			if (args->returnval == 0) {
				/* ==> ?? Treat ERANGE differently ?? */
				nss_endent(be->db_rootp, be->db_initf,
					&be->db_context);
				be->state = GETENT_FILE;
				continue;
			}
			if (strset_in(&be->minuses, (*be->getnamef)(args))) {
				continue;
			}
			name = 0; /* tell code below we've done the lookup */
			break;

		    case GETENT_NETGROUP:
			linelen = be->linelen;
			if (!netgr_next_u(be, &name)) {
				netgr_end(be);
				be->state = GETENT_FILE;
				continue;
			}
			/* pass "name" variable to code below... */
			break;
		}

		if (name != 0) {
			if (strset_in(&be->minuses, name)) {
				continue;
			}
			/*
			 * Do a getXXXnam(name).  If we were being pure,
			 *   we'd introduce yet another function-pointer
			 *   that the database-specific code had to supply
			 *   to us.  Instead we'll be grotty and hard-code
			 *   the knowledge that
			 *	(a) The username is always passwd in key.name,
			 *	(b) NSS_DBOP_PASSWD_BYNAME ==
			 *		NSS_DBOP_SHADOW_BYNAME ==
			 *		NSS_DBOP_next_iter.
			 */
			savename = args->key.name;
			args->key.name	= name;
			args->returnval	= 0;
			nss_search(be->db_rootp, be->db_initf,
				NSS_DBOP_next_iter, args);
			args->key.name = savename;  /* In case anyone cares */
		}
		/*
		 * Found one via "+", "+name" or "@netgroup".
		 * Override some fields if the /etc file says to do so.
		 */
		if (args->returnval == 0) {
			/* ==> ?? Should treat erange differently? */
			continue;
		}
		/* 'colon' was set umpteen iterations ago in GETENT_FILE */
		if (colon != 0) {
			*colon = ':';
			colon = 0;
		}
		return (do_merge(be, args, instr, linelen));
	}
}

/* We don't use this directly;  we just copy the bits when we want to	 */
/* initialize the variable (in the compat_backend struct) that we do use */
static DEFINE_NSS_GETENT(context_initval);

nss_backend_t *
_nss_compat_constr(ops, n_ops, filename, min_bufsize, rootp, initf, netgroups,
		getname_func, merge_func)
	compat_backend_op_t	ops[];
	int			n_ops;
	const char		*filename;
	int			min_bufsize;
	nss_db_root_t		*rootp;
	nss_db_initf_t		initf;
	int			netgroups;
	compat_get_name		getname_func;
	compat_merge_func	merge_func;
{
	compat_backend_ptr_t	be;

	if ((be = (compat_backend_ptr_t) malloc(sizeof (*be))) == 0) {
		return (0);
	}
	be->ops		= ops;
	be->n_ops	= n_ops;
	be->filename	= filename;
	be->f		= 0;
	be->minbuf	= min_bufsize;
	be->buf		= 0;

	be->db_rootp	= rootp;
	be->db_initf	= initf;
	be->db_context	= context_initval;

	be->getnamef	= getname_func;
	be->mergef	= merge_func;

	be->state	= GETENT_FILE;	/* i.e. do Automatic setent(); */
	be->minuses	= 0;

	be->permit_netgroups = netgroups;
	be->yp_domain	= 0;
	be->getnetgrent_backend	= 0;
	be->netgr_buffer = 0;

	return ((nss_backend_t *) be);
}
