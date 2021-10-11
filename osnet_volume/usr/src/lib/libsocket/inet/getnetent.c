/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */


#pragma ident	"@(#)getnetent.c	1.14	97/08/13 SMI"

#include <netdb.h>
#include <nss_dbdefs.h>


#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 */

/*
 * Don't free this, even on an endnetent(), because bitter experience shows
 * that there's production code that does getXXXbyYYY(), then endXXXent(),
 * and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define	GETBUF()	\
NSS_XbyY_ALLOC(&buffer, (int)sizeof (struct netent), NSS_BUFLEN_NETWORKS)
	/* === ?? set ENOMEM on failure?  */

struct netent *
getnetbyname(const char *nam)
{
	nss_XbyY_buf_t	*b;
	struct netent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getnetbyname_r(nam, b->result, b->buffer, b->buflen);
	}
	return (res);
}

struct netent *
getnetbyaddr(in_addr_t net, int type)
{
	nss_XbyY_buf_t	*b;
	struct netent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getnetbyaddr_r(net, type, b->result,
				b->buffer, b->buflen);
	}
	return (res);
}

struct netent *
getnetent(void)
{
	nss_XbyY_buf_t	*b;
	struct netent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getnetent_r(b->result, b->buffer, b->buflen);
	}
	return (res);
}

#endif	NSS_INCLUDE_UNSAFE
