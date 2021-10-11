/*
 * Copyright (c) 1986-1994 by Sun Microsystems Inc.
 *
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 *
 * lib/libsocket/inet/getservent.c
 */

#pragma ident	"@(#)getservent.c	1.15	97/04/15 SMI"

#include <netdb.h>
#include <nss_dbdefs.h>

#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Don't free this, even on an endservent(), because bitter experience shows
 * that there's production code that does getXXXbyYYY(), then endXXXent(),
 * and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define	GETBUF()						\
NSS_XbyY_ALLOC(&buffer, (int)sizeof (struct servent), NSS_BUFLEN_SERVICES)
	/* === ?? set ENOMEM on failure?  */

struct servent *
getservbyname(const char *nam, const char *proto)
{
	nss_XbyY_buf_t	*b;
	struct servent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getservbyname_r(nam, proto,
					b->result, b->buffer, b->buflen);
	}
	return (res);
}

struct servent *
getservbyport(int port, const char *proto)
{
	nss_XbyY_buf_t	*b;
	struct servent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getservbyport_r(port, proto,
					b->result, b->buffer, b->buflen);
	}
	return (res);
}

struct servent *
getservent(void)
{
	nss_XbyY_buf_t	*b;
	struct servent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getservent_r(b->result, b->buffer, b->buflen);
	}
	return (res);
}

#endif	NSS_INCLUDE_UNSAFE
