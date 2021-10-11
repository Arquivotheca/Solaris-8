/*	Copyright (c) 1992 Sun Microsystems, Inc.	*/

/*
 * getnetgrent.c
 *
 *	- name-service switch frontend routines for the netgroup API.
 *
 * Policy decision:
 *	If netgroup A refers to netgroup B, both must occur in the same
 *	source (any other choice gives very confusing semantics).  This
 *	assumption is deeply embedded in the code below and in the backends.
 *
 * innetgr() is implemented on top of something called __multi_innetgr(),
 * which replaces each (char *) argument of innetgr() with a counted vector
 * of (char *).  The semantics are the same as an OR of the results of
 * innetgr() operations on each possible 4-tuple picked from the arguments,
 * but it's possible to implement some cases more efficiently.  This is
 * important for mountd, which used to read YP netgroup.byhost directly in
 * order to determine efficiently whether a given host belonged to any one
 * of a long list of netgroups.  Wildcarded arguments are indicated by a
 * count of zero.
 */

/*LINTLIBRARY*/
/* ====> Find out da rules on netgr functions and <synonyms.h> */
#if	0
/*
 * ==== The pre-switch versions of innetgr.c and getnetgrent.c didn't have
 *   this, and the functions aren't mentioned in libc/inc/synonyms.h;
 *   what's the deal?
 */
#ifdef	__STDC__
#pragma weak innetgr = _innetgr
#pragma weak setnetgrent = _setnetgrent
#pragma weak getnetgrent = _getnetgrent
#pragma weak endnetgrent = _endnetgrent
#endif	__STDC__
#endif	0

#include "synonyms.h"
#include "shlib.h"	/* <=== ? */
#include <string.h>
#include <synch.h>
/* #include <lib/mtlib.h> */	/* <=== */
#include <nss_dbdefs.h>
#include <mtlib.h>
#include <libc.h>

#ifdef	__STDC__
#define	Const	const
#else
#define	Const
#endif

static DEFINE_NSS_DB_ROOT(db_root);

void
_nss_initf_netgroup(p)
	nss_db_params_t	*p;
{
	p->name	= NSS_DBNAM_NETGROUP;
	p->default_config = NSS_DEFCONF_NETGROUP;
}

/*
 * The netgroup routines aren't quite like the majority of the switch clients.
 *   innetgr() more-or-less fits the getXXXbyYYY mould, but for the others:
 *	- setnetgrent("netgroup") is really a getXXXbyYYY routine, i.e. it
 *	  searches the sources until it finds an entry with the given name.
 *	  Rather than returning the (potentially large) entry, it simply
 *	  initializes a cursor, and then...
 *      - getnetgrent(...) is repeatedly invoked by the user to extract the
 *	  contents of the entry found by setnetgrent().
 *	- endnetgrent() is almost like a real endXXXent routine.
 * If we were certain that all the backends could provide netgroup information
 * in a common form, we could make the setnetgrent() backend return the entire
 * entry to the frontend, then implement getnetgrent() and endnetgrent()
 * strictly in the frontend (aka here).  But we're not certain, so we won't.
 *
 * NOTE:
 *	In the SunOS 4.x (YP) version of this code, innetgr() did not
 *	affect the state of {set,get,end}netgrent().  Somewhere out
 *	there probably lurks a program that depends on this behaviour,
 *	so this version (both frontend and backends) had better
 *	behave the same way.
 */

/* ===> ?? fix "__" name */
__multi_innetgr(ngroup,	pgroup,
		nhost,	phost,
		nuser,	puser,
		ndomain, pdomain)
	nss_innetgr_argc	ngroup, nhost, nuser, ndomain;
	nss_innetgr_argv	pgroup, phost, puser, pdomain;
{
	struct nss_innetgr_args	ia;

	if (ngroup == 0) {
		return (0);	/* One thing fewer to worry backends */
	}

	ia.groups.argc			= ngroup;
	ia.groups.argv			= pgroup;
	ia.arg[NSS_NETGR_MACHINE].argc	= nhost;
	ia.arg[NSS_NETGR_MACHINE].argv	= phost;
	ia.arg[NSS_NETGR_USER].argc	= nuser;
	ia.arg[NSS_NETGR_USER].argv	= puser;
	ia.arg[NSS_NETGR_DOMAIN].argc	= ndomain;
	ia.arg[NSS_NETGR_DOMAIN].argv	= pdomain;
	ia.status			= NSS_NETGR_NO;

	nss_search(&db_root, _nss_initf_netgroup, NSS_DBOP_NETGROUP_IN, &ia);
	return (ia.status == NSS_NETGR_FOUND);
}

innetgr(group, host, user, domain)
	Const char *group, *host, *user, *domain;
{
#define	IA(charp)	((charp) != 0), (nss_innetgr_argv)(&(charp))

	return (__multi_innetgr(IA(group), IA(host), IA(user), IA(domain)));
}

/*
 * Context for setnetgrent()/getnetgrent().  If the user is being sensible
 *   the requests will be serialized anyway, but let's play safe and
 *   serialize them ourselves (anything to prevent a coredump)...
 */
static mutex_t		backend_lock = DEFAULTMUTEX;
static nss_backend_t	*getnetgrent_backend;

int
setnetgrent(netgroup)
	Const char	*netgroup;
{
	nss_backend_t	*be;

	if (netgroup == 0) {
		/* Prevent coredump, otherwise don't do anything profound */
		netgroup = "";
	}

	mutex_lock(&backend_lock);
	be = getnetgrent_backend;
	if (be != 0 && NSS_INVOKE_DBOP(be, NSS_DBOP_SETENT,
	    (void *)netgroup) != NSS_SUCCESS) {
		NSS_INVOKE_DBOP(be, NSS_DBOP_DESTRUCTOR, 0);
		be = 0;
	}
	if (be == 0) {
		struct nss_setnetgrent_args	args;

		args.netgroup	= netgroup;
		args.iterator	= 0;
		nss_search(&db_root, _nss_initf_netgroup,
		    NSS_DBOP_NETGROUP_SET, &args);
		be = args.iterator;
	}
	getnetgrent_backend = be;
	mutex_unlock(&backend_lock);
	return (0);
}

int
getnetgrent_r(machinep, namep, domainp, buffer, buflen)
	char		**machinep;
	char		**namep;
	char		**domainp;
	char		*buffer;
	int		buflen;
{
	struct nss_getnetgrent_args	args;

	args.buffer	= buffer;
	args.buflen	= buflen;
	args.status	= NSS_NETGR_NO;

	mutex_lock(&backend_lock);
	if (getnetgrent_backend != 0) {
		NSS_INVOKE_DBOP(getnetgrent_backend, NSS_DBOP_GETENT, &args);
	}
	mutex_unlock(&backend_lock);

	if (args.status == NSS_NETGR_FOUND) {
		*machinep = args.retp[NSS_NETGR_MACHINE];
		*namep	  = args.retp[NSS_NETGR_USER];
		*domainp  = args.retp[NSS_NETGR_DOMAIN];
		return (1);
	} else {
		return (0);
	}
}

static nss_XbyY_buf_t *buf;

int
getnetgrent(machinep, namep, domainp)
	char		**machinep;
	char		**namep;
	char		**domainp;
{
	NSS_XbyY_ALLOC(&buf, 0, NSS_BUFLEN_NETGROUP);
	return (getnetgrent_r(machinep, namep, domainp,
	    buf->buffer, buf->buflen));
}

int
endnetgrent()
{
	mutex_lock(&backend_lock);
	if (getnetgrent_backend != 0) {
		NSS_INVOKE_DBOP(getnetgrent_backend, NSS_DBOP_DESTRUCTOR, 0);
		getnetgrent_backend = 0;
	}
	mutex_unlock(&backend_lock);
	nss_delete(&db_root);	/* === ? */
	NSS_XbyY_FREE(&buf);
	return (0);
}
