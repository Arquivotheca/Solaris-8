#ident	"@(#)dlprims.c	1.12	97/03/27 SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#include	<stdio.h>
#include	<errno.h>
#include	<sys/types.h>
#include	<string.h>
#include	<sys/sysmacros.h>
#include	<sys/stropts.h>
#include	<sys/dlpi.h>
#include	<sys/syslog.h>
#include	<stropts.h>
#include	<sys/poll.h>
#include	<locale.h>

#define	DLMAXWAIT	(10)	/* max wait in seconds for response */
#define	DLMAXBUF	(256)

extern void dhcpmsg(int, ...);
static int strgetmsg(int, struct strbuf *, struct strbuf *, int *, char *);
static int expecting(u_long, union DL_primitives *, char *);

/*
 * Issue DL_INFO_REQ and wait for DL_INFO_ACK. Returns 0 for success, 1
 * otherwise.
 */
int
dlinforeq(int fd, dl_info_ack_t *infoackp)
{
	union DL_primitives	*dlp;
	long			buf[DLMAXBUF / sizeof (long)];
	struct strbuf		ctl;
	int			flags;
	register int		err = 0;

	/* ALIGNED */
	dlp = (union DL_primitives *) buf;

	dlp->info_req.dl_primitive = DL_INFO_REQ;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_INFO_REQ_SIZE;
	ctl.buf = (char *) dlp;

	flags = RS_HIPRI;

	if (putmsg(fd, &ctl, NULL, flags) < 0) {
		dhcpmsg(LOG_ERR, "DL_INFO_REQ: %s.\n", strerror(errno));
		return (errno);
	}
	if ((err = strgetmsg(fd, &ctl, NULL, &flags, "dlinfoack")) != 0)
		return (err);
	if ((err = expecting(DL_INFO_ACK, dlp, "dlinfoack")) != 0)
		return (err);
	if (ctl.len < DL_INFO_ACK_SIZE) {
		dhcpmsg(LOG_ERR,
		    "DL_INFO_REQ: %s, too short: %d\n", strerror(errno),
		    ctl.len);
		return (1);
	}
	if (flags != RS_HIPRI) {
		dhcpmsg(LOG_ERR, "DL_INFO_REQ: %s, not M_PCPROTO.\n",
		    strerror(errno));
		return (1);
	}
	if (infoackp)
		*infoackp = dlp->info_ack;

	return (0);
}
/*
 * Issue DL_ATTACH_REQ. Return zero on success, nonzero on error.
 */
int
dlattachreq(int fd, u_long ppa)
{
	union DL_primitives	*dlp;
	long			buf[DLMAXBUF / sizeof (long)];
	struct strbuf		ctl;
	int			flags = 0;
	register int		err = 0;

	dlp = (union DL_primitives *) buf;

	dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
	dlp->attach_req.dl_ppa = ppa;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_ATTACH_REQ_SIZE;
	ctl.buf = (char *)dlp;

	if (putmsg(fd, &ctl, NULL, flags) < 0) {
		dhcpmsg(LOG_ERR, "DL_ATTACH_REQ: %s\n", strerror(errno));
		return (errno);
	}
	if ((err = strgetmsg(fd, &ctl, NULL, &flags, "dlattachreq")) != 0)
		return (err);

	return (expecting(DL_OK_ACK, dlp, "dlattachreq"));
}
/*
 * Issue DL_DETACH_REQ. Return zero on success, nonzero on error.
 */
int
dldetachreq(int fd)
{
	union DL_primitives	*dlp;
	long			buf[DLMAXBUF / sizeof (long)];
	struct strbuf		ctl;
	int			flags = 0;
	register int		err = 0;

	/* ALIGNED */
	dlp = (union DL_primitives *) buf;

	dlp->detach_req.dl_primitive = DL_DETACH_REQ;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_DETACH_REQ_SIZE;
	ctl.buf = (char *) dlp;

	if (putmsg(fd, &ctl, NULL, flags) < 0) {
		dhcpmsg(LOG_ERR, "DL_DETACH_REQ: %s.\n", strerror(errno));
		return (errno);
	}
	if ((err = strgetmsg(fd, &ctl, NULL, &flags, "dldetachreq")) != 0)
		return (err);

	return (expecting(DL_OK_ACK, dlp, "dldetachreq"));
}
/*
 * Issue DL_BIND_REQ. Returns 0 for success, nonzero otherwise.
 */
int
dlbindreq(int fd, u_long sap, u_long max_conind, u_short service_mode,
    u_short conn_mgmt)
{
	union DL_primitives	*dlp;
	long			buf[DLMAXBUF / sizeof (long)];
	struct strbuf		ctl;
	int			flags = 0;
	register int		err = 0;

	/* ALIGNED */
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

	if (putmsg(fd, &ctl, NULL, flags) < 0) {
		dhcpmsg(LOG_ERR, "DL_BIND_REQ: %s.\n", strerror(errno));
		return (errno);
	}

	ctl.len = 0;

	if ((err = strgetmsg(fd, &ctl, NULL, &flags, "dlbindack")) != 0)
		return (err);

	if ((err = expecting(DL_BIND_ACK, dlp, "dlbindack")) != 0)
		return (err);

	if (ctl.len < sizeof (DL_BIND_ACK_SIZE)) {
		dhcpmsg(LOG_ERR, "DL_BIND_REQ: %s, too short: %d\n",
		    strerror(errno), ctl.len);
		return (1);
	}
	return (0);
}
/*
 * Issue DL_UNBIND_REQ. Returns 0 for success, nonzero otherwise.
 */
int
dlunbindreq(int fd)
{
	union DL_primitives	*dlp;
	long			buf[DLMAXBUF / sizeof (long)];
	struct strbuf		ctl;
	int			flags = 0;
	register int		err = 0;

	/* ALIGNED */
	dlp = (union DL_primitives *) buf;

	dlp->unbind_req.dl_primitive = DL_UNBIND_REQ;

	ctl.maxlen = DLMAXBUF;
	ctl.len = DL_UNBIND_REQ_SIZE;
	ctl.buf = (char *) dlp;

	if (putmsg(fd, &ctl, NULL, flags) < 0) {
		dhcpmsg(LOG_ERR, "DL_UNBIND_REQ: %s.\n", strerror(errno));
		return (errno);
	}

	ctl.len = 0;

	if ((err = strgetmsg(fd, &ctl, NULL, &flags, "dlunbindack")) != 0)
		return (err);

	return (expecting(DL_OK_ACK, dlp, "dlunbindack"));
}
/*
 * Returns: 0 for success, 1 otherwise.
 */
static int
strgetmsg(int fd, struct strbuf *ctlp, struct strbuf *datap, int *flagsp,
	char *caller)
{
	register int	err = 0;
	register int	rc;
	struct pollfd	fds;

	fds.fd = fd;
	fds.events = POLLIN | POLLPRI;
	fds.revents = 0;

	if ((err = poll(&fds, 1, DLMAXWAIT * 1000)) < 0) {
		dhcpmsg(LOG_ERR, "Poll: %s, Caller: %s\n", strerror(errno),
		    caller);
		return (errno);
	}
	if (err == 0) {
		dhcpmsg(LOG_ERR,
"Timed out waiting to response from STREAMS  message from caller: %s\n",
		    caller);
		return (ETIME);
	}

	/*
	 * Set flags argument and issue getmsg().
	 */
	*flagsp = 0;
	if ((rc = getmsg(fd, ctlp, datap, flagsp)) < 0) {
		dhcpmsg(LOG_ERR,
		    "GET: %s, caller %s.\n", strerror(errno), caller);
		return (errno);
	}
	/*
	 * Check for MOREDATA and/or MORECTL.
	 */
	if ((rc & (MORECTL | MOREDATA)) == (MORECTL | MOREDATA)) {
		dhcpmsg(LOG_ERR, "Unexpected: %s, Caller: %s\n",
		    strerror(errno), caller);
		return (1);
	}
	if (rc & MORECTL) {
		dhcpmsg(LOG_ERR, "Unexpected: %s, Caller: %s\n",
		    strerror(errno), caller);
		return (1);
	}
	if (rc & MOREDATA) {
		dhcpmsg(LOG_ERR, "Unexpected: %s, Caller: %s\n",
		    strerror(errno), caller);
		return (1);
	}

	/*
	 * Check for at least sizeof (long) control data portion.
	 */
	if (ctlp->len < sizeof (long)) {
		dhcpmsg(LOG_ERR, "Control message is too short (less than 4 \
octets). Caller: %s\n",
		    caller);
		return (1);
	}
	return (0);
}

static const char *
dl_error_msg(int code, char *bufp, int len)
{
	char	*msg;

	switch (code) {
	case DL_ACCESS:
		msg = "Improper permissions for request";
		break;
	case DL_BADADDR:
		msg = "DLSAP addr in improper format or invalid";
		break;
	case DL_BADCORR:
		msg = "Seq number not from outstand DL_CONN_IND";
		break;
	case DL_BADDATA:
		msg = "User data exceeded provider limit";
		break;
	case DL_BADPPA:
		msg = "Specified PPA was invalid";
		break;
	case DL_BADPRIM:
		msg = "Primitive received not known by provider";
		break;
	case DL_BADQOSPARAM:
		msg = "QOS parameters contained invalid values";
		break;
	case DL_BADQOSTYPE:
		msg = "QOS structure type is unknown/unsupported";
		break;
	case DL_BADSAP:
		msg = "Bad LSAP selector";
		break;
	case DL_BADTOKEN:
		msg = "Token used not an active stream";
		break;
	case DL_BOUND:
		msg = "Attempted second bind with dl_max_conind";
		break;
	case DL_INITFAILED:
		msg = "Physical Link initialization failed";
		break;
	case DL_NOADDR:
		msg = "Provider couldn't allocate alt. address";
		break;
	case DL_NOTINIT:
		msg = "Physical Link not initialized";
		break;
	case DL_OUTSTATE:
		msg = "Primitive issued in improper state";
		break;
	case DL_SYSERR:
		msg = "UNIX system error occurred";
		break;
	case DL_UNSUPPORTED:
		msg = "Requested serv. not supplied by provider";
		break;
	case DL_UNDELIVERABLE:
		msg = "Previous data unit could not be delivered";
		break;
	case DL_NOTSUPPORTED:
		msg = "Primitive is known but not supported";
		break;
	case DL_TOOMANY:
		msg = "Limit exceeded";
		break;
	case DL_NOTENAB:
		msg = "Promiscuous mode not enabled";
		break;
	case DL_BUSY:
		msg = "Other streams for PPA in post-attached";
		break;
	case DL_NOAUTO:
		msg = "Automatic handling XID&TEST not supported";
		break;
	case DL_NOXIDAUTO:
		msg = "Automatic handling of XID not supported";
		break;
	case DL_NOTESTAUTO:
		msg = "Automatic handling of TEST not supported";
		break;
	case DL_XIDAUTO:
		msg = "Automatic handling of XID response";
		break;
	case DL_TESTAUTO:
		msg = "Automatic handling of TEST response";
		break;
	case DL_PENDING:
		msg = "Pending outstanding connect indications";
		break;
	default:
		msg = "Unknown dl_error";
		break;
	}
	if (bufp != NULL) {
		if (len > strlen(msg))
			len = strlen(msg) + 1;
		(void) memcpy(bufp, msg, len);
		bufp[len] = '\0';
	}
	return (bufp);
}

/*
 * returns 0 for success, 1 otherwise.
 */
static int
expecting(u_long prim, union DL_primitives *dlp, char *caller)
{
	char	buf[BUFSIZ];

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		dhcpmsg(LOG_ERR,
"DL_ERROR_ACK: dl_error: (%d - %s)\n\tunix_errno: %d from caller: %s\n",
		    dlp->error_ack.dl_errno,
		    dl_error_msg(dlp->error_ack.dl_errno, buf, sizeof (buf)),
		    dlp->error_ack.dl_unix_errno,
		    caller);
		return (1);
	}
	if (dlp->dl_primitive != prim) {
		dhcpmsg(LOG_ERR,
		    "Unexpected DLPI primitive: 0x%x received. Caller: %s\n",
		    dlp->dl_primitive, caller);
		return (1);
	}
	return (0);
}
