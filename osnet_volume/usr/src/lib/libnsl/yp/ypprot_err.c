/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <sys/types.h>
#include <rpc/trace.h>

/*
 * Maps a yp protocol error code (as defined in
 * yp_prot.h) to a yp client interface error code (as defined in
 * ypclnt.h).
 */
int
ypprot_err(yp_protocol_error)
	int yp_protocol_error;
{
	int reason;
	trace2(TR_ypprot_err, 0, yp_protocol_error);
	switch (yp_protocol_error) {
	case YP_TRUE:
		reason = 0;
		break;
	case YP_NOMORE:
		reason = YPERR_NOMORE;
		break;
	case YP_NOMAP:
		reason = YPERR_MAP;
		break;
	case YP_NODOM:
		reason = YPERR_DOMAIN;
		break;
	case YP_NOKEY:
		reason = YPERR_KEY;
		break;
	case YP_BADARGS:
		reason = YPERR_BADARGS;
		break;
	case YP_BADDB:
		reason = YPERR_BADDB;
		break;
	case YP_VERS:
		reason = YPERR_VERS;
		break;
	default:
		reason = YPERR_YPERR;
		break;
	}
	trace1(TR_ypprot_err, 1);
	return (reason);
}
