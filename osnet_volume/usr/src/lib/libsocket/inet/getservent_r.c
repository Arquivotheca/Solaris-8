/*
 *  Copyright (c) 1986-1994 by Sun Microsystems Inc.
 *
 * lib/libsocket/inet/getservent_r.c
 *
 * This file defines and implements the re-entrant enumeration routines for
 *   services: setservent(), getservent_r(), and endservent(). They consult
 *   the switch policy directly and do not "share" their enumeration state
 *   nor the stayopen flag with the implentation of the more common
 *   getservbyname_r()/getservbyport_r(). The latter follows a tortuous
 *   route in order to be consistent with netdir_getbyYY() (see
 *   getservbyname_r.c and lib/libnsl/nss/netdir_inet.c).
 */

#pragma ident	"@(#)getservent_r.c	1.6	97/04/15 SMI"

#include <sys/types.h>
#include <nss_dbdefs.h>

/*
 * str2servent is implemented in libnsl, libnsl/nss/netdir_inet.c, since
 * the "engine" of the new gethost/getserv/netdir lives in libnsl.
 */
int str2servent(const char *, int, void *, char *, int);

/*
 * Unsynchronized, but it affects only
 * efficiency, not correctness.
 */
static int services_stayopen;
static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

static void
_nss_initf_services(nss_db_params_t *p)
{
	p->name	= NSS_DBNAM_SERVICES;
	p->default_config = NSS_DEFCONF_SERVICES;
}

int
setservent(int stay)
{
	services_stayopen |= stay;
	nss_setent(&db_root, _nss_initf_services, &context);
	return (0);
}

int
endservent()
{
	services_stayopen = 0;
	nss_endent(&db_root, _nss_initf_services, &context);
	nss_delete(&db_root);
	return (0);
}

struct servent *
getservent_r(struct servent *result, char *buffer, int buflen)
{
	nss_XbyY_args_t arg;
	nss_status_t	res;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2servent);
	/*
	 * Setting proto to NULL here is a bit of a hack since we share
	 * the parsing code in the NIS+ backend with our getservbyYY()
	 * brethren who can search on 1-1/2 key. If they pass a NULL
	 * proto, the parsing code deals with it by picking the protocol
	 * from the first NIS+ matching object and combining all entries
	 * with "that" proto field. NIS+ is the only name service, so far,
	 * that can return multiple entries on a lookup.
	 */
	arg.key.serv.proto	= NULL;
	/* === No stayopen flag;  of course you stay open for iteration */
	res = nss_getent(&db_root, _nss_initf_services, &context, &arg);
	arg.status = res;
	return (struct servent *)NSS_XbyY_FINI(&arg);
}
