#pragma ident	"@(#)rusersxdr.c	1.9	97/05/29 SMI" 

/*
 * rusersxdr.c
 * These are the non-rpcgen-able XDR routines for version 2 of the rusers
 * protocol.
 *
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <rpc/rpc.h>
#include <rpcsvc/rusers.h>

xdr_ru_utmp(xdrsp, up)
	XDR *xdrsp;
	struct ru_utmp *up;
{
	u_int len;
	char *p;

	/*
	 * This code implements demented byte vectors:  we send out the length
	 * of fixed-length vectors, followed by the opaque bytes.  This is to
	 * be compatible with the over-the-wire protocol, as well as the
	 * rusers.h definition for struct ru_utmp.
	 */
	len = (int)sizeof (up->ut_line);
	if (xdr_u_int(xdrsp, &len) == FALSE)
		return (0);
	if (len != sizeof (up->ut_line)) {
		return (0);
	}
	if (!xdr_opaque(xdrsp, (char *)up->ut_line, len)) {
		return (0);
	}
	len = (int)sizeof (up->ut_name);
	if (xdr_u_int(xdrsp, &len) == FALSE)
		return (0);
	if (len != sizeof (up->ut_name)) {
		return (0);
	}
	if (!xdr_opaque(xdrsp, (char *)up->ut_name, len)) {
		return (0);
	}
	len = (int)sizeof (up->ut_host);
	if (xdr_u_int(xdrsp, &len) == FALSE)
		return (0);
	if (len != sizeof (up->ut_host)) {
		return (0);
	}
	if (!xdr_opaque(xdrsp, (char *)up->ut_host, len)) {
		return (0);
	}
	if (xdr_int(xdrsp, (int32_t *) &up->ut_time) == FALSE)
		return (0);
	return (1);
}

xdr_utmpidle(xdrsp, ui)
	XDR *xdrsp;
	struct utmpidle *ui;
{
	if (xdr_ru_utmp(xdrsp, &ui->ui_utmp) == FALSE)
		return (0);
	if (xdr_u_int(xdrsp, &ui->ui_idle) == FALSE)
		return (0);
	return (1);
}

xdr_utmpidleptr(xdrsp, up)
	XDR *xdrsp;
	struct utmpidle **up;
{
	if (xdr_reference(xdrsp, (char **) up, sizeof (struct utmpidle),
			xdr_utmpidle) == FALSE)
		return (0);
	return (1);
}

xdr_utmpidlearr(xdrsp, up)
	XDR *xdrsp;
	struct utmpidlearr *up;
{
	return (xdr_array(xdrsp, (char **) &up->uia_arr,
		(u_int *)&(up->uia_cnt), MAXUSERS, sizeof (struct utmpidle *),
		xdr_utmpidleptr));
}
