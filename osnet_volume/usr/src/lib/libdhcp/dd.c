/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd.c	1.25	99/03/31 SMI"

/*
 * This module contains the functions presented as the interface to the
 * table library.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/systeminfo.h>
#include <dd_impl.h>

/*
 * Free the contents of a table structure.
 */
void
free_dd(Tbl *tbl)
{
	int i;

	for (i = 0; i < tbl->rows; ++i) {
		if (tbl->ra[i] != NULL)
			_dd_free_row(tbl->ra[i]);
	}
	(void) free(tbl->ra);
	tbl->rows = 0;
	tbl->ra = NULL;
}

/*
 * Free the contents of a table stat structure.
 */
void
free_dd_stat(Tbl_stat *tbl_st)
{
	if (tbl_st != NULL) {
		if (tbl_st->name != (char *)NULL)
			(void) free(tbl_st->name);
		if (tbl_st->ns == TBL_NS_NISPLUS) {
			if (tbl_st->perm.nis.owner_user != NULL)
				(void) free(tbl_st->perm.nis.owner_user);
			if (tbl_st->perm.nis.owner_group != NULL)
				(void) free(tbl_st->perm.nis.owner_group);
		}
		(void) free(tbl_st);
	}
}

/*
 * List the contents of a table. Caller is responsible for freeing
 * resultant Tbl structure.
 */
int
list_dd(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error value */
	Tbl *tbl,		/* Structure to return data in */
	...)
{
	va_list			ap;
	register char		*dbp, *np;
	struct tbl_trans_data	*ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;
	char			*args[TBL_MAX_ARGS] = { NULL };
	int			argno;
	char			*pathp = NULL;
	char			buf[MAXPATHLEN];

	va_start(ap, tbl);
	for (argno = 0; argno < ttp->search_args; ++argno)
		args[argno] = va_arg(ap, char *);
	va_end(ap);

	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		return (_list_dd_ufs(dbp, tbl_err, tbl, ttp, args));
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		return (_list_dd_nisplus(name, dbp, tbl_err, tbl, ttp, args));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (TBL_FAILURE);
	}
}

/*
 * Create a new table.
 */
int
make_dd(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error return value */
	char *user,		/* Optional name of owning user */
	char *group)		/* Optional name of owning group */
{
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;
	struct tbl_make_data *tmp = _dd_tmd[ti];
	register char *dbp, *np;
	char *pathp = NULL;
	char buf[MAXPATHLEN];

	switch (ti) {
	case TBL_DHCPIP:
	case TBL_DHCPTAB:
		break;
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_UNSUPPORTED_FUNC;
		return (TBL_FAILURE);
	}

	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE) {
			if (*tbl_err == TBL_STAT_ERROR) {
				/*
				 * This is a horrendous hack; just do the
				 * dd_ns call again and it will succeed.
				 * We do this because we half-expect the
				 * stat failure and assume that the caller
				 * won't mind the side-effect of the UFS
				 * version creating its needed directories
				 * as it goes.  This really ought to get
				 * fixed more cleanly.
				 */
				ns = dd_ns(tbl_err, &pathp);
			} else {
				return (TBL_FAILURE);
			}
		}
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		return (_make_dd_ufs(dbp, tbl_err, ttp, user, group));
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		return (_make_dd_nisplus(name, dbp, tbl_err, ttp, tmp, user,
		    group));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (TBL_FAILURE);
	}
}

/*
 * Remove an existing table.
 */
int
del_dd(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err)		/* Error return value */
{
	register char *dbp, *np;
	char *pathp = NULL;
	char buf[MAXPATHLEN];
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;

	switch (ti) {
	case TBL_DHCPIP:
	case TBL_DHCPTAB:
		break;
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_UNSUPPORTED_FUNC;
		return (TBL_FAILURE);
	}

	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		return (_del_dd_ufs(dbp, tbl_err, ttp));
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		return (_del_dd_nisplus(name, dbp, tbl_err, ttp));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (TBL_FAILURE);
	}
}

/*
 * Verify existence of a table and return stats.
 */
int
stat_dd(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error return value */
	Tbl_stat **tbl_st)	/* Table stats return structure */
{
	register char *dbp, *np;
	char *pathp = NULL;
	char buf[MAXPATHLEN];
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;

	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		return (_stat_dd_ufs(dbp, tbl_err, ttp, tbl_st));
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		return (_stat_dd_nisplus(name, dbp, tbl_err, ttp, tbl_st));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (TBL_FAILURE);
	}
}

/*
 * Add an entry to a table.
 */
int
add_dd_entry(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error return value */
	...)
{
	va_list ap;
	register int	err = TBL_FAILURE;
	register char *dbp, *np;
	char *pathp = NULL;
	char buf[MAXPATHLEN];
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;
	char *args[TBL_MAX_ARGS] = { NULL };
	int argno;

	va_start(ap, tbl_err);
	for (argno = 0; argno < ttp->args; ++argno) {
		args[argno] = va_arg(ap, char *);
		if ((args[argno] != NULL) && args[argno][0] &&
		    !_dd_validate(ttp->av[argno].valid_function,
		    args[argno])) {
			if (*tbl_err != NULL)
				*tbl_err = ttp->av[argno].err_index;
			return (TBL_FAILURE);
		}
	}
	va_end(ap);
	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		err = _add_dd_ufs(dbp, tbl_err, ttp, args);
		if (ti == TBL_HOSTS && err == TBL_SUCCESS) {
			/* Create host context in xfn... */
			(void) _dd_update_hosts_context(NULL, args[1]);
		}
		break;
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		err = _add_dd_nisplus(name, dbp, tbl_err, ttp, args);
		if (ti == TBL_HOSTS && err == TBL_SUCCESS) {
			/* Create host context in xfn... */
			(void) _dd_update_hosts_context(dbp, args[1]);
		}
		break;
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		err = TBL_FAILURE;
	}

	return (err);
}

/*
 * Modify an existing table entry.
 */
int
mod_dd_entry(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error return valid */
	...)
{
	va_list ap;
	register char *dbp, *np;
	char *pathp = NULL;
	char buf[MAXPATHLEN];
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;
	char *args[TBL_MAX_ARGS] = { NULL };
	int argno, dargno;

	va_start(ap, tbl_err);
	for (argno = 0; argno < ttp->search_args; ++argno)
		args[argno] = va_arg(ap, char *);
	for (dargno = 0; dargno < ttp->args; ++dargno) {
		args[argno+dargno] = va_arg(ap, char *);
		if ((args[argno+dargno] != NULL) && args[dargno+argno][0] &&
		    !_dd_validate(ttp->av[dargno].valid_function,
		    args[argno+dargno])) {
			if (tbl_err != NULL)
				*tbl_err = ttp->av[dargno].err_index;
			return (TBL_FAILURE);
		}
	}
	va_end(ap);
	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		return (_mod_dd_ufs(dbp, tbl_err, ttp, args));
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		return (_mod_dd_nisplus(name, dbp, tbl_err, ttp, args));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (TBL_FAILURE);
	}
}

/*
 * Remove an entry from a table.
 */
int
rm_dd_entry(
	uint_t ti,		/* Index into table format array */
	int ns,			/* Nameservice index */
	char *name,		/* Override default table name */
	char *domain,		/* Nameservice domain */
	int *tbl_err,		/* Error return value */
	...)
{
	va_list ap;
	register int err = TBL_FAILURE;
	register char *cp, *dbp, *np, *contextp = NULL, *hostp;
	char *pathp = NULL;
	struct in_addr	ip;
	struct hostent	*hp;
	char buf[MAXPATHLEN];
	char hostbuf[MAXNAMELEN];
	struct tbl_trans_data *ttp = _dd_ttd[ti];
	struct tbl_fmt		*tfp;
	char *args[TBL_MAX_ARGS] = { NULL };
	int argno;

	va_start(ap, tbl);
	for (argno = 0; argno < ttp->search_args; ++argno)
		args[argno] = va_arg(ap, char *);
	va_end(ap);
	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (TBL_FAILURE);
	}
	switch (ns) {
	case TBL_NS_UFS:
		if (pathp != NULL) {
			if (name != NULL)
				np = name;
			else {
				tfp = &ttp->fmts[TBL_NS_UFS];
				np = tfp->name;
			}
			if (ti != TBL_HOSTS) {
				(void) sprintf(buf, "%s/%s", pathp, np);
				dbp = buf;
			} else
				dbp = np;
		} else
			dbp = name;
		err = _rm_dd_ufs(dbp, tbl_err, ttp, args);
		break;
	case TBL_NS_NISPLUS:
		if (domain == NULL && pathp != NULL)
			dbp = pathp;
		else
			dbp = domain;
		contextp = dbp;
		err = _rm_dd_nisplus(name, dbp, tbl_err, ttp, args);
		break;
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		err = TBL_FAILURE;
		break;
	}

	if (ti == TBL_HOSTS && err == TBL_SUCCESS) {
		/* Destroy host context in xfn... */
		if (args[1] == NULL) {
			/* convert ip address to hostname */
			ip.s_addr = inet_addr(args[0]);
			if (ip.s_addr != (u_long)0xffffffff &&
			    (hp = gethostbyaddr((char *)&ip,
			    sizeof (struct in_addr), AF_INET)) != NULL) {
				(void) strcpy(hostbuf, hp->h_name);
				hostp = hostbuf;
			} else
				hostp = NULL;
		} else
			hostp = args[1];
		if (hostp != NULL) {
			if ((cp = strchr(hostp, '.')) != NULL)
				*cp = '\0';
			(void) _dd_destroy_hosts_context(contextp, hostp);
		}
	}

	return (err);
}

/*
 * Returns list of DHCP network table names in the specified nameservice.
 * List is NULL terminated.  NULL return indicates error in tbl_err.
 */
char **
dd_ls(
	int ns,			/* Nameservice index */
	char *domain,		/* Nameservice domain */
	int *tbl_err)		/* Error value */
{
	char *pathp = NULL;

	if (ns == TBL_NS_DFLT) {
		ns = dd_ns(tbl_err, &pathp);
		if (ns == TBL_FAILURE)
			return (NULL);
	}

	/* If user overrode path, use it */
	if (domain != NULL) {
		pathp = domain;
	}

	switch (ns) {
	case TBL_NS_UFS:
		return (_dd_ls_ufs(tbl_err, pathp));
	case TBL_NS_NISPLUS:
		return (_dd_ls_nisplus(tbl_err, pathp));
	default:
		if (tbl_err != NULL)
			*tbl_err = TBL_BAD_NS;
		return (NULL);
	}
}

/*
 * Determines from 'dhcp' the data resource used and path in that
 * resource to the dhcp databases, and returns these values in 'resource' and
 * 'path' respectively. Assumes resource and path are big enough.
 *
 * Defaults to resource='nisplus', path='current domain' if dhcp does
 * not exist.
 */
int
dd_ns(int *tbl_err, char **pathpp)
{
	register int	len;
	register char	*tp, *ep, *ap;
	static int	ns = TBL_NS_DFLT;
	static char	path[MAXPATHLEN];
	FILE		*fp;
	struct stat	statbuf;
	char		buffer[MAXNAMELEN];

	if (pathpp != NULL)
		*pathpp = path;

	if (ns != TBL_NS_DFLT) {
		return (ns);
	}

	if ((fp = fopen(TBL_NS_FILE, "r")) != NULL) {
		while ((ns == -1 || *path == '\0') &&
		    (tp = fgets(buffer, MAXNAMELEN, fp)) != NULL) {
			if (*tp == '#' || *tp == '\n')
				continue;
			while (isspace(*tp))
				tp++;
			len = strlen(tp);
			if (tp[len - 1] == '\n')
				tp[len - 1] = '\0';
			if ((ep = strchr(tp, '=')) == NULL) {
				if (tbl_err != NULL)
					*tbl_err = TBL_BAD_SYNTAX;
				continue;
			} else {
				for (ap = tp; ap < ep; ap++) {
					if (isspace(*ap))
						*ap = '\0';
				}
				*ep++ = '\0';
			}
			if (strcasecmp(tp, TBL_NS_RESOURCE) == 0) {
				while (isspace(*ep) && *ep != '\0')
					ep++;
				if (strcasecmp(ep, TBL_NS_UFS_STRING) == 0) {
					ns = TBL_NS_UFS;
					continue;
				}
				if (strcasecmp(ep,
				    TBL_NS_NISPLUS_STRING) == 0) {
					ns = TBL_NS_NISPLUS;
					continue;
				}
				if (tbl_err != NULL)
					*tbl_err = TBL_BAD_NS;
				(void) fclose(fp);
				return (TBL_FAILURE);
			} else if (strcasecmp(tp, TBL_NS_PATH) == 0) {
				while (isspace(*ep) && *ep != '\0')
					ep++;
				(void) strcpy(path, ep);
			} else {
				if (tbl_err != NULL)
					*tbl_err = TBL_BAD_DIRECTIVE;
			}
		}

		/*
		 * Validate and/or set to defaults.
		 */
		switch (ns) {
		case TBL_NS_UFS:
			if (*path == '\0')
				(void) strcpy(path, TBL_DHCP_DB);
			else {
				if (stat(path, &statbuf) < 0) {
					if (tbl_err != NULL)
						*tbl_err = TBL_STAT_ERROR;
					(void) fclose(fp);
					return (TBL_FAILURE);
				}
			}
			break;
		case TBL_NS_NISPLUS:
			if (*path == '\0') {
				(void) sysinfo((int)SI_SRPC_DOMAIN, path,
				    (long)MAXPATHLEN);
			} else {
				if (!_dd_validate(TBL_VALID_DOMAINNAME,
				    path)) {
					if (tbl_err != NULL)
						*tbl_err = TBL_BAD_DOMAIN;
					(void) fclose(fp);
					return (TBL_FAILURE);
				}
				/*
				 * Set NIS_PATH if nisplus and path is not the
				 * default.
				 */
				(void) _set_nis_path(path);
			}
			break;
		default:
			if (tbl_err != NULL)
				*tbl_err = TBL_BAD_NS;
			(void) fclose(fp);
			return (TBL_FAILURE);
		}

		*pathpp = path;

		(void) fclose(fp);
	} else {
		if (tbl_err != NULL)
			*tbl_err = TBL_OPEN_ERROR;
		return (TBL_FAILURE);
	}

	if (tbl_err != NULL)
		*tbl_err = TBL_SUCCESS;

	return (ns);
}

/*
 * Produce appropriate per network database name given a network address and
 * a subnet mask. These in_addr's are expected in network order.
 */
void
build_dhcp_ipname(char *namep, struct in_addr *np, struct in_addr *mp)
{
	register u_long		addr;

	addr = ntohl(np->s_addr) & ntohl(mp->s_addr);
	(void) sprintf(namep, PN_NAME_FORMAT,
	((addr & 0xff000000) >> 24), ((addr & 0x00ff0000) >> 16),
	((addr & 0x0000ff00) >> 8), (addr & 0x000000ff));
}

/*
 * Given a table stat struct, determine in the current user has permission
 * to access the object.
 *
 * Returns 0 for success, nonzero if an error occurs. Sets errno appropriately.
 */
int
check_dd_access(Tbl_stat *tsp, int *errp)
{
	register int	err, i, numgids;
	register char	*urootp, *tp;
	uid_t		uid;
	struct passwd	*pwp;
	mode_t		files_testmode;
	u_long		nis_testmode;
	u_long		nis_rights;
	gid_t		gidlist[NGROUPS_MAX];
	char		dombuff[DOM_NM_LN];
	char		namebuff[MAXNETNAMELEN + 1];
	char		hostname[MAXHOSTNAMELEN + 1];

	if (tsp == NULL || errp == NULL)
		return (-1);

	uid = getuid();

	err = -1;

	switch (tsp->ns) {
	case TBL_NS_UFS:

		/* Check if "others" permissions grant access */
		files_testmode = (mode_t)(S_IROTH + S_IWOTH);
		if ((tsp->perm.ufs.mode & files_testmode) == files_testmode) {
			err = 0;
			break;
		}

		/* Check if user is owning user and access granted */
		files_testmode = (mode_t)((mode_t)(S_IROTH + S_IWOTH) << 6);
		if ((uid == tsp->perm.ufs.owner_uid) &&
		    ((tsp->perm.ufs.mode & files_testmode) == files_testmode)) {
			err = 0;
			break;
		}

		/* Get groups and check if member of owning group */
		if ((pwp = getpwuid(uid)) == (struct passwd *)NULL ||
		    initgroups(pwp->pw_name, pwp->pw_gid) != 0) {
			/* errno will be set */
			*errp = TBL_NO_USER;
			err = -1;
			break;
		}

		numgids = getgroups((int)NGROUPS_MAX, gidlist);
		files_testmode = (mode_t)((mode_t)(S_IROTH +
		    S_IWOTH) << 3);
		for (i = 0; i < numgids; i++) {
			if (tsp->perm.ufs.owner_gid == gidlist[i])
				if ((tsp->perm.ufs.mode & files_testmode) ==
				    files_testmode) {
					err = 0;
					break;
				}
		}

		if (err != 0)
			*errp = TBL_NO_ACCESS;
		break;

	case TBL_NS_NISPLUS:

		nis_rights = (u_long)(NIS_READ_ACC + NIS_MODIFY_ACC +
		    NIS_CREATE_ACC + NIS_DESTROY_ACC);

		/* Check if nobody permissions grant access */
		nis_testmode = (u_long)(nis_rights << 24);
		if ((tsp->perm.nis.mode & nis_testmode) == nis_testmode) {
			err = 0;
			break;
		}

		/* Check if world permissions grant access */
		nis_testmode = nis_rights;
		if ((tsp->perm.nis.mode & nis_testmode) == nis_testmode) {
			err = 0;
			break;
		}

		/*
		 * Check if this user is the owner of the object.
		 * If so, check if owner permissions grant access.
		 */

		/* build nis net name */
		if (uid == (uid_t)0) {
			(void) sysinfo((int)SI_HOSTNAME, hostname,
			    (long)(MAXHOSTNAMELEN + 1));
			if ((tp = strchr(hostname, '.')) != NULL)
				*tp = '\0';
			urootp = hostname;
		} else {
			if ((pwp = getpwuid(uid)) == NULL) {
				err = -1;
				break;
			} else
				urootp = pwp->pw_name;
		}

		(void) sysinfo((int)SI_SRPC_DOMAIN, dombuff, (long)DOM_NM_LN);
		(void) sprintf(namebuff, "%s.%s.", urootp, dombuff);
		if (strcmp(namebuff, tsp->perm.nis.owner_user) == 0) {
			nis_testmode = (mode_t)(nis_rights << 16);
			if ((tsp->perm.nis.mode & nis_testmode) ==
			    nis_testmode) {
				err = 0;
				break;
			}
		}

		/*
		 * Check if this user is in the owning NIS+ group of
		 * the object.
		 * If so, check if group permissions grant access.
		 */
		nis_testmode = (u_long)(nis_rights << 8);
		if ((tsp->perm.nis.mode & nis_testmode) != nis_testmode) {
			*errp = TBL_NO_ACCESS;
			err = -1;
			break;
		}

		if (nis_ismember(namebuff, tsp->perm.nis.owner_group)) {
			err = 0;
			break;
		}

		if (err != 0)
			*errp = TBL_NO_ACCESS;
	}

	return (err);
}
