/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

#include <rpcsvc/ypclnt.h>
#include <sys/types.h>
#include <rpc/trace.h>

/*
 * This returns a pointer to an error message string appropriate to an input
 * yp error code.  An input value of zero will return a success message.
 * In all cases, the message string will start with a lower case chararacter,
 * and will be terminated neither by a period (".") nor a newline.
 */

char *
yperr_string(code)
	int code;
{
	char *pmesg;

	trace2(TR_yperr_string, 0, code);
	switch (code) {

	case 0:  {
		pmesg = "yp operation succeeded";
		break;
	}

	case YPERR_BADARGS:  {
		pmesg = "args to yp function are bad";
		break;
	}

	case YPERR_RPC:  {
		pmesg = "RPC failure on yp operation";
		break;
	}

	case YPERR_DOMAIN:  {
		pmesg = "can't bind to a server which serves domain";
		break;
	}

	case YPERR_MAP:  {
		pmesg = "no such map in server's domain";
		break;
	}

	case YPERR_KEY:  {
		pmesg = "no such key in map";
		break;
	}

	case YPERR_YPERR:  {
		pmesg = "internal yp server or client error";
		break;
	}

	case YPERR_RESRC:  {
		pmesg = "local resource allocation failure";
		break;
	}

	case YPERR_NOMORE:  {
		pmesg = "no more records in map database";
		break;
	}

	case YPERR_PMAP:  {
		pmesg = "can't communicate with rpcbind";
		break;
		}

	case YPERR_YPBIND:  {
		pmesg = "can't communicate with ypbind";
		break;
		}

	case YPERR_YPSERV:  {
		pmesg = "can't communicate with ypserv";
		break;
		}

	case YPERR_NODOM:  {
		pmesg = "local domain name not set";
		break;
	}

	case YPERR_BADDB:  {
		pmesg = "yp map data base is bad";
		break;
	}

	case YPERR_VERS:  {
		pmesg = "yp client/server version mismatch";
		break;
	}

	case YPERR_ACCESS: {
		pmesg = "permission denied";
		break;
	}

	case YPERR_BUSY: {
		pmesg = "database is busy";
		break;
	}

	default:  {
		pmesg = "unknown yp client error code";
		break;
	}

	}

	trace1(TR_yperr_string, 1);
	return (pmesg);
}
