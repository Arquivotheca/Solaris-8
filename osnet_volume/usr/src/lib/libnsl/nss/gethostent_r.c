/*
 * 	(c) 1991-1994  Sun Microsystems, Inc
 *	All rights reserved.
 *
 * lib/libnsl/nss/gethostent_r.c
 *
 * This file defines and implements the re-entrant enumeration routines for
 *   hosts: sethostent(), gethostent_r(), and endhostent(). They consult
 *   the switch policy directly and do not "share" their enumeration state
 *   nor the stayopen flag with the implentation of the more commonly used
 *   gethostbyname_r()/gethostbyaddr_r(). The latter follows a tortuous
 *   route in order to be consistent with netdir_getbyYY() (see
 *   gethostbyname_r.c and netdir_inet.c).
 */

#ident	"@(#)gethostent_r.c	1.6	99/03/21	SMI"

#include <sys/socket.h>
#include <sys/types.h>
#include <nss_dbdefs.h>
#include <rpc/trace.h>

int __str2hostent(int, const char *, int, void *, char *, int);

/*
 * str2hostent() is now a wrapper to __str2hostent().
 * __str2hostent() now takes an extra argument to specify the
 * AF_INET type for address parsing.
 */
int
str2hostent(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
        return (__str2hostent(AF_INET, instr, lenstr, ent, buffer, buflen));
}

static int hosts_stayopen;
/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness
 */

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_hosts(p)
	nss_db_params_t	*p;
{
	trace1(TR__nss_initf_hosts, 0);
	p->name	= NSS_DBNAM_HOSTS;
	p->default_config = NSS_DEFCONF_HOSTS;
	trace1(TR__nss_initf_hosts, 1);
}

sethostent(stay)
	int		stay;
{
	trace1(TR_sethostent, 0);
	hosts_stayopen |= stay;
	nss_setent(&db_root, _nss_initf_hosts, &context);
	trace1(TR_sethostent, 0);
	return (0);
}

endhostent()
{
	trace1(TR_endhostent, 0);
	hosts_stayopen = 0;
	nss_endent(&db_root, _nss_initf_hosts, &context);
	nss_delete(&db_root);
	trace1(TR_endhostent, 0);
	return (0);
}

struct hostent *
gethostent_r(result, buffer, buflen, h_errnop)
	struct hostent	*result;
	char		*buffer;
	int		buflen;
	int		*h_errnop;
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	trace2(TR_gethostbyent_r, 0, buflen);
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2hostent);
	res = nss_getent(&db_root, _nss_initf_hosts,
	    &context, &arg);
	arg.status = res;
	*h_errnop = arg.h_errno;
	trace2(TR_gethostbyent_r, 1, buflen);
	return (struct hostent *) NSS_XbyY_FINI(&arg);
}
