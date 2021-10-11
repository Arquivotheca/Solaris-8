/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 *
 * Ye olde non-reentrant interface (MT-unsafe, caveat utor)
 */

#include <rpc/rpcent.h>
#include <rpc/trace.h>
#include <nss_dbdefs.h>

#ifdef	NSS_INCLUDE_UNSAFE

/*
 * Don't free this, even on an endrpcent(), because bitter experience shows
 * that there's production code that does getXXXbyYYY(), then endXXXent(),
 * and then continues to use the pointer it got back.
 */
static nss_XbyY_buf_t *buffer;
#define GETBUF()	\
	NSS_XbyY_ALLOC(&buffer, sizeof (struct rpcent), NSS_BUFLEN_RPC)
	/* === ?? set ENOMEM on failure?  */

struct rpcent *
getrpcbyname(nam)
	const char	*nam;
{
	nss_XbyY_buf_t	*b;
	struct rpcent	*res = 0;

	trace1(TR_getrpcbyname, 0);
	if ((b = GETBUF()) != 0) {
		res = getrpcbyname_r(nam, b->result, b->buffer, b->buflen);
	}
	trace1(TR_getrpcbyname, 1);
	return (res);
}

struct rpcent *
getrpcbynumber(num)
	int		num;
{
	nss_XbyY_buf_t	*b;
	struct rpcent	*res = 0;

	trace2(TR_getrpcbynumber, 0, num);
	if ((b = GETBUF()) != 0) {
		res = getrpcbynumber_r(num, b->result, b->buffer, b->buflen);
	}
	trace2(TR_getrpcbynumber, 1, num);
	return (res);
}

struct rpcent *
getrpcent()
{
	nss_XbyY_buf_t	*b;
	struct rpcent	*res = 0;

	trace1(TR_getrpcent, 0);
	if ((b = GETBUF()) != 0) {
		res = getrpcent_r(b->result, b->buffer, b->buflen);
	}
	trace1(TR_getrpcent, 1);
	return (res);
}

#endif	NSS_INCLUDE_UNSAFE
