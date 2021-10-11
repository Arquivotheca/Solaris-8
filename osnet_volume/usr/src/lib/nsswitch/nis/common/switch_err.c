/*
 *	switch_err.c
 *
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)switch_err.c	1.7	93/03/20 SMI"

#include <rpcsvc/ypclnt.h>
#include <nsswitch.h>

/*
 * maps errors returned by libnsl/yp routines into switch errors
 */

int
switch_err(ypclnt_err)
	int ypclnt_err;
{
	int serr;

	switch (ypclnt_err) {
	case 0:
		serr = __NSW_SUCCESS;
		break;
	case YPERR_BADARGS:
	case YPERR_KEY:
	case YPERR_NOMORE:
		serr = __NSW_NOTFOUND;
		break;
	case YPERR_RPC:
	case YPERR_DOMAIN:
	case YPERR_MAP:
	case YPERR_YPERR:
	case YPERR_RESRC:
	case YPERR_PMAP:
	case YPERR_YPBIND:
	case YPERR_YPSERV:
	case YPERR_NODOM:
	case YPERR_BADDB:
	case YPERR_VERS:
	case YPERR_ACCESS:
		serr = __NSW_UNAVAIL;
		break;
	case YPERR_BUSY:
		serr = __NSW_TRYAGAIN; /* :-) */
	}

	return (serr);
}
