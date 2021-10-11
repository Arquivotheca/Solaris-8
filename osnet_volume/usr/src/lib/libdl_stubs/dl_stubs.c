/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 *
 * Stub _dl*() routines: hardcoded for the nametoaddr libraries: straddr;
 * and switch libraries nss_files, nss_nis, nss_nisplus,
 * nss_compat. This is a hack that may be retained forever, to explicitly
 * allow building commands statically, like rcp, su, sulogin, ufsrestore etc.
 * and also as an easy way to debug these zillions of shared objects without
 * using dynamic linking (i.e. for doing "static dynamic network selection"TM).
 *
 * The dlopen() call returns an integer cookie to identify the intended
 * backend in the following dlsym() calls. By default, it expects to be
 * linked with -lstraddr, -lnss_files, -lnss_nis,
 * -lnss_nisplus, -lnss_compat and, of course, -lnsl -lsocket and -lc, all
 * statically with the -dn option. If linked for additional services with
 * the appropriate -D<name>, e.g. OSI, the corresponding
 * static library should be linked with the application.
 *
 *	The library names expected are:
 *
 *	/usr/lib/straddr.so,
 *
 *	Each of these <name>.so, must have an associated lib<name>.a that has
 *	all the <name>_netdir_*() routines defined in it and the -l<name> mus
 *	be statically linked with the application.
 *
 *	/usr/lib/nss_files.so.1
 *	/usr/lib/nss_nis.so.1
 *	/usr/lib/nss_nisplus.so.1
 *	/usr/lib/nss_compat.so.1	group, passwd and shadow only
 *	/usr/lib/nss_dns.so.1		hosts only
 *
 *	Each of these <name>.so.1 must have an associated lib<name>.a that has
 *	_nss_<name>_<database>_constr() routines defined in it;  this library
 *	must be included in the arguments to the linker ("-l<name>") for
 *	every statically-linked application.
 *
 *	Note that the (MT-unsafe) version of the name-service switch that
 *	shipped in Solaris 2.0 used libraries with different names
 *	("/usr/lib/nsw<name>.so") and a different subroutine interface.
 */

#pragma ident	"@(#)dl_stubs.c	1.15	94/03/24 SMI"

#include <stdio.h>
#include <string.h>
#include <nss_dbdefs.h>

/*
 * arbitrarily high illegal fd's, only for consumption by
 * _dlsym() in libdl_stubs.a.
 */

/* for nametoaddr libs in network selection */
#define	STRFD		1001
#define	OSIFD		1007

/* for libc->libnsl access */
#define	NSLFD		2000
#define	SOCKFD		3000

/* for the switch shared objects */
#define	NSS_FILESFD	4000
#define	NSS_NISFD	4001
#define	NSS_NISPLUSFD	4002
#define	NSS_COMPATFD	4003
#define	NSS_DNSFD	4004
#define	NUM_NSS_LIBS	5

/* loopback routines */
extern struct nd_addrlist 	*str_netdir_getbyname();
extern struct nd_hostservlist 	*str_netdir_getbyaddr();
extern char			*str_taddr2uaddr();
extern struct netbuf		*str_uaddr2taddr();
extern int			str_netdir_options();

#ifdef	OSI
extern struct nd_addrlist 	*osi_netdir_getbyname();
extern struct nd_hostservlist 	*osi_netdir_getbyaddr();
extern char			*osi_taddr2uaddr();
extern struct netbuf		*osi_uaddr2taddr();
extern int			osi_netdir_options();
#endif

/* NIS routines called from yp_stubs in libc */
#ifdef NSL
extern int yp_get_default_domain();
extern int yp_first();
extern int yp_next();
extern int yp_match();
#endif

struct nssgrp {
	char	*db;
	int	db_len;
	void	*syms[NUM_NSS_LIBS];
};

#define	NSSGRP(macname, symname) {			\
	NSS_DBNAM_##macname,				\
	sizeof (NSS_DBNAM_##macname) - 1,		\
	(void *)_nss_files_##symname##_constr,		\
	(void *)_nss_nis_##symname##_constr,		\
	(void *)_nss_nisplus_##symname##_constr,	\
	0, 0						\
}
#define	NSSGRP_COMPAT(macname, symname) {		\
	NSS_DBNAM_##macname,			\
	sizeof (NSS_DBNAM_##macname) - 1,	\
	0, 0, 0,					\
	(void *)_nss_compat_##symname##_constr,		\
	0						\
}
#define	NSSGRP_DNS(macname, symname) {			\
	NSS_DBNAM_##macname,				\
	sizeof (NSS_DBNAM_##macname) - 1,		\
	0, 0, 0, 0,					\
	(void *)_nss_dns_##symname##_constr		\
}

#define	NSSEXT(symname)		extern nss_backend_t	\
	*_nss_files_##symname##_constr(),		\
	*_nss_nis_##symname##_constr(),			\
	*_nss_nisplus_##symname##_constr()
#define	NSSEXT_COMPAT(symname)		extern nss_backend_t	\
	*_nss_compat_##symname##_constr()
#define	NSSEXT_DNS(symname)		extern nss_backend_t	\
	*_nss_dns_##symname##_constr()


NSSEXT(passwd);
NSSEXT(group);
NSSEXT(shadow);
NSSEXT_COMPAT(passwd);
NSSEXT_COMPAT(group);
NSSEXT_COMPAT(shadow);
NSSEXT(hosts);
NSSEXT_DNS(hosts);
NSSEXT(rpc);
NSSEXT(services);
NSSEXT(networks);
NSSEXT(protocols);
NSSEXT(ethers);
NSSEXT(bootparams);
NSSEXT(netgroup);
/* NSSEXT(publickey); */
/* NSSEXT(netmasks); */
/* NSSEXT(automount); */
/* NSSEXT(aliases); */

struct nssgrp nss_syms[] = {
	NSSGRP(PASSWD,	passwd),
	NSSGRP_COMPAT(PASSWD,	passwd),
	NSSGRP(GROUP,	group),
	NSSGRP_COMPAT(GROUP,	group),
	NSSGRP(SHADOW,	shadow),
	NSSGRP_COMPAT(PASSWD,	shadow),	/* sic (or rather, sick?) */
	NSSGRP(HOSTS,	hosts),
	NSSGRP_DNS(HOSTS,	hosts),
	NSSGRP(RPC,	rpc),
	NSSGRP(SERVICES, services)
	/* ==== Add references when backends are implemented */
};

#pragma weak dlopen = _dlopen
#pragma weak dlclose = _dlclose
#pragma weak dlsym = _dlsym
#pragma weak dlerror = _dlerror

int
_dlopen(path, mode)
	char *path; int mode;
{
#ifdef	DEBUG
	fprintf(stderr, "***** STUB DLOPEN CALLED for %s *****\n", path);
#endif
	if (strstr(path, "nss_")) {
		if (strstr(path, "nss_files.so.1"))
			return (NSS_FILESFD);
		else if (strstr(path, "nss_compat.so.1"))
			return (NSS_COMPATFD);
		else if (strstr(path, "nss_nis.so.1"))
			return (NSS_NISFD);
		else if (strstr(path, "nss_nisplus.so.1"))
			return (NSS_NISPLUSFD);
		else if (strstr(path, "nss_dns.so.1"))
			return (NSS_DNSFD);
		else
			return (0);
	}
	if (strstr(path, "straddr.so"))
		return (STRFD);

#ifdef NSL
	else if (strstr(path, "libnsl.so"))
		return (NSLFD);
#endif
	else if (strstr(path, "libsocket.so"))
		return (SOCKFD);	/* just to avoid dlerror; */
					/* will not be used for dlsym */
#ifdef OSI
	else if (strstr(path, "osi.so"))
		return (OSIFD);
#endif
	return (0);	/* return "failed" */
}

void *
_dlsym(fd, symbol)
	int fd; char *symbol;
{
	/* fd is the dummy one returned by _dlopen above */
#ifdef	DEBUG
	fprintf(stderr, "***** STUB DLSYM CALLED for fd %d sym %s  *****\n",
		fd, symbol);
#endif

	if (strncmp(symbol, "_nss_", 5) == 0) {
		char	*p, *q;
		int	len, i;

		/*
		 * Symbols are of the form
		 *		"_nss_"<source>"_"<database>"_constr"
		 * We don't bother checking that the <source> matches the
		 * pseudo-fd, just go ahead and look at the <database>.
		 */
		p = strchr(symbol + 5, '_');
		if (p == NULL) {
			return (NULL);
		}
		p++;
		q = strchr(p, '_');
		if (q == NULL) {
			return (NULL);
		}
		len = q - p;
		for (i = 0; i < sizeof (nss_syms) / sizeof (nss_syms[0]); i++) {
		    struct nssgrp *grp = nss_syms + i;

		    if (len == grp->db_len && strncmp(p, grp->db, len) == 0) {
			    switch (fd) {
				case NSS_FILESFD:
				case NSS_NISFD:
				case NSS_COMPATFD:
				case NSS_NISPLUSFD:
				case NSS_DNSFD:
				    if (grp->syms[fd - NSS_FILESFD] == 0)
					continue;
				    else {
#ifdef	DEBUG
	fprintf(stderr, "***** STUB DLSYM RETURNING SWITCH %s  %08x*****\n",
		grp->db, grp->syms[fd - NSS_FILESFD]);
#endif
					return (grp->syms[fd - NSS_FILESFD]);
				    }
				    break;
				default:
				    return (NULL);
			    }
		    }
		}
		return (NULL);
	}

	if (strcmp(symbol, "_netdir_getbyname") == 0) {
		switch (fd) {
		case STRFD:
			return ((void *) str_netdir_getbyname);
#ifdef OSI
		case OSIFD:
			return ((void *) osi_netdir_getbyname);
#endif
		default:
			return (NULL);
		}
	} else if (strcmp(symbol, "_netdir_getbyaddr") == 0) {
		switch (fd) {
		case STRFD:
			return ((void *) str_netdir_getbyaddr);
#ifdef OSI
		case OSIFD:
			return ((void *) osi_netdir_getbyaddr);
#endif
		default:
			return (NULL);
		}
	} else if (strcmp(symbol, "_netdir_options") == 0) {
		switch (fd) {
		case STRFD:
			return ((void *) str_netdir_options);
#ifdef OSI
		case OSIFD:
			return ((void *) osi_netdir_options);
#endif
		default:
			return (NULL);
		}
	} else if (strcmp(symbol, "_taddr2uaddr") == 0) {
		switch (fd) {
		case STRFD:
			return ((void *) str_taddr2uaddr);
#ifdef OSI
		case OSIFD:
			return ((void *) osi_taddr2uaddr);
#endif
		default:
			return (NULL);
		}
	} else if (strcmp(symbol, "_uaddr2taddr") == 0) {
		switch (fd) {
		case STRFD:
			return ((void *) str_uaddr2taddr);
#ifdef OSI
		case OSIFD:
			return ((void *) osi_uaddr2taddr);
#endif
		default:
			return (NULL);
		}
	}
#ifdef NSL
	else if (strcmp(symbol, "yp_get_default_domain") == 0)
		return ((void *) yp_get_default_domain);
	else if (strcmp(symbol, "yp_first") == 0)
		return ((void *) yp_first);
	else if (strcmp(symbol, "yp_next") == 0)
		return ((void *) yp_next);
	else if (strcmp(symbol, "yp_match") == 0)
		return ((void *) yp_match);
#endif
	return ((void *) NULL);
}

char *
_dlerror()
{
#ifdef	DEBUG
	fprintf(stderr, "***** STUB DLERROR CALLED *****\n");
#endif
	return ("DLERROR STUB ERROR");
}

int
_dlclose(handle)
	void *handle;
{
#ifdef	DEBUG
	fprintf(stderr, "***** STUB DLCLOSE CALLED *****\n");
#endif
	return (0);	/* successful close */
}
