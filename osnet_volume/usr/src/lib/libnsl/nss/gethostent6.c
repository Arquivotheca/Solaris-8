/*
 * 	(c) 1991-1998  Sun Microsystems, Inc
 *	All rights reserved.
 *
 * lib/libnsl/nss/gethostent6.c
 *
 * This file defines and implements the re-entrant enumeration routines for
 *   IPv6 hosts: sethostent6(), gethostent6(), and endhostent6().
 *   They consult the switch policy directly and do not "share" their
 *   enumeration state nor the stayopen flag with the implentation of the
 *   more commonly used getipnodebyname()/getipnodebyaddr(). The latter
 *   follows a tortuous route in order to be consistent with netdir_getbyYY()
 *   (see getipnodebyname.c and netdir_inet.c).
 */

#ident	"@(#)gethostent6.c	1.1	99/03/21	SMI"

#include <sys/socket.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>

int __str2hostent(int, const char *, int, void *, char *, int);


/* IPv6 wrapper for __str2hostent() */
int
str2hostent6(instr, lenstr, ent, buffer, buflen)
const char	*instr;
int		lenstr;
void		*ent;
char		*buffer;
int		buflen;
{
	return (__str2hostent(AF_INET6, instr, lenstr, ent, buffer, buflen));
}

static int ipnodes_stayopen;
/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_ipnodes(p)
	nss_db_params_t	*p;
{
	trace1(TR__nss_initf_ipnodes, 0);
	p->name	= NSS_DBNAM_IPNODES;
	p->default_config = NSS_DEFCONF_IPNODES;
	trace1(TR__nss_initf_ipnodes, 1);
}

__sethostent6(stay)
	int		stay;
{
	trace1(TR_sethostent6, 0);
	ipnodes_stayopen |= stay;
	nss_setent(&db_root, _nss_initf_ipnodes, &context);
	trace1(TR_sethostent6, 1);
	return (0);
}

__endhostent6()
{
	trace1(TR_endhostent6, 0);
	ipnodes_stayopen = 0;
	nss_endent(&db_root, _nss_initf_ipnodes, &context);
	nss_delete(&db_root);
	trace1(TR_endhostent6, 1);
	return (0);
}

struct hostent *
__gethostent6(result, buffer, buflen, h_errnop)
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR_gethostent6, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent6);
	res = nss_getent(&db_root, _nss_initf_ipnodes,
	    &context, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR_gethostent6, 1, buflen);
	return (struct hostent *) NSS_XbyY_FINI(&arg);
}
