/*
 * Copyright (c) 1993-94,1999 by Sun Microsystems, Inc.
 */

/*
 * nss_netdir.h
 *
 * Defines structures that are shared between the OSNET-private
 * _get_hostserv_inetnetdir_byYY() interfaces and the public
 * interfaces gethostbyYY()/getservbyYY() and netdir_getbyYY().
 * Ideally, this header file should never be visible to developers
 * outside of the OSNET build.
 */

#ifndef _NSS_NETDIR_H
#define	_NSS_NETDIR_H

#pragma ident	"@(#)nss_netdir.h	1.9	99/04/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	NSS_HOST,
	NSS_SERV,
	NETDIR_BY,
	NETDIR_BY_NOSRV,		/* bypass service lookup */
	NETDIR_BY6,
	NETDIR_BY_NOSRV6,		/* bypass service lookup */
	NSS_HOST6
} nss_netdir_op_t;

struct nss_netdirbyname_in {
	nss_netdir_op_t	op_t;
	union {
		struct nd_hostserv *nd_hs;
		union {
			struct {
				const char	*name;
				char	*buf;
				int	buflen;
			} host;
			struct {
				const char	*name;
				const char	*proto;
				char	*buf;
				int	buflen;
			} serv;
		} nss;
	} arg;
};

union nss_netdirbyname_out {
	struct nd_addrlist **nd_alist;
	union {
		struct {
			struct hostent *hent;
			int	*herrno_p;
		} host;
		struct servent *serv;
	} nss;
};

struct nss_netdirbyaddr_in {
	nss_netdir_op_t	op_t;
	union {
		struct netbuf *nd_nbuf;
		union {
			struct {
				const char	*addr;
				int	len;
				int	type;
				char	*buf;
				int	buflen;
			} host;
			struct {
				int	port;
				const char	*proto;
				char	*buf;
				int	buflen;
			} serv;
		} nss;
	} arg;
};

union nss_netdirbyaddr_out {
	struct nd_hostservlist **nd_hslist;
	union {
		struct {
			struct hostent *hent;
			int	*herrno_p;
		} host;
		struct servent *serv;
	} nss;
};

#ifdef __STDC__

int __classic_netdir_getbyname(struct netconfig *,
		struct nd_hostserv *, struct nd_addrlist **);
int __classic_netdir_getbyaddr(struct netconfig *,
		struct nd_hostservlist **, struct netbuf *);
int _get_hostserv_inetnetdir_byname(struct netconfig *,
		struct nss_netdirbyname_in *, union nss_netdirbyname_out *);
int _get_hostserv_inetnetdir_byaddr(struct netconfig *,
		struct nss_netdirbyaddr_in *, union nss_netdirbyaddr_out *);
int __inet_netdir_options(struct netconfig *,
		int option, int fd, char *par);
struct netbuf *__inet_uaddr2taddr(struct netconfig *, char *);
char *__inet_taddr2uaddr(struct netconfig *, struct netbuf *);

#else

int __classic_netdir_getbyname();
int __classic_netdir_getbyaddr();
int _get_hostserv_inetnetdir_byname();
int _get_hostserv_inetnetdir_byaddr();
int __inet_netdir_options();
struct netbuf *__inet_uaddr2taddr();
char *__inet_taddr2uaddr();

#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif /* _NSS_NETDIR_H */
