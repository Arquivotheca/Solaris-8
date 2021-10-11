/*
 *	getgrent.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 * lib/nsswitch/compat/getgrent.c -- name-service-switch backend for getgrnam()
 *   et al that does 4.x compatibility.  It looks in /etc/group; if it finds
 *   group entries there that begin with "+" or "-", it consults other
 *   services.  By default it uses NIS (YP), but the user can override this
 *   with a "group_compat" entry in /etc/nsswitch.conf, e.g.
 *			group_compat: nisplus
 *
 * This code tries to produce the same results as the 4.x code, even when
 *   the latter seems ill thought-out.  Bug-compatible, in other words.
 *   Though we do try to be more reasonable about the format of "+" and "-"
 *   entries here, i.e. you don't have to pad them with spurious colons and
 *   bogus uid/gid values.
 *
 * Caveats:
 *    -	More than one source may be specified, with the usual switch semantics,
 *	but having multiple sources here is definitely odd.
 *    -	People who recursively specify "compat" deserve what they get.
 */

#pragma ident	"@(#)getgrent.c	1.12	97/08/12 SMI"

#include <grp.h>
#include <stdlib.h>
#include <unistd.h>		/* for GF_PATH */
#include <strings.h>
#include "compat_common.h"

static DEFINE_NSS_DB_ROOT(db_root);

void
_nss_initf_group_compat(p)
	nss_db_params_t	*p;
{
	p->name		  = NSS_DBNAM_GROUP;
	p->config_name	  = NSS_DBNAM_GROUP_COMPAT;
	p->default_config = NSS_DEFCONF_GROUP_COMPAT;
}

static const char *
get_grname(argp)
	nss_XbyY_args_t		*argp;
{
	struct group		*g = (struct group *)argp->returnval;

	return (g->gr_name);
}

static int
check_grname(argp)
	nss_XbyY_args_t		*argp;
{
	struct group		*g = (struct group *)argp->returnval;

	return (strcmp(g->gr_name, argp->key.name) == 0);
}

static nss_status_t
getbyname(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_compat_XY_all(be, argp, check_grname,
				NSS_DBOP_GROUP_BYNAME));
}

static int
check_grgid(argp)
	nss_XbyY_args_t		*argp;
{
	struct group		*g = (struct group *)argp->returnval;

	return (g->gr_gid == argp->key.gid);
}

static nss_status_t
getbygid(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	nss_XbyY_args_t		*argp = (nss_XbyY_args_t *) a;

	return (_nss_compat_XY_all(be, argp, check_grgid,
				NSS_DBOP_GROUP_BYGID));
}

static nss_status_t
getbymember(be, a)
	compat_backend_ptr_t	be;
	void			*a;
{
	struct nss_groupsbymem	*argp = (struct nss_groupsbymem *) a;
	int			numgids = argp->numgids;
	int			maxgids = argp->maxgids;
	gid_t			*gid_array = argp->gid_array;
	struct nss_XbyY_args	grargs;
	struct group		g;
	char			*buffer;

	/*
	 * Generic implementation:  enumerate using getent(), then check each
	 *   group returned by getent() to see whether it contains the user.
	 *   There are much faster ways, but at least this one gets the right
	 *   answer.
	 */
	if (numgids >= maxgids) {
		/* full gid_array;  nobody should have bothered to call us */
		return (NSS_SUCCESS);
	}
	if ((buffer = (void *)malloc(NSS_BUFLEN_GROUP)) == 0) {
		return (NSS_UNAVAIL);
	}
	NSS_XbyY_INIT(&grargs, &g, buffer, NSS_BUFLEN_GROUP, argp->str2ent);

	_nss_compat_setent(be, 0);
	while (_nss_compat_getent(be, &grargs) == NSS_SUCCESS) {
		char		**mem;

		if (grargs.returnval == 0) {
			continue;
		}
		for (mem = g.gr_mem;  *mem != 0;  mem++) {
			if (strcmp(*mem, argp->username) == 0) {
				int	gid = g.gr_gid;
				int	i;
				for (i = 0;  i < numgids;  i++) {
					if (gid == gid_array[i]) {
						break;
					}
				}
				if (i == numgids) {
					gid_array[numgids++] = gid;
					argp->numgids = numgids;
					if (numgids >= maxgids) {
						/* filled the gid_array */
						_nss_compat_endent(be, 0);
						return (NSS_SUCCESS);
					}
					/* Done with this group, try next */
					break;
				}
			}
		}
	}
	_nss_compat_endent(be, 0);
	return (NSS_NOTFOUND);	/* Really means "gid_array not full yet" */
}

/*ARGSUSED*/
static int
merge_grents(be, argp, fields)
	compat_backend_ptr_t	be;
	nss_XbyY_args_t		*argp;
	const char		**fields;
{
	struct group		*g	= (struct group *)argp->buf.result;
	char			*buf;
	char			*s;
	int			parsestat;

	/*
	 * We're allowed to override the passwd (has anyone ever actually used
	 *   the passwd in a group entry?) and the membership list, but not
	 *   the groupname or the gid.
	 * That's what the SunOS 4.x code did;  who are we to question it...
	 *
	 * Efficiency is heartlessly abandoned in the quest for simplicity.
	 */
	if (fields[1] == 0 && fields[3] == 0) {
		/* No legal overrides, leave *argp unscathed */
		return (NSS_STR_PARSE_SUCCESS);
	}
	if ((buf = malloc(NSS_LINELEN_GROUP)) == 0) {
		return (NSS_STR_PARSE_PARSE);
		/* Really "out of memory", but PARSE_PARSE will have to do */
	}
	s = buf;
	sprintf(s, "%s:%s:%d:",
		g->gr_name,
		fields[1] != 0 ? fields[1] : g->gr_passwd,
		g->gr_gid);
	s += strlen(s);
	if (fields[3] != 0) {
		strcpy(s, fields[3]);
		s += strlen(s);
	} else {
		char	**memp;

		for (memp = g->gr_mem;  *memp != 0;  memp++) {
			size_t	len = strlen(*memp);
			if (s + len + 1 <= buf + NSS_LINELEN_GROUP) {
				if (memp != g->gr_mem) {
					*s++ = ',';
				}
				memcpy(s, *memp, len);
				s += len;
			} else {
				free(buf);
				return (NSS_STR_PARSE_ERANGE);
			}
		}
	}
	parsestat = (*argp->str2ent)(buf, s - buf,
				    argp->buf.result,
				    argp->buf.buffer,
				    argp->buf.buflen);
	free(buf);
	return (parsestat);
}

static compat_backend_op_t group_ops[] = {
	_nss_compat_destr,
	_nss_compat_endent,
	_nss_compat_setent,
	_nss_compat_getent,
	getbyname,
	getbygid,
	getbymember
};

/*ARGSUSED*/
nss_backend_t *
_nss_compat_group_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_compat_constr(group_ops,
				sizeof (group_ops) / sizeof (group_ops[0]),
				GF_PATH,
				NSS_LINELEN_GROUP,
				&db_root,
				_nss_initf_group_compat,
				0,
				get_grname,
				merge_grents));
}
