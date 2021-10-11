/*
 * Copyright (c) 1991,1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)dlprims.c	1.6	99/10/20 SMI"	/* SunOS	*/

#include	<sys/types.h>
#include	<sys/stropts.h>
#include	<sys/signal.h>
#include	<sys/dlpi.h>
#include	<stdio.h>

#include	"snoop.h"

#define	DLMAXWAIT	(10)	/* max wait in seconds for response */
#define	DLMAXBUF	(256)

/*
 * Alarm timeout routine.
 */
static	void	sigalrm();

/*
 * Issue DL_INFO_REQ and wait for DL_INFO_ACK.
 */
dlinforeq(fd, infoackp)
int	fd;
dl_info_ack_t	*infoackp;
{
	union	DL_primitives	*dlp;
	char	buf[DLMAXBUF];
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->info_req.dl_primitive = DL_INFO_REQ;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_INFO_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = RS_HIPRI;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlinforeq:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dlinfoack");
	expecting(DL_INFO_ACK, dlp, "dlinfoack");

	if (ctl.len < DL_INFO_ACK_SIZE)
		err("dlinfoack:  response ctl.len too short:  %d", ctl.len);

	if (flags != RS_HIPRI)
		err("dlinfoack:  DL_INFO_ACK was not M_PCPROTO");

	if (infoackp)
		*infoackp = dlp->info_ack;
}

/*
 * Issue DL_ATTACH_REQ.
 * Return zero on success, nonzero on error.
 */
dlattachreq(fd, ppa)
int	fd;
u_long	ppa;
{
	union	DL_primitives	*dlp;
	char	buf[DLMAXBUF];
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
	dlp->attach_req.dl_ppa = ppa;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_ATTACH_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlattachreq:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dlattachreq");
	expecting(DL_OK_ACK, dlp, "dlattachreq");
}

/*
 * Issue DL_PROMISCON_REQ and wait for DL_OK_ACK.
 */
dlpromiscon(fd, level)
int	fd;
int	level;
{
	union	DL_primitives	*dlp;
	char	buf[DLMAXBUF];
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->promiscon_req.dl_primitive = DL_PROMISCON_REQ;
	dlp->promiscon_req.dl_level = level;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_PROMISCON_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlpromiscon:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dlpromisconreq");
	expecting(DL_OK_ACK, dlp, "dlpromisconreq");
}

/*
 * Issue DL_PROMISCOFF_REQ and wait for DL_OK_ACK.
 */
dlpromiscoff(fd, level)
int	fd;
int	level;
{
	union	DL_primitives	*dlp;
	char	buf[DLMAXBUF];
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->promiscoff_req.dl_primitive = DL_PROMISCON_REQ;
	dlp->promiscoff_req.dl_level = level;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_PROMISCOFF_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlpromiscoff:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dlpromiscoffreq");
	expecting(DL_OK_ACK, dlp, "dlpromiscoffreq");
}

dlenabmulti(fd, addrp, len)
int	fd;
char	*addrp;
int	len;
{
	char	buf[DLMAXBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->dl_primitive = DL_ENABMULTI_REQ;
	dlp->enabmulti_req.dl_addr_length = len;
	dlp->enabmulti_req.dl_addr_offset = DL_ENABMULTI_REQ_SIZE;
	memcpy((caddr_t) (buf + DL_ENABMULTI_REQ_SIZE), addrp, len);

	ctl.maxlen = sizeof (*dlp);
	ctl.len = DL_ENABMULTI_REQ + len;
	ctl.buf = buf;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlenabmultireq:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dlenabmultireq");
	expecting(DL_OK_ACK, dlp, "dlenabmultireq");
}

dldisabmulti(fd, addrp, len)
int	fd;
char	*addrp;
int	len;
{
	char	buf[DLMAXBUF];
	union	DL_primitives	*dlp;
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->dl_primitive = DL_DISABMULTI_REQ;
	dlp->enabmulti_req.dl_addr_length = len;
	dlp->enabmulti_req.dl_addr_offset = DL_DISABMULTI_REQ_SIZE;
	memcpy((caddr_t) (buf + DL_DISABMULTI_REQ_SIZE), addrp, len);

	ctl.maxlen = sizeof (*dlp);
	ctl.len = DL_DISABMULTI_REQ + len;
	ctl.buf = buf;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dldisabmultireq:  putmsg");

	strgetmsg(fd, &ctl, NULL, &flags, "dldisabmultireq");
	expecting(DL_OK_ACK, dlp, "dldisabmultireq");
}

dlbindreq(fd, sap, max_conind, service_mode, conn_mgmt)
int	fd;
u_long	sap;
u_long	max_conind;
u_short	service_mode;
u_short	conn_mgmt;
{
	union	DL_primitives	*dlp;
	char	buf[DLMAXBUF];
	struct	strbuf	ctl;
	int	flags;

	dlp = (union DL_primitives *) buf;

	dlp->bind_req.dl_primitive = DL_BIND_REQ;
	dlp->bind_req.dl_sap = sap;
	dlp->bind_req.dl_max_conind = max_conind;
	dlp->bind_req.dl_service_mode = service_mode;
	dlp->bind_req.dl_conn_mgmt = conn_mgmt;
	dlp->bind_req.dl_xidtest_flg = 0;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_BIND_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = 0;

	if (putmsg(fd, &ctl, NULL, flags) < 0)
		syserr("dlbindreq:  putmsg");

	ctl.len = 0;

	strgetmsg(fd, &ctl, NULL, &flags, "dlbindack");
	expecting(DL_BIND_ACK, dlp, "dlbindack");

	if (ctl.len < sizeof (DL_BIND_ACK_SIZE))
		err("dlbindack:  response ctl.len too short:  %d", ctl.len);
}

static void
sigalrm()
{
	(void) err("sigalrm:  TIMEOUT");
}

strgetmsg(fd, ctlp, datap, flagsp, caller)
int	fd;
struct	strbuf	*ctlp, *datap;
int	*flagsp;
char	*caller;
{
	int	rc;
	static	char	errmsg[80];

	/*
	 * Start timer.
	 */
	if (snoop_alarm(DLMAXWAIT, sigalrm) < 0) {
		sprintf(errmsg, "%s:  alarm", caller);
		syserr(errmsg);
	}

	/*
	 * Set flags argument and issue getmsg().
	 */
	*flagsp = 0;
	if ((rc = getmsg(fd, ctlp, datap, flagsp)) < 0) {
		sprintf(errmsg, "%s:  getmsg", caller);
		syserr(errmsg);
	}

	/*
	 * Stop timer.
	 */
	if (snoop_alarm(0, sigalrm) < 0) {
		sprintf(errmsg, "%s:  alarm", caller);
		syserr(errmsg);
	}

	/*
	 * Check for MOREDATA and/or MORECTL.
	 */
	if ((rc & (MORECTL | MOREDATA)) == (MORECTL | MOREDATA))
		err("%s:  strgetmsg:  MORECTL|MOREDATA", caller);
	if (rc & MORECTL)
		err("%s:  strgetmsg:  MORECTL", caller);
	if (rc & MOREDATA)
		err("%s:  strgetmsg:  MOREDATA", caller);

	/*
	 * Check for at least sizeof (long) control data portion.
	 */
	if (ctlp->len < sizeof (long))
		err("%s:  control portion length < sizeof (long)",
		caller);
}

expecting(prim, dlp, caller)
u_long	prim;
union	DL_primitives	*dlp;
char	*caller;
{
	if (dlp->dl_primitive == DL_ERROR_ACK) {
		err("%s:  DL_ERROR_ACK:  dl_errno %d unix_errno %d\n",
		caller,
		dlp->error_ack.dl_errno,
		dlp->error_ack.dl_unix_errno);
		return (1);
	}

	if (dlp->dl_primitive != prim) {
		err("%s:  unexpected primitive 0x%x received\n",
		caller,
		dlp->dl_primitive);
		return (1);
	}

	return (0);
}

err(fmt, a1, a2, a3, a4)
char	*fmt;
char	*a1, *a2, *a3, *a4;
{
	(void) fprintf(stderr, fmt, a1, a2, a3, a4);
	(void) fprintf(stderr, "\n");
	(void) exit(1);
}

syserr(s)
char	*s;
{
	perror(s);
	exit(1);
}
