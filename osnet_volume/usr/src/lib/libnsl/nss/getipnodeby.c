/*
 * Copyright (c) 1999 by Sun Microsystems Inc.
 * All rights reserved.
 *
 * This file defines and implements the re-entrant getipnodebyname(),
 * getipnodebyaddr(), and freehostent() routines for IPv6. These routines
 * follow use the netdir_getbyYY() (see netdir_inet.c).
 *
 * lib/libnsl/nss/getipnodeby.c
 */

#pragma ident	"@(#)getipnodeby.c	1.5	99/09/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <stropts.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <nss_netdir.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netdir.h>
#include <thread.h>
#include <synch.h>
#include <fcntl.h>

static struct hostent *__mapv4tov6(struct hostent *he4, struct hostent *he6,
				nss_XbyY_buf_t *res, int mapped);
static struct hostent *__mappedtov4(struct hostent *he, int *extract_error);
static struct hostent *__filter_v4(struct hostent *he, int *filter_error);
static int __find_mapped(struct hostent *he, int find_both);
static nss_XbyY_buf_t *__IPv6_alloc(int bufsz);
static void __IPv6_cleanup(nss_XbyY_buf_t *bufp);
static int __ai_addrconfig(int af);


extern struct netconfig *__rpc_getconfip();

extern struct hostent *
_switch_gethostbyaddr_r(const char *addr, int length, int type,
	struct hostent *result, char *buffer, int buflen, int *h_errnop);

extern struct hostent *
_switch_getipnodebyname_r(const char *nam, struct hostent *result, char *buffer,
	int buflen, int *h_errnop);

extern struct hostent *
_switch_getipnodebyaddr_r(const char *addr, int length, int type,
	struct hostent *result, char *buffer, int buflen, int *h_errnop);

#ifdef PIC
struct hostent *
_uncached_getipnodebyname(const char *nam, struct hostent *result,
	char *buffer, int buflen, int *h_errnop)
{
	return
	(_switch_getipnodebyname_r(nam, result, buffer, buflen, h_errnop));
}

struct hostent *
_uncached_getipnodebyaddr(const char *addr, int length, int type,
	struct hostent *result, char *buffer, int buflen, int *h_errnop)
{
	if (type == AF_INET)
		return (_switch_gethostbyaddr_r(addr, length, type,
					result, buffer, buflen, h_errnop));
	else if (type == AF_INET6)
		return (_switch_getipnodebyaddr_r(addr, length, type,
					result, buffer, buflen, h_errnop));
}
#endif

/*
 * To handle the task of checking for configured interfaces, an
 * ioctl call on a socket has to be made for any getXbyY() call with
 * AI_ADDRCONFIG specified (which is part of AI_DEFAULT). The socket will
 * opened once (behind a mutex) and then any thread is free to use it for
 * the ioctl call. Since multiple threads can use the fd, the mutex only is
 * needed for initialization.
 */
static int _sfd = 0;
static mutex_t _sfd_lock = DEFAULTMUTEX;

struct hostent *
getipnodebyname(const char *name, int af, int flags, int *error_num)
{
	struct hostent *hp = NULL;
	nss_XbyY_buf_t *buf4 = NULL;
	nss_XbyY_buf_t *buf6 = NULL;
	nss_XbyY_buf_t *resbuf = NULL;
	struct netconfig *nconf;
	struct  nss_netdirbyname_in nssin;
	union   nss_netdirbyname_out nssout;
	int neterr;

	trace1(TR_getipnodebyname, 0);
	if ((nconf = __rpc_getconfip("udp")) == NULL &&
	    (nconf = __rpc_getconfip("tcp")) == NULL) {
		trace2(TR_getipnodebyname, 1, buflen);
		*error_num = NO_RECOVERY;
		return ((struct hostent *)NULL);
	}
	if ((buf6 = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
		*error_num = NO_RECOVERY;
		(void) freenetconfigent(nconf);
		return ((struct hostent *)NULL);
	}
	switch (af) {
	case AF_INET6:
		/*
		 * Handle case of literal address string in name.
		 */
		if (strstr(name, ":")) {
			if ((flags & AI_ADDRCONFIG) &&
			    (__ai_addrconfig(AF_INET6) < 1)) {
				__IPv6_cleanup(buf6);
				*error_num = HOST_NOT_FOUND;
				(void) freenetconfigent(nconf);
				return ((struct hostent *)NULL);
			}
			nssin.op_t = NSS_HOST6;
			nssin.arg.nss.host.name = name;
			nssin.arg.nss.host.buf = buf6->buffer;
			nssin.arg.nss.host.buflen = buf6->buflen;
			nssout.nss.host.hent = buf6->result;
			nssout.nss.host.herrno_p = error_num;
			neterr =
			_get_hostserv_inetnetdir_byname(nconf, &nssin, &nssout);
			(void) freenetconfigent(nconf);
			if (neterr != ND_OK) {
				trace2(TR_getipnodebyname, 2, buflen);
				__IPv6_cleanup(buf6);
				*error_num = HOST_NOT_FOUND;
				return ((struct hostent *)NULL);
			}
			trace2(TR_getipnodebyname, 3, buflen);
			free(buf6);
			return (nssout.nss.host.hent);
		} else if (inet_addr(name) != -1) {
			if ((flags & AI_ADDRCONFIG) &&
			    (__ai_addrconfig(AF_INET) < 1)) {
				__IPv6_cleanup(buf6);
				(void) freenetconfigent(nconf);
				*error_num = HOST_NOT_FOUND;
				return ((struct hostent *)NULL);
			}
			if (!(flags & AI_V4MAPPED)) {
				__IPv6_cleanup(buf6);
				(void) freenetconfigent(nconf);
				*error_num = HOST_NOT_FOUND;
				return ((struct hostent *)NULL);
			}
			if ((buf4 = __IPv6_alloc(NSS_BUFLEN_HOSTS)) == 0) {
				__IPv6_cleanup(buf6);
				(void) freenetconfigent(nconf);
				*error_num = NO_RECOVERY;
				return ((struct hostent *)NULL);
			}
			nssin.op_t = NSS_HOST;
			nssin.arg.nss.host.name = name;
			nssin.arg.nss.host.buf = buf4->buffer;
			nssin.arg.nss.host.buflen = buf4->buflen;
			nssout.nss.host.hent = buf4->result;
			nssout.nss.host.herrno_p = error_num;
			if ((neterr = _get_hostserv_inetnetdir_byname(nconf,
						&nssin, &nssout)) == ND_OK) {
				trace2(TR_getipnodebyname, 3, buflen);
				__mapv4tov6(buf4->result, 0, buf6, 1);
				(void) freenetconfigent(nconf);
				__IPv6_cleanup(buf4);
				hp = buf6->result;
				free(buf6);
				return (hp);
			} else {
				__IPv6_cleanup(buf6);
				__IPv6_cleanup(buf4);
				(void) freenetconfigent(nconf);
				*error_num = NO_RECOVERY;
				return (0);
			}
		}
		if ((flags == 0) || (flags == AI_ADDRCONFIG &&
					__ai_addrconfig(AF_INET6) > 0)) {
			nssin.op_t = NSS_HOST6;
			nssin.arg.nss.host.name = name;
			nssin.arg.nss.host.buf = buf6->buffer;
			nssin.arg.nss.host.buflen = buf6->buflen;
			nssout.nss.host.hent = buf6->result;
			nssout.nss.host.herrno_p = error_num;
			/*
			 * We pass in nconf and let the implementation of the
			 * long-named func decide whether to use the switch
			 * based on nc_nlookups.
			 */
			neterr =
			_get_hostserv_inetnetdir_byname(nconf, &nssin, &nssout);
			(void) freenetconfigent(nconf);
			if (neterr != ND_OK) {
				trace2(TR_getipnodebyname, 2, buflen);
				__IPv6_cleanup(buf6);
				*error_num = HOST_NOT_FOUND;
				return ((struct hostent *)NULL);
			}
			if ((hp = __filter_v4(buf6->result,
						error_num)) == NULL) {
			/* note error_num is either NO_ADDRESS or NO_RECOVERY */
				trace2(TR_getipnodebyname, 2, buflen);
				__IPv6_cleanup(buf6);
				return ((struct hostent *)NULL);
			}
			trace2(TR_getipnodebyname, 3, buflen);
			free(buf6);
			return (hp);
		}

		if (flags & AI_ALL) {
		/*
		 * Get all v6 AND v4(mapped) addresses. Check the AI_ADDRCONFIG
		 * flag to see if a v6 and a v4 must be configured.
		 */
			if (((flags & AI_ADDRCONFIG) &&
				(__ai_addrconfig(AF_INET6) > 0)) ||
				!(flags & AI_ADDRCONFIG)) {
				nssin.op_t = NSS_HOST6;
				nssin.arg.nss.host.name = name;
				nssin.arg.nss.host.buf = buf6->buffer;
				nssin.arg.nss.host.buflen = buf6->buflen;
				nssout.nss.host.hent = buf6->result;
				nssout.nss.host.herrno_p = error_num;
				if ((neterr = _get_hostserv_inetnetdir_byname(
					nconf, &nssin, &nssout)) != ND_OK) {
					trace2(TR_getipnodebyname, 3, buflen);
					__IPv6_cleanup(buf6);
					buf6 = NULL;
				}
			} else { /* AI_ADDRCONFIG set and no v6 interfaces */
				__IPv6_cleanup(buf6);
				buf6 = NULL;
			}
			/*
			 * Now get v4 if configured and AI_V4MAPPED set
			 */
			if (flags & AI_V4MAPPED) {
				if (((flags & AI_ADDRCONFIG) &&
					(__ai_addrconfig(AF_INET) > 0)) ||
					!(flags & AI_ADDRCONFIG)) {
					if (buf6 &&
						__find_mapped(buf6->result,
									0)) {
					/* map'd v4s are there, return */
						(void) freenetconfigent(nconf);
						hp = buf6->result;
						free(buf6);
						return (hp);
					}
					/* This is the failover case to hosts */
					if ((buf4 =
					__IPv6_alloc(NSS_BUFLEN_HOSTS)) ==
								0) {
						(void) freenetconfigent(nconf);
						__IPv6_cleanup(buf6);
						*error_num = NO_RECOVERY;
						return ((struct hostent *)NULL);
					}
					hp = gethostbyname_r(name,
						buf4->result, buf4->buffer,
						buf4->buflen, error_num);
				}
			} else {
			/*
			 * !AI_V4MAPPED or AI_ADDRCONFIG set & no v4 interfaces
			 * NOTE: we must filter out any v4mapped addrs if there
			 */
				(void) freenetconfigent(nconf);
				if (buf6) { /* if v6 found, return only them */
					if ((hp = __filter_v4(buf6->result,
							error_num)) == NULL) {
						__IPv6_cleanup(buf6);
						return ((struct hostent *)NULL);
					}
					free(buf6);
					return (hp);
				} else { /* no v4 and no v6, return NULL */
					__IPv6_cleanup(buf6);
					*error_num = HOST_NOT_FOUND;
					return ((struct hostent *)NULL);
				}
			}
			(void) freenetconfigent(nconf);
			/*
			 * At this point, hp pts to any v4's, buf6 pts to v6's.
			 * If no v4's, return the v6's. Otherwise, map & merge
			 * the v4's with the v6's & return. Note, at this point
			 * there were no v4's in ipnodes, so the v4's are in
			 * separate hostent, that must be merged with the v6s.
			 */
			if (!hp) {
				__IPv6_cleanup(buf4);
				if (buf6) {
					hp = buf6->result;
					free(buf6);
					return (hp);
				} else {
					*error_num = HOST_NOT_FOUND;
					return ((struct hostent *)NULL);
				}
			}
			if ((resbuf = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
				__IPv6_cleanup(buf4);
				__IPv6_cleanup(buf6);
				*error_num = NO_RECOVERY;
				return ((struct hostent *)NULL);
			}
			/*
			 * We at least have v4 results, maybe v6, so merge/map
			 * them into a final result buffer and return.
			 */
			__mapv4tov6(buf4->result,
				(buf6 != NULL) ? buf6->result : NULL,
				resbuf, 1);
			__IPv6_cleanup(buf4);
			__IPv6_cleanup(buf6);
			hp = resbuf->result;
			free(resbuf);
			return (hp);
		}

		if (flags & AI_V4MAPPED) {
		/*
		 * Get all v6 EOR v4(mapped) addresses. By checking
		 * the AI_ADDRCONFIG flag here, we've handled the case of
		 * AI_DEFAULT (AI_V4MAPPED|AI_ADDRCONFIG)
		 * Note: since ipnodes can have v4s, can't hide the ipnodes
		 * lookup behind the (__ai_addrconfig(AF_INET6)).
		 */
			nssin.op_t = NSS_HOST6;
			nssin.arg.nss.host.name = name;
			nssin.arg.nss.host.buf = buf6->buffer;
			nssin.arg.nss.host.buflen = buf6->buflen;
			nssout.nss.host.hent = buf6->result;
			nssout.nss.host.herrno_p = error_num;
			if ((neterr = _get_hostserv_inetnetdir_byname(
				nconf, &nssin, &nssout)) != ND_OK) {
				__IPv6_cleanup(buf6);
				buf6 = NULL;
			}
			if (((flags & AI_ADDRCONFIG) &&
				(__ai_addrconfig(AF_INET6) > 0)) ||
				!(flags & AI_ADDRCONFIG)) {
				if (buf6) {
					int mapd;
					/*
					 * __find_mapped == 2 means there are
					 * both v4 & v6, so v4s must be filterd.
					 * Otherwise, it's one or other, so just
					 * return them.
					 */
					mapd = __find_mapped(buf6->result, 1);
					if (mapd == 2) {
						hp = __filter_v4(buf6->result,
						    error_num);
						(void) freenetconfigent(nconf);
						if (hp == NULL) {
							__IPv6_cleanup(buf6);
							return (NULL);
						}
						free(buf6);
						return (hp);
					} else if (mapd == 0) {
						free(buf6);
						(void) freenetconfigent(nconf);
						return (nssout.nss.host.hent);
					}
					/*
					 * if (mapd == 1), there are only mapd
					 * v4s in buf6, so fall through and
					 * handle v4 case below.
					 */
				}
			}
			/*
			 * We're here if:
			 * 	- No v6 interface up
			 * 	- Didn't find anything in ipnodes (buf6 == NULL)
			 *	- Fall through w/buf6 containing v4mapped.
			 */
			if (((flags & AI_ADDRCONFIG) &&
				(__ai_addrconfig(AF_INET) > 0)) ||
				!(flags & AI_ADDRCONFIG)) {
				if (buf6) { /* Something left from ipnodes */
					hp = (struct hostent *)buf6->result;
					if (__find_mapped(hp, 1) == 1) {
					/* v4 mapped only, we're done! */
						free(buf6);
						(void) freenetconfigent(nconf);
						return (hp);
					}
					__IPv6_cleanup(buf6);
				}
				/* No v4's in ipnodes, fail over to hosts */

				if ((buf4 = __IPv6_alloc(NSS_BUFLEN_HOSTS))
									== 0) {
					(void) freenetconfigent(nconf);
					*error_num = NO_RECOVERY;
					return ((struct hostent *)NULL);
				}
				if ((hp = gethostbyname_r(name, buf4->result,
				buf4->buffer, buf4->buflen, error_num)) == 0) {
					__IPv6_cleanup(buf4);
					(void) freenetconfigent(nconf);
					*error_num = HOST_NOT_FOUND;
					return ((struct hostent *)NULL);
				}
				if ((resbuf = __IPv6_alloc(NSS_BUFLEN_IPNODES))
									== 0) {
					__IPv6_cleanup(buf4);
					(void) freenetconfigent(nconf);
					*error_num = NO_RECOVERY;
					return ((struct hostent *)NULL);
				}
				__mapv4tov6(buf4->result, 0, resbuf, 1);
				__IPv6_cleanup(buf4);
				hp = resbuf->result;
				free(resbuf);
				(void) freenetconfigent(nconf);
				return (hp);
			}
		}
		/*
		 * If we got to here, must be bogus flags.
		 */
		(void) freenetconfigent(nconf);
		__IPv6_cleanup(buf6);
		*error_num = HOST_NOT_FOUND;
		return ((struct hostent *)NULL);

	case AF_INET:
		if (strstr(name, ":")) {
			__IPv6_cleanup(buf6);
			(void) freenetconfigent(nconf);
			*error_num = HOST_NOT_FOUND;
			return ((struct hostent *)NULL);
		}
		if ((flags & AI_ADDRCONFIG) &&
				(__ai_addrconfig(AF_INET) < 1)) {
			__IPv6_cleanup(buf6);
			(void) freenetconfigent(nconf);
			*error_num = HOST_NOT_FOUND;
			return ((struct hostent *)NULL);
		}
		if (inet_addr(name) == -1) {
			/* Not a literal IPv4 address; look in ipnodes */
			nssin.op_t = NSS_HOST6;
			nssin.arg.nss.host.name = name;
			nssin.arg.nss.host.buf = buf6->buffer;
			nssin.arg.nss.host.buflen = buf6->buflen;
			nssout.nss.host.hent = buf6->result;
			nssout.nss.host.herrno_p = error_num;
			if ((neterr = _get_hostserv_inetnetdir_byname(
				nconf, &nssin, &nssout)) != ND_OK) {
				__IPv6_cleanup(buf6);
				(void) freenetconfigent(nconf);
				buf6 = NULL;
			} else {
				(void) freenetconfigent(nconf);
				/*
				 * Any v4s? If yes, extract IPv4 addresses
				 * and return
				 */
				if ((hp = __mappedtov4(buf6->result,
						error_num)) == NULL) {
					/*
					 * note error_num is either
					 * NO_ADDRESS or NO_RECOVERY
					 */
					trace2(TR_getipnodebyname, 2, buflen);
					__IPv6_cleanup(buf6);
					if (*error_num == NO_RECOVERY) {
						return (0);
					}
				} else {
					trace2(TR_getipnodebyname, 3, buflen);
					__IPv6_cleanup(buf6);
					return (hp);
				}
			}
		}
		/* Literal IPv4, or no v4s in ipnodes. Try hosts */
		if ((buf4 = __IPv6_alloc(NSS_BUFLEN_HOSTS)) == 0) {
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		if ((hp = gethostbyname_r(name, buf4->result,
			buf4->buffer, buf4->buflen, error_num)) == NULL) {
			__IPv6_cleanup(buf4);
			*error_num = HOST_NOT_FOUND;
			return ((struct hostent *)NULL);
		}
		hp->h_aliases = NULL;
		free(buf4);
		return (hp);

	default:
		__IPv6_cleanup(buf6);
		(void) freenetconfigent(nconf);
		*error_num = HOST_NOT_FOUND;
		return ((struct hostent *)NULL);

	} /* switch (af) */
}

/*
 * This is the IPv6 interface for "gethostbyaddr".
 */
struct hostent *
getipnodebyaddr(const void *src, size_t len, int type, int *error_num)
{
	struct in6_addr *addr6 = 0;
	struct in_addr *addr4 = 0;
	nss_XbyY_buf_t *buf = 0;
	nss_XbyY_buf_t *res = 0;
	struct netconfig *nconf;
	struct hostent *hp = 0;
	struct	nss_netdirbyaddr_in nssin;
	union	nss_netdirbyaddr_out nssout;
	int neterr;
	char tmpbuf[64];

	trace2(TR_gethostbyaddr, 0, len);
	if (type == AF_INET6) {
		if ((addr6 = (struct in6_addr *)src) == NULL) {
			*error_num = HOST_NOT_FOUND;
			return ((struct hostent *)NULL);
		}
	} else if (type == AF_INET) {
		if ((addr4 = (struct in_addr *)src) == NULL) {
			*error_num = HOST_NOT_FOUND;
			return ((struct hostent *)NULL);
		}
	} else {
		*error_num = HOST_NOT_FOUND;
		return ((struct hostent *)NULL);
	}
	/*
	 * Specific case: query for "::"
	 */
	if (type == AF_INET6 && IN6_IS_ADDR_UNSPECIFIED(addr6)) {
		*error_num = HOST_NOT_FOUND;
		return ((struct hostent *)NULL);
	}
	/*
	 * Step 1: IPv4-mapped address  or IPv4 Compat
	 */
	if ((type == AF_INET6 && len == 16) &&
		((IN6_IS_ADDR_V4MAPPED(addr6)) ||
		(IN6_IS_ADDR_V4COMPAT(addr6)))) {
		if ((buf = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		if ((nconf = __rpc_getconfip("udp")) == NULL &&
		    (nconf = __rpc_getconfip("tcp")) == NULL) {
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		nssin.op_t = NSS_HOST6;
		if (IN6_IS_ADDR_V4COMPAT(addr6)) {
			memcpy((void *)tmpbuf, (void *)addr6,
						sizeof (*addr6));
			tmpbuf[10] = 0xffU;
			tmpbuf[11] = 0xffU;
			nssin.arg.nss.host.addr = (const char *)tmpbuf;
		} else {
			nssin.arg.nss.host.addr = (const char *)addr6;
		}
		nssin.arg.nss.host.len = sizeof (struct in6_addr);
		nssin.arg.nss.host.type = AF_INET6;
		nssin.arg.nss.host.buf = buf->buffer;
		nssin.arg.nss.host.buflen = buf->buflen;

		nssout.nss.host.hent = buf->result;
		nssout.nss.host.herrno_p = error_num;
		/*
		 * We pass in nconf and let the implementation of the
		 * long-named func decide whether to use the switch based on
		 * nc_nlookups.
		 */
		neterr =
			_get_hostserv_inetnetdir_byaddr(nconf, &nssin, &nssout);

		(void) freenetconfigent(nconf);
		if (neterr != ND_OK) {
			/* Failover case, try hosts db for v4 address */
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			if (!gethostbyaddr_r(((char *)addr6) + 12,
				sizeof (in_addr_t), AF_INET, buf->result,
				buf->buffer, buf->buflen, error_num)) {
				__IPv6_cleanup(buf);
				return ((struct hostent *)NULL);
			}
			/* Found one, now format it into mapped/compat addr */
			if ((res = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
				(void) __IPv6_cleanup(buf);
				*error_num = NO_RECOVERY;
				return ((struct hostent *)NULL);
			}
			/* Convert IPv4 to mapped/compat address w/name */
			hp = res->result;
			__mapv4tov6(buf->result, 0, res,
						IN6_IS_ADDR_V4MAPPED(addr6));
			(void) __IPv6_cleanup(buf);
			(void) free(res);
			return (hp);
		}
		/*
		 * At this point, we'll have a v4mapped hostent. If that's
		 * what was passed in, just return. If the request was a compat,
		 * twiggle the two bytes to make the mapped address a compat.
		 */
		hp = buf->result;
		if (IN6_IS_ADDR_V4COMPAT(addr6)) {
			addr6 = (struct in6_addr *)hp->h_addr_list[0];
			addr6->s6_addr[10] = 0;
			addr6->s6_addr[11] = 0;
		}
		free(buf);
		return (hp);
	}
	/*
	 * Step 2: AF_INET, v4 lookup. Since we're going to search the
	 * ipnodes (v6) path first, we need to treat this as a v4mapped
	 * address. nscd(1m) caches v4 from ipnodes as mapped v6's. The
	 * switch backend knows to lookup v4's (not v4mapped) from the
	 * name services.
	 */
	if (type == AF_INET) {
		struct in6_addr v4mapbuf;
		addr6 = &v4mapbuf;

		IN6_INADDR_TO_V4MAPPED(addr4, addr6);
		if ((nconf = __rpc_getconfip("udp")) == NULL &&
		    (nconf = __rpc_getconfip("tcp")) == NULL) {
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		if ((buf = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		nssin.op_t = NSS_HOST6;
		nssin.arg.nss.host.addr = (const char *)addr6;
		nssin.arg.nss.host.len = sizeof (struct in6_addr);
		nssin.arg.nss.host.type = AF_INET6;
		nssin.arg.nss.host.buf = buf->buffer;
		nssin.arg.nss.host.buflen = buf->buflen;

		nssout.nss.host.hent = buf->result;
		nssout.nss.host.herrno_p = error_num;
		/*
		 * We pass in nconf and let the implementation of the
		 * long-named func decide whether to use the switch based on
		 * nc_nlookups.
		 */
		neterr =
			_get_hostserv_inetnetdir_byaddr(nconf, &nssin, &nssout);

		(void) freenetconfigent(nconf);
		if (neterr != ND_OK) {
			/* Failover case, try hosts db for v4 address */
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			hp = buf->result;
			if (!gethostbyaddr_r(src, len, type, buf->result,
					buf->buffer, buf->buflen, error_num)) {
				__IPv6_cleanup(buf);
				return ((struct hostent *)NULL);
			}
			free(buf);
			return (hp);
		}
		if ((hp = __mappedtov4(buf->result, error_num)) == NULL) {
			__IPv6_cleanup(buf);
			return ((struct hostent *)NULL);
		}
		(void) __IPv6_cleanup(buf);
		return (hp);
	}
	/*
	 * Step 3: AF_INET6, plain vanilla v6 getipnodebyaddr() call.
	 */
	if (type == AF_INET6) {
		if ((nconf = __rpc_getconfip("udp")) == NULL &&
		    (nconf = __rpc_getconfip("tcp")) == NULL) {
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		if ((buf = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
			*error_num = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		nssin.op_t = NSS_HOST6;
		nssin.arg.nss.host.addr = (const char *)addr6;
		nssin.arg.nss.host.len = len;
		nssin.arg.nss.host.type = type;
		nssin.arg.nss.host.buf = buf->buffer;
		nssin.arg.nss.host.buflen = buf->buflen;

		nssout.nss.host.hent = buf->result;
		nssout.nss.host.herrno_p = error_num;
		/*
		 * We pass in nconf and let the implementation of the
		 * long-named func decide whether to use the switch based on
		 * nc_nlookups.
		 */
		neterr =
			_get_hostserv_inetnetdir_byaddr(nconf, &nssin, &nssout);

		(void) freenetconfigent(nconf);
		if (neterr != ND_OK) {
			trace3(TR__getipnodebyaddr, 0, len, buf->buflen);
			__IPv6_cleanup(buf);
			return ((struct hostent *)NULL);
		}
		trace2(TR_gethostbyaddr, 1, len);
		free(buf);
		return (nssout.nss.host.hent);
	}
	/*
	 * If we got here, unknown type.
	 */
	*error_num = HOST_NOT_FOUND;
	return ((struct hostent *)NULL);
}

void
freehostent(struct hostent *hent)
{
	free(hent);
}

static int
__ai_addrconfig(int af)
{
	struct lifnum lifn;

	if (_sfd == 0) {
		mutex_lock(&_sfd_lock);
		if ((_sfd = open("/dev/udp", O_RDONLY)) < 0) {
			mutex_unlock(&_sfd_lock);
			return (-1);
		}
		mutex_unlock(&_sfd_lock);
	}
	lifn.lifn_family = af;	/* Checking v4 or v6 */
	/*
	 * We want to determine if this machine knows anything at all
	 * about the address family; the status of the interface is less
	 * important. Hence, set 'lifn_flags' to zero.
	 */
	lifn.lifn_flags = 0;
	if (ioctl(_sfd, SIOCGLIFNUM, &lifn, sizeof (lifn)) < 0)
		return (-1);
	return (lifn.lifn_count);
}

/*
 * This routine will either convert an IPv4 address to a mapped or compat
 * IPv6 (if he6 == NULL) or merge IPv6 (he6) addresses with mapped
 * v4 (he4) addresses. In either case, the results are returned in res.
 * Caller must provide all buffers.
 * Inputs:
 * 		he4	pointer to IPv4 buffer
 *		he6	pointer to IPv6 buffer (NULL if not merging v4/v6
 *		res	pointer to results buffer
 *		mapped	mapped == 1, map IPv4 : mapped == 0, compat IPv4
 *			mapped flag is ignored if he6 != NULL
 *
 * The results are packed into the res->buffer as follows:
 * <--------------- buffer + buflen -------------------------------------->
 * |-----------------|-----------------|----------------|----------------|
 * | pointers vector | pointers vector | aliases grow   | addresses grow |
 * | for addresses   | for aliases     |                |                |
 * | this way ->     | this way ->     | <- this way    |<- this way     |
 * |-----------------|-----------------|----------------|----------------|
 * | grows in PASS 1 | grows in PASS2  | grows in PASS2 | grows in PASS 1|
 */
static struct hostent *
__mapv4tov6(struct hostent *he4, struct hostent *he6, nss_XbyY_buf_t *res,
		int mapped)
{
	char	*buffer, *limit;
	int	buflen = res->buflen;
	struct	in6_addr *addr6p;
	char	*buff_locp;
	struct	hostent *host;
	int	count = 0, len, i;
	char	*h_namep;

	if (he4 == NULL || res == NULL) {
		return (NULL);
	}
	limit = res->buffer + buflen;
	host = (struct hostent *)res->result;
	buffer = res->buffer;

	buff_locp = (char *)ROUND_DOWN(limit, sizeof (addr6p));
	host->h_addr_list = (char **)ROUND_UP(buffer, sizeof (char **));
	if ((char *)host->h_addr_list >= limit ||
		buff_locp <= (char *)host->h_addr_list) {
		return ((struct hostent *)NULL);
	}
	if (he6 == NULL) {
		/*
		 * If he6==NULL, map the v4 address into the v6 address format.
		 * This is used for getipnodebyaddr() (single address, mapped or
		 * compatible) or for v4 mapped for getipnodebyname(), which
		 * could be multiple addresses. This could also be a literal
		 * address string, which is why there is a inet_addr() call.
		 */
		for (i = 0; he4->h_addr_list[i] != NULL; i++) {
			buff_locp -= sizeof (struct in6_addr);
			if (buff_locp <=
				(char *)&(host->h_addr_list[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			addr6p = (struct in6_addr *)buff_locp;
			host->h_addr_list[count] = (char *)addr6p;
			bzero(addr6p->s6_addr, sizeof (struct in6_addr));
			if (mapped) {
				addr6p->s6_addr[10] = 0xff;
				addr6p->s6_addr[11] = 0xff;
			}
			bcopy((char *)he4->h_addr_list[i],
				&addr6p->s6_addr[12], sizeof (struct in_addr));
			++count;
		}
		/*
		 * Set last array element to NULL and add cname as first alias
		 */
		host->h_addr_list[count] = NULL;
		host->h_aliases = host->h_addr_list + count + 1;
		count = 0;
		if ((int)(inet_addr(he4->h_name)) != -1) {
		/*
		 * Literal address string, since we're mapping, we need the IPv6
		 * V4 mapped literal address string for h_name.
		 */
			char	tmpstr[128];
			inet_ntop(AF_INET6, host->h_addr_list[0], tmpstr,
							sizeof (tmpstr));
			buff_locp -= (len = strlen(tmpstr) + 1);
			h_namep = tmpstr;
			if (buff_locp <= (char *)(host->h_aliases))
				return ((struct hostent *)NULL);
			bcopy(h_namep, buff_locp, len);
			host->h_name = buff_locp;
			host->h_aliases = NULL; /* no aliases for literal */
			host->h_length = sizeof (struct in6_addr);
			host->h_addrtype = AF_INET6;
			return (host); 		/* we're done, return result */
		}
		/*
		 * Not a literal address string, so just copy h_name.
		 */
		buff_locp -= (len = strlen(he4->h_name) + 1);
		h_namep = he4->h_name;
		if (buff_locp <= (char *)(host->h_aliases))
			return ((struct hostent *)NULL);
		bcopy(h_namep, buff_locp, len);
		host->h_name = buff_locp;
		/*
		 * Pass 2 (IPv4 aliases):
		 */
		for (i = 0; he4->h_aliases[i] != NULL; i++) {
			buff_locp -= (len = strlen(he4->h_aliases[i]) + 1);
			if (buff_locp <=
					(char *)&(host->h_aliases[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			host->h_aliases[count] = buff_locp;
			bcopy((char *)he4->h_aliases[i], buff_locp, len);
			++count;
		}
		host->h_aliases[count] = NULL;
		host->h_length = sizeof (struct in6_addr);
		host->h_addrtype = AF_INET6;
		return (host);
	} else {
		/*
		 * Merge IPv4 mapped addresses with IPv6 addresses. The
		 * IPv6 address will go in first, followed by the v4 mapped.
		 *
		 * Pass 1 (IPv6 addresses):
		 */
		for (i = 0; he6->h_addr_list[i] != NULL; i++) {
			buff_locp -= sizeof (struct in6_addr);
			if (buff_locp <=
				(char *)&(host->h_addr_list[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			host->h_addr_list[count] = buff_locp;
			bcopy((char *)he6->h_addr_list[i], buff_locp,
						sizeof (struct in6_addr));
			++count;
		}
		/*
		 * Pass 1 (IPv4 mapped addresses):
		 */
		for (i = 0; he4->h_addr_list[i] != NULL; i++) {
			buff_locp -= sizeof (struct in6_addr);
			if (buff_locp <=
				(char *)&(host->h_addr_list[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			addr6p = (struct in6_addr *)buff_locp;
			host->h_addr_list[count] = (char *)addr6p;
			bzero(addr6p->s6_addr, sizeof (struct in6_addr));
			addr6p->s6_addr[10] = 0xff;
			addr6p->s6_addr[11] = 0xff;
			bcopy(he4->h_addr_list[i], &addr6p->s6_addr[12],
						sizeof (struct in_addr));
			++count;
		}
		/*
		 * Pass 2 (IPv6 aliases, host name first). We start h_aliases
		 * one after where h_addr_list array ended. This is where cname
		 * is put, followed by all aliases. Reset count to 0, for index
		 * in the h_aliases array.
		 */
		host->h_addr_list[count] = NULL;
		host->h_aliases = host->h_addr_list + count + 1;
		count = 0;
		buff_locp -= (len = strlen(he6->h_name) + 1);
		if (buff_locp <= (char *)(host->h_aliases))
			return ((struct hostent *)NULL);
		bcopy(he6->h_name, buff_locp, len);
		host->h_name = buff_locp;
		for (i = 0; he6->h_aliases[i] != NULL; i++) {
			buff_locp -= (len = strlen(he6->h_aliases[i]) + 1);
			if (buff_locp <=
					(char *)&(host->h_aliases[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			host->h_aliases[count] = buff_locp;
			bcopy((char *)he6->h_aliases[i], buff_locp, len);
			++count;
		}
		/*
		 * Pass 2 (IPv4 aliases):
		 */
		for (i = 0; he4->h_aliases[i] != NULL; i++) {
			buff_locp -= (len = strlen(he4->h_aliases[i]) + 1);
			if (buff_locp <=
					(char *)&(host->h_aliases[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				return ((struct hostent *)NULL);
			}
			host->h_aliases[count] = buff_locp;
			bcopy((char *)he4->h_aliases[i], buff_locp, len);
			++count;
		}
		host->h_aliases[count] = NULL;
		host->h_length = sizeof (struct in6_addr);
		host->h_addrtype = AF_INET6;
		return (host);
	}
}

/*
 * This routine will convert a mapped v4 hostent (AF_INET6) to a
 * AF_INET hostent. If no mapped addrs found, then a NULL is returned.
 * If mapped addrs found, then a new buffer is alloc'd and all the v4 mapped
 * addresses are extracted and copied to it. On sucess, a pointer to a new
 * hostent is returned.
 * There are two possible errors in which case a NULL is returned.
 * One of two error codes are returned:
 *
 * NO_RECOVERY - a malloc failed or the like for which there's no recovery.
 * NO_ADDRESS - after filtering all the v4, there was nothing left!
 *
 * Inputs:
 *              he              pointer to hostent with mapped v4 addresses
 *              filter_error    pointer to return error code
 * Return:
 *		pointer to a malloc'd hostent with v4 addresses.
 *
 * The results are packed into the res->buffer as follows:
 * <--------------- buffer + buflen -------------------------------------->
 * |-----------------|-----------------|----------------|----------------|
 * | pointers vector | pointers vector | aliases grow   | addresses grow |
 * | for addresses   | for aliases     |                |                |
 * | this way ->     | this way ->     | <- this way    |<- this way     |
 * |-----------------|-----------------|----------------|----------------|
 * | grows in PASS 1 | grows in PASS2  | grows in PASS2 | grows in PASS 1|
 */
static struct hostent *
__mappedtov4(struct hostent *he, int *extract_error)
{
	char	*buffer, *limit;
	nss_XbyY_buf_t *res;
	int	buflen = NSS_BUFLEN_HOSTS;
	struct	in_addr *addr4p;
	char	*buff_locp;
	struct	hostent *host;
	int	count = 0, len, i;
	char	*h_namep;

	if (he == NULL) {
		*extract_error = NO_ADDRESS;
		return ((struct hostent *)NULL);
	}
	if ((__find_mapped(he, 0)) == 0) {
		*extract_error = NO_ADDRESS;
		return ((struct hostent *)NULL);
	}
	if ((res = __IPv6_alloc(NSS_BUFLEN_HOSTS)) == 0) {
		*extract_error = NO_RECOVERY;
		return ((struct hostent *)NULL);
	}
	limit = res->buffer + buflen;
	host = (struct hostent *)res->result;
	buffer = res->buffer;

	buff_locp = (char *)ROUND_DOWN(limit, sizeof (addr4p));
	host->h_addr_list = (char **)ROUND_UP(buffer, sizeof (char **));
	if ((char *)host->h_addr_list >= limit ||
		buff_locp <= (char *)host->h_addr_list) {
		*extract_error = NO_RECOVERY;
		__IPv6_cleanup(res);
		return ((struct hostent *)NULL);
	}
	/*
	 * "Unmap" the v4 mapped address(es) into a v4 hostent format.
	 * This is used for getipnodebyaddr() (single address) or for
	 * v4 mapped for getipnodebyname(), which could be multiple
	 * addresses. This could also be a literal address string,
	 * which is why there is a inet_addr() call.
	 */
		for (i = 0; he->h_addr_list[i] != NULL; i++) {
			if (!IN6_IS_ADDR_V4MAPPED((struct in6_addr *)
							he->h_addr_list[i]))
			continue;
			buff_locp -= sizeof (struct in6_addr);
			if (buff_locp <=
				(char *)&(host->h_addr_list[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				__IPv6_cleanup(res);
				*extract_error = NO_RECOVERY;
				return ((struct hostent *)NULL);
			}
			addr4p = (struct in_addr *)buff_locp;
			host->h_addr_list[count] = (char *)addr4p;
			bzero((char *)&addr4p->s_addr,
						sizeof (struct in_addr));
			IN6_V4MAPPED_TO_INADDR(
					(struct in6_addr *)he->h_addr_list[i],
					addr4p);
			++count;
		}
		/*
		 * Set last array element to NULL and add cname as first alias
		 */
		host->h_addr_list[count] = NULL;
		host->h_aliases = host->h_addr_list + count + 1;
		count = 0;
		if ((int)(inet_addr(&he->h_name[12])) != -1) {
		/*
		 * Literal address string, since we're mapping, we need the IPv4
		 * literal address string from the mapped address for h_name.
		 */
			char	tmpstr[128];
			inet_ntop(AF_INET, host->h_addr_list[0], tmpstr,
							sizeof (tmpstr));
			buff_locp -= (len = strlen(tmpstr) + 1);
			h_namep = tmpstr;
			if (buff_locp <= (char *)(host->h_aliases)) {
				*extract_error = NO_RECOVERY;
				return ((struct hostent *)NULL);
			}
			bcopy(h_namep, buff_locp, len);
			host->h_name = buff_locp;
			host->h_aliases = NULL; /* no aliases for literal */
			host->h_length = sizeof (struct in_addr);
			host->h_addrtype = AF_INET;
			free(res);
			return (host); 		/* we're done, return result */
		}
		/*
		 * Not a literal address string, so just copy h_name.
		 */
		buff_locp -= (len = strlen(he->h_name) + 1);
		h_namep = he->h_name;
		if (buff_locp <= (char *)(host->h_aliases)) {
			*extract_error = NO_RECOVERY;
			__IPv6_cleanup(res);
			return ((struct hostent *)NULL);
		}
		bcopy(h_namep, buff_locp, len);
		host->h_name = buff_locp;
		/*
		 * Pass 2 (IPv4 aliases):
		 */
		for (i = 0; he->h_aliases[i] != NULL; i++) {
			buff_locp -= (len = strlen(he->h_aliases[i]) + 1);
			if (buff_locp <=
					(char *)&(host->h_aliases[count + 1])) {
			/*
			 * Has to be room for the pointer to the address we're
			 * about to add, as well as the final NULL ptr.
			 */
				*extract_error = NO_RECOVERY;
				__IPv6_cleanup(res);
				return ((struct hostent *)NULL);
			}
			host->h_aliases[count] = buff_locp;
			bcopy((char *)he->h_aliases[i], buff_locp, len);
			++count;
		}
		host->h_aliases[count] = NULL;
		host->h_length = sizeof (struct in_addr);
		host->h_addrtype = AF_INET;
		free(res);
		return (host);
}

/*
 * This routine takes as input a pointer to a hostent and filters out
 * any v4 mapped addresses (which might be returned from the call to the
 * ipnodes db). If no mapped addrs found, then the hostent pointer that was
 * passed in is returned. This is an optimization, but requires that the
 * caller NOT call __IPv6_cleanup() on the buffer that contains the hostent
 * passed in to this routine.
 * If mapped addrs found, then a new buffer is alloc'd and all the v6 addrs
 * are copied to it.
 * NOTE: Caller's hostent passed in is freed, and a pointer to the
 * new hostent is returned. If anything goes wrong, a NULL is returned,
 * the caller's hostent is NOT freed and one of two error codes are returned:
 *
 * NO_RECOVERY - a malloc failed or the like for which there's no recovery.
 * NO_ADDRESS - after filtering all the v4, there was nothing left!
 *
 * Freeing the old hostent and returning a new pointer avoids having the
 * caller pass in an alloc'd buffer (only alloc'd if needed).
 * Inputs:
 *		he		pointer to filtered hostent to be returned
 *		filter_error	pointer to return error code
 *
 * NOTE: the results are packed into the res->buffer as follows:
 * <--------------- buffer + buflen -------------------------------------->
 * |-----------------|-----------------|----------------|----------------|
 * | pointers vector | pointers vector | aliases grow   | addresses grow |
 * | for addresses   | for aliases     |                |                |
 * | this way ->     | this way ->     | <- this way    |<- this way     |
 * |-----------------|-----------------|----------------|----------------|
 * | grows in PASS 1 | grows in PASS2  | grows in PASS2 | grows in PASS 1|
 */
static struct hostent *
__filter_v4(struct hostent *he, int *filter_error)
{
	char	*buffer, *limit;
	nss_XbyY_buf_t *res;
	int	buflen = NSS_BUFLEN_IPNODES;
	struct	in6_addr *addr6p;
	char	*buff_locp;
	struct	hostent *host;
	int	count = 0, len, i;
	char	*h_namep;
	int	mapped_found = 0;


	if (he == NULL) {
		*filter_error = NO_ADDRESS;
		return ((struct hostent *)NULL);
	}
	if ((mapped_found = __find_mapped(he, 1)) == 0) {
		return (he);
	}
	if (mapped_found == 1) {
	/*
	 * If we didn't find any v6's, we can't return a hostent with no
	 * addresses, so set filter_error = NO_ADDRESS and return NULL. Note
	 * we don't release callers hostent, since we're not returning one.
	 */
		*filter_error = NO_ADDRESS;
		return ((struct hostent *)NULL);
	}
	if ((res = __IPv6_alloc(NSS_BUFLEN_IPNODES)) == 0) {
		*filter_error = NO_RECOVERY;
		return ((struct hostent *)NULL);
	}

	limit = res->buffer + buflen;
	host = (struct hostent *)res->result;
	buffer = res->buffer;
	buff_locp = (char *)ROUND_DOWN(limit, sizeof (addr6p));
	host->h_addr_list = (char **)ROUND_UP(buffer, sizeof (char **));
	if ((char *)host->h_addr_list >= limit ||
		buff_locp <= (char *)host->h_addr_list) {
		__IPv6_cleanup(res);
		*filter_error = NO_RECOVERY;
		return ((struct hostent *)NULL);
	}
	/*
	 * Skip the v4 mapped address(es), copy only the v6 addrs into
	 * the new hostent buffer.
	 */
	for (i = 0; he->h_addr_list[i] != NULL; i++) {
		if (IN6_IS_ADDR_V4MAPPED((struct in6_addr *)
							he->h_addr_list[i]))
			continue;
		buff_locp -= sizeof (struct in6_addr);
		if (buff_locp <= (char *)&(host->h_addr_list[count + 1])) {
		/*
		 * Has to be room for the pointer to the address we're
		 * about to add, as well as the final NULL ptr.
		 */
			__IPv6_cleanup(res);
			*filter_error = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		addr6p = (struct in6_addr *)buff_locp;
		host->h_addr_list[count] = (char *)addr6p;
		bzero((char *)addr6p->s6_addr, sizeof (struct in6_addr));
		bcopy((char *)he->h_addr_list[i], buff_locp,
						sizeof (struct in6_addr));
		++count;
	}
	/*
	 * Set last array element to NULL and add cname as first alias
	 */
	host->h_addr_list[count] = NULL;
	host->h_aliases = host->h_addr_list + count + 1;
	count = 0;
	/* Now copy h_name. */
	buff_locp -= (len = strlen(he->h_name) + 1);
	h_namep = he->h_name;
	if (buff_locp <= (char *)(host->h_aliases)) {
		__IPv6_cleanup(res);
		*filter_error = NO_RECOVERY;
		return ((struct hostent *)NULL);
	}
	bcopy(h_namep, buff_locp, len);
	host->h_name = buff_locp;
	/*
	 * Pass 2 (IPv4 aliases):
	 */
	for (i = 0; he->h_aliases[i] != NULL; i++) {
		buff_locp -= (len = strlen(he->h_aliases[i]) + 1);
		if (buff_locp <= (char *)&(host->h_aliases[count + 1])) {
		/*
		 * Has to be room for the pointer to the address we're
		 * about to add, as well as the final NULL ptr.
		 */
			__IPv6_cleanup(res);
			*filter_error = NO_RECOVERY;
			return ((struct hostent *)NULL);
		}
		host->h_aliases[count] = buff_locp;
		bcopy((char *)he->h_aliases[i], buff_locp, len);
		++count;
	}
	host->h_aliases[count] = NULL;
	host->h_length = sizeof (struct in6_addr);
	host->h_addrtype = AF_INET6;
	free(res); /* We're done with the nss_XbyY_buf_t struct, so free */
	free(he); /* Don't panic, we're giving the caller a new he */
	return (host);
}

/*
 * This routine searches a hostent for v4 mapped IPv6 addresses.
 * he		hostent structure to seach
 * find_both	flag indicating if only want mapped or both map'd and v6
 * return values:
 * 			0 = No mapped addresses
 *			1 = Mapped v4 address found (returns on first one found)
 *			2 = Both v6 and v4 mapped are present
 *
 * If hostent passed in with no addresses, zero will be returned.
 */

static int
__find_mapped(struct hostent *he, int find_both)
{
	int i;
	int mapd_found = 0;
	int v6_found = 0;

	for (i = 0; he->h_addr_list[i] != NULL; i++) {
		if (IN6_IS_ADDR_V4MAPPED(
				(struct in6_addr *)he->h_addr_list[i])) {
			if (find_both)
				mapd_found = 1;
			else
				return (1);
		} else {
			v6_found = 1;
		}
		/* save some iterations once both found */
		if (mapd_found && v6_found)
			return (2);
	}
	return (mapd_found);
}

/*
 * This routine was added specifically for the IPv6 getipnodeby*() APIs. This
 * separates the result pointer (ptr to hostent+data buf) from the
 * nss_XbyY_buf_t ptr (required for nsswitch API). The returned hostent ptr
 * can be passed to freehostent() and freed independently.
 *
 *   bufp->result    bufp->buffer
 *		|		|
 *		V		V
 *		------------------------------------------------...--
 *		|struct hostent	|addresses		     aliases |
 *		------------------------------------------------...--
 *		|               |<--------bufp->buflen-------------->|
 */

#define	ALIGN(x) ((((long)(x)) + sizeof (long) - 1) & ~(sizeof (long) - 1))

static nss_XbyY_buf_t *
__IPv6_alloc(int bufsz)
{
	nss_XbyY_buf_t *bufp;

	if ((bufp = (nss_XbyY_buf_t *)malloc(sizeof (nss_XbyY_buf_t))) == 0)
		return ((nss_XbyY_buf_t *)NULL);

	if ((bufp->result = (void *)malloc(ALIGN(sizeof (struct hostent))
							+ bufsz)) == NULL) {
		free(bufp);
		return ((nss_XbyY_buf_t *)NULL);
	}
	bufp->buffer = (char *)(bufp->result) + sizeof (struct hostent);
	bufp->buflen = bufsz;
	return (bufp);
}

/*
 * This routine is use only for error return cleanup. This will free the
 * hostent pointer, so don't use for successful returns.
 */
static void
__IPv6_cleanup(nss_XbyY_buf_t *bufp)
{
	if (bufp == NULL)
		return;
	free(bufp->result);
	free(bufp);
}
