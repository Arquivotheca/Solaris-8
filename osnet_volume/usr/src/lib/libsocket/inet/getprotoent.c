/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#pragma ident	"@(#)getprotoent.c	1.13	97/04/15 SMI"

#include <netdb.h>
#include <nss_dbdefs.h>


#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 */

/*
 * Don't free this, even on an endprotoent(), because bitter experience shows
 * that there's production code that does getXXXbyYYY(), then endXXXent(),
 * and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define	GETBUF()	\
NSS_XbyY_ALLOC(&buffer, (int)sizeof (struct protoent), NSS_BUFLEN_PROTOCOLS)
	/* === ?? set ENOMEM on failure?  */

struct protoent *
getprotobyname(const char *nam)
{
	nss_XbyY_buf_t	*b;
	struct protoent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getprotobyname_r(nam, b->result, b->buffer, b->buflen);
	}
	return (res);
}

struct protoent *
getprotobynumber(int proto)
{
	nss_XbyY_buf_t	*b;
	struct protoent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getprotobynumber_r(proto, b->result,
					b->buffer, b->buflen);
	}
	return (res);
}

struct protoent *
getprotoent()
{
	nss_XbyY_buf_t	*b;
	struct protoent	*res = 0;

	if ((b = GETBUF()) != 0) {
		res = getprotoent_r(b->result, b->buffer, b->buflen);
	}
	return (res);
}

#endif	NSS_INCLUDE_UNSAFE
