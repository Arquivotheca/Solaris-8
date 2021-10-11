/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ppp_diag.c	1.18	98/09/30 SMI"

/*
 * ppp_diag.c
 *
 * Diagnostics module for ppp.	Pushed on stream below ppp, it looks at
 * data packets going to or from the ppp module and reports on them.
 */

#include <sys/ioctl.h>
#include <sys/systm.h>
#include <sys/stropts.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/ddi.h>
#include <sys/dlpi.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/strlog.h>
#include <sys/modctl.h>
#include <sys/kmem.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include <sys/vjcomp.h>
#include <sys/ppp_ioctl.h>
#include <sys/ppp_sys.h>
#include <sys/ppp_pap.h>
#include <sys/ppp_chap.h>
#include <sys/ppp_lqm.h>
#include <sys/ppp_diag.h>

#define	UPSTR	1
#define	DOWNSTR 2

#define	HEXDLIM 20
#define	PROTOHEXLIM 200

#define	PPP_FCS(fcs, c) (((fcs) >> 8) ^ fcstab[((fcs) ^ (c)) & 0xff])
#define	PPP_GOODFCS	(0xf0b8)
#define	PPP_INITFCS	(0xffff)

typedef struct {
	int accmvalid, didlcpreq;
	unsigned int accmask;
	unsigned int upmask, downmask;
	ppp_diag_conf_t diag_info;
	strlog_struct_t strlog_info;
	mblk_t *dcp, *dnp;
	mblk_t *ucp, *unp;
	char outputbuf[PPP_DG_MAX_OUTPUT+1];
	int outputbuflen;
} ppp_diag_struct_t;

/*
 * FCS lookup table as calculated by genfcstab.
 */
ushort_t fcstab[256] = {
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};
struct module_info rminfo =
{PPP_DG_MOD_ID, "ppp_diag", 0, INFPSZ, 2048, 128 };

#define	DOING_ASCII_NAME

static int chrsrv(queue_t *), chwsrv(queue_t *);
static int chrpu(queue_t *, mblk_t *), chwpu(queue_t *, mblk_t *);
static int mclose(queue_t *, int, cred_t *);
static int mopen(queue_t *, dev_t *, int, int, cred_t *);
static void data_diag(mblk_t *, int, int, ppp_diag_struct_t *);
static void outbuf(int, char *, ppp_diag_struct_t *);
static void diag_ioctl(queue_t *, mblk_t *, ppp_diag_struct_t *);
static void *check_param(mblk_t *, size_t);
static int process_lcp_opts(uchar_t **, int *, ppp_diag_struct_t *);
static int process_ipcp_opts(uchar_t **, int *, ppp_diag_struct_t *);
static int process_ipcp(uchar_t **, int *, ppp_diag_struct_t *);
static int process_pap(uchar_t **, int *, ppp_diag_struct_t *);
static int process_chap(uchar_t **, int *, ppp_diag_struct_t *);
static int process_lcp(uchar_t **, int *, ppp_diag_struct_t *);
static int process_proto_rej(uchar_t **, int *, ppp_diag_struct_t *);
static int process_echo(uchar_t **, int *, ppp_diag_struct_t *);
static int process_discard(uchar_t **, int *, ppp_diag_struct_t *);
static int process_lqm(uchar_t **, int *, ppp_diag_struct_t *);
static void hexdump(int, unsigned char *, int, ppp_diag_struct_t *);
static void trunchexdump(int, uchar_t *, int, int, ppp_diag_struct_t *);
static int decompress_frame(mblk_t *, int *, int *, ushort_t *,
    ushort_t *, ppp_diag_struct_t *);
static void init_strlog(int, int, int, int, ppp_diag_struct_t *);
static int filter_esc(int, mblk_t *, ppp_diag_struct_t *);
static void outputline(ppp_diag_struct_t *);
static void process(mblk_t *, int, ppp_diag_struct_t *);
static void report_hangup(ppp_diag_struct_t *);
static void ppp_diag_set_conf(queue_t *, ppp_diag_conf_t *,
    ppp_diag_struct_t *);
static int ppp_diag_get_conf(ppp_diag_conf_t *, ppp_diag_struct_t *);
static LQM_pack_t ntoh_LQM_pack_t(LQM_pack_t);
static void free_diag_struct(ppp_diag_struct_t *dp);



struct qinit rinit = {
	chrpu,
	chrsrv,
	mopen,
	mclose,
	NULL,
	&rminfo
};

struct qinit winit = {
	chwpu,
	chwsrv,
	mopen,
	mclose,
	NULL,
	&rminfo
};

struct streamtab com_diaginfo = {
	&rinit,
	&winit,
	NULL,
	NULL
};

static struct fmodsw fsw = {
	"ppp_diag",
	&com_diaginfo,
	D_NEW|D_MP
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops,
	"ppp diagnostics",
	&fsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlstrmod,
	NULL
};

_init(void)
{
	return (mod_install(&modlinkage));
}

_fini(void)
{

	return (mod_remove(&modlinkage));
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
mopen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	char buf[] = "PPP DIAG OPEN";
	ppp_diag_conf_t			dconf;
	ppp_diag_struct_t *dp;

	if (sflag != MODOPEN) {		/* can only be opened as a module */
		return (-1);
	}

	if (q->q_ptr) {			/* already opened? */
		return (0);
	}


	dp = kmem_zalloc(sizeof (ppp_diag_struct_t), KM_NOSLEEP);

	if (dp == NULL)
		return (-1);

	WR(q)->q_ptr = q->q_ptr = (caddr_t)dp;

	dp->upmask = 0xffffffffU;
	dp->downmask = 0xffffffffU;
	dconf.media_type = DEFAULT_MEDIA;
	dconf.outputdest = DEFAULT_OUT;
	dconf.tracelevel = DEFAULT_TRACE;
	dconf.debuglevel = DEFAULT_DEBUGLEVEL;
	ppp_diag_set_conf(q, &dconf, dp);

	qprocson(q);
	outbuf(PPP_DG_INF, buf, dp);
	outputline(dp);
	return (0);
}

/*ARGSUSED*/
static int
mclose(queue_t *q, int flag, cred_t *credp)
{
	char buf[] = "PPP DIAG CLOSE";
	ppp_diag_struct_t *dp;

	qprocsoff(q);
	dp = (ppp_diag_struct_t *)q->q_ptr;
	outbuf(PPP_DG_INF, buf, dp);
	outputline(dp);
	free_diag_struct(dp);
	return (0);
}

static int
chrsrv(queue_t *q)
{
	ppp_diag_struct_t *dp;
	mblk_t *bp;

	dp = (ppp_diag_struct_t *)q->q_ptr;

	while ((bp = getq(q)) != NULL) {
		process(bp, UPSTR, dp);
	}
	return (0);
}

static int
chwsrv(queue_t *q)
{
	ppp_diag_struct_t *dp;
	mblk_t *bp;

	dp = (ppp_diag_struct_t *)q->q_ptr;

	while ((bp = getq(q)) != NULL) {
		process(bp, DOWNSTR, dp);
	}
	return (0);
}


static int
chrpu(queue_t *q, mblk_t *mp)
{
	mblk_t *bp;

	bp = copymsg(mp);
	putnext(q, mp);
	(void) putq(q, bp);
	return (0);
}

static int
chwpu(queue_t *q, mblk_t *mp)
{
	mblk_t *bp;
	ppp_diag_struct_t *dp;

	switch (mp-> b_datap-> db_type) {
	case M_IOCTL:
		dp = (ppp_diag_struct_t *)q->q_ptr;
		diag_ioctl(q, mp, dp);
		break;
	default:
		/* ISDN SPECIFIC FIX */

		bp = copymsg(mp);
		if (bp) {
			(void) putq(q, bp);
		}
		putnext(q, mp);
		break;

		/* END OF ISDN SPECIFIC FIX */
	}
	return (0);
}

static void
diag_ioctl(queue_t *q, mblk_t *mp, ppp_diag_struct_t *dp)
{
	struct iocblk			*iocp;
	ppp_diag_conf_t			*dconf;
	int				rc;

	iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	case PPP_DIAG_SET_CONF:
		dconf = (ppp_diag_conf_t *)check_param(mp, sizeof (*dconf));
		if (dconf == NULL) {
			goto iocnak;
		}
		ppp_diag_set_conf(q, dconf, dp);
		if (dconf == NULL) {
			iocp->ioc_error = -1;
			goto iocnak;
		}
		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCACK;

		qreply(q, mp);

		break;
	case PPP_DIAG_GET_CONF:
		dconf = (ppp_diag_conf_t *)check_param(mp, sizeof (*dconf));
		if (dconf == NULL) {
			goto iocnak;
		}
		rc = ppp_diag_get_conf(dconf, dp);
		if (rc) {
			iocp->ioc_error = rc;
			goto iocnak;
		}
		mp->b_datap->db_type = M_IOCACK;

		qreply(q, mp);

		break;
	default:
		putnext(q, mp);
		break;
	}
	return;
iocnak:
		iocp->ioc_count = 0;
		mp->b_datap->db_type = M_IOCNAK;
		qreply(q, mp);
}

static void
ppp_diag_set_conf(queue_t *q, ppp_diag_conf_t *dconf, ppp_diag_struct_t *dp)
{
	dp -> diag_info.media_type = dconf -> media_type;
	dp -> diag_info.outputdest = dconf -> outputdest;
	dp -> diag_info.tracelevel = dconf -> tracelevel;
	dp -> diag_info.debuglevel = dconf -> debuglevel;
	dp -> diag_info.ifid = dconf -> ifid;
	switch (dp -> diag_info.outputdest) {
	case PPP_DG_STRLOG_DEST:
		init_strlog(q->q_qinfo->qi_minfo->mi_idnum,
		    dp -> diag_info.ifid, dp->diag_info.tracelevel,
		    SL_TRACE, dp);
		break;
	}
}

static int
ppp_diag_get_conf(ppp_diag_conf_t *dconf, ppp_diag_struct_t *dp)
{
	dconf -> media_type = dp -> diag_info.media_type;
	dconf -> outputdest = dp -> diag_info.outputdest;
	dconf -> tracelevel = dp -> diag_info.tracelevel;
	dconf -> debuglevel = dp -> diag_info.debuglevel;
	dconf -> ifid = dp -> diag_info.ifid;

	return (0);
}


static void *
check_param(mblk_t *mp, size_t size)
{
	struct iocblk	*iocp;

	ASSERT(size != 0);

/*
 * check parameter size
 */
	iocp = (struct iocblk *)mp->b_rptr;
	if (iocp->ioc_count != size) {
		return (NULL);
	}

	return (mp->b_cont->b_rptr);
}


static void
report_hangup(ppp_diag_struct_t *dp)
{
	char buf[] = "PPP ASYNC DEVICE HANGUP";
	outbuf(PPP_DG_INF, buf, dp);
	outputline(dp);
}

static void
process(mblk_t *mp, int direc, ppp_diag_struct_t *dp)
{
	mblk_t *bp, *np, *cp, *zmp;
	uchar_t *check;
	char bflag = 0;
	char idlin[20];

	if (!ISPULLEDUP(mp)) {
		zmp = msgpullup(mp, -1);
		freemsg(mp);
		mp = zmp;
		if (mp == NULL) {
			return;
		}
	}


	if (direc == UPSTR) {
		cp = dp -> ucp;
		np = dp -> unp;
	} else {
		cp = dp -> dcp;
		np = dp -> dnp;
	}

	bp = mp;
	check = (uchar_t *)bp-> b_rptr;
	switch (mp-> b_datap-> db_type) {
	case M_HANGUP:
		report_hangup(dp);
		break;

	case M_DATA:
		/* Print raw data if at that debug level */

		outbuf(PPP_DG_RAW, (direc == UPSTR) ? "RECEIVE " : "SEND ",
		    dp);
		(void) sprintf(idlin, "%lu octets: ", msgdsize(mp));
		outbuf(PPP_DG_RAW, idlin, dp);
		hexdump(PPP_DG_RAW, check, (int)msgdsize(mp), dp);
		outputline(dp);

		if (dp -> diag_info.media_type == pppAsync) {
			bp = mp;
			while (check < (uchar_t *)bp-> b_wptr) {
				if (*check == 0x7e) {
					if (cp != NULL) {
						data_diag(cp, direc, bflag,
						    dp);
						/* Assume data_diag frees cp */
						bflag = 0;
						cp = np = NULL;
					} else {
						bflag = 1;
					}
				} else {
					if (np == NULL ||
						np-> b_wptr >= np-> b_datap->
							db_lim) {
						np = (mblk_t *)
						    allocb(1000, BPRI_MED);
						if (cp == NULL) {
							cp = np;
						} else {
							linkb(cp, np);
						}
					}
					*(np-> b_wptr++) = *check;
				}
				check++;
			}
			freemsg(mp);
		} else {
			data_diag(mp, (int)direc, (int)bflag, dp);
			/* Assume data_diag frees mp */
		}
		break;
	default:
		break;
	}
	if (dp -> diag_info.media_type == pppAsync) {
		if (direc == UPSTR) {
			dp -> ucp = cp;
			dp -> unp = np;
		} else {
			dp -> dcp = cp;
			dp -> dnp = np;
		}
	}
}


static void
trunchexdump(int debuglevel, uchar_t *buf, int cnt, int max_cnt,
		ppp_diag_struct_t *dp)
{
	int scnt;

	scnt = (cnt <= max_cnt ? cnt : max_cnt);
	hexdump(debuglevel, buf, scnt, dp);
	if (scnt < cnt) {
		outbuf(debuglevel, "..... ", dp);
	}
}

static void
hexdump(int debuglevel, uchar_t *buf, int cnt, ppp_diag_struct_t *dp)
{
	int i;
	unsigned int val;
	char valstr[10];

	if (dp -> diag_info.debuglevel & debuglevel) {
		for (i = 0; i < cnt; i++) {
			val = (unsigned int)buf[i] & 0xff;
			(void) sprintf(valstr, "%02x ", val);
			outbuf(debuglevel, valstr, dp);
		}
	}
}

void
outbuf(int debuglevel, char *src, ppp_diag_struct_t *dp)
{
	if (PPP_DG_MAX_OUTPUT - dp->outputbuflen - 1 <= 0)
		return;
	if (dp->diag_info.debuglevel & debuglevel) {
		(void) strncpy(&dp->outputbuf[dp->outputbuflen], src,
					PPP_DG_MAX_OUTPUT - dp->outputbuflen);
	if ((int)strlen(src) >= PPP_DG_MAX_OUTPUT - dp->outputbuflen)
		dp->outputbuflen = PPP_DG_MAX_OUTPUT;
	else
		dp->outputbuflen += (int)strlen(src);
	dp->outputbuf[dp->outputbuflen] = '\0';
	}
}

static void
init_strlog(int mid, int sid, int tracelevel, int flags, ppp_diag_struct_t *dp)
{
	dp -> strlog_info.mid = mid;
	dp -> strlog_info.sid = sid;
	dp -> strlog_info.tracelevel = tracelevel;
	dp -> strlog_info.flags = flags;
}

static void
outputline(ppp_diag_struct_t *dp)
{
	if (dp -> outputbuflen == 0)
		return;
	switch (dp -> diag_info.outputdest) {
	case PPP_DG_STRLOG_DEST:
		(void) strlog(dp -> strlog_info.mid, dp -> strlog_info.sid,
		    dp -> strlog_info.tracelevel,
		    dp -> strlog_info.flags, dp -> outputbuf);
	}
	dp -> outputbuflen = 0;
}

static void
data_diag(mblk_t *mp, int direc, int bflag, ppp_diag_struct_t *dp)
{
	uchar_t *bufp;
	int cnt;
	ushort_t proto, fcs;
	char valstr[30];
	mblk_t *zmp;
	int addrcomp, protocomp;


	if (!ISPULLEDUP(mp)) {
		zmp = msgpullup(mp, -1);
		freemsg(mp);
		mp = zmp;
		if (mp == NULL) {
			return;
		}
	}


	outbuf(PPP_DG_INF, (direc == UPSTR) ? "RECEIVE " : "SEND ", dp);

	bufp = mp-> b_rptr;
	if (dp->diag_info.media_type == pppAsync) {
		dp -> accmvalid = 0;
		dp -> didlcpreq = 0;

		if (filter_esc(direc, mp, dp) < 0) {
			outbuf(PPP_DG_ERR, "{Remove esc char failed} ", dp);
			outbuf(PPP_DG_ERRREP, "data: ", dp);
			hexdump(PPP_DG_ERRREP, mp -> b_rptr, (int)msgdsize(mp),
				dp);
			outputline(dp);
			return;
		}

		if (!decompress_frame(mp, &addrcomp, &protocomp,
		    &proto, &fcs, dp)) {
			outbuf(PPP_DG_ERR,
			    "{Bad frame compression} data: ", dp);
			outbuf(PPP_DG_ERRREP, "data: ", dp);
			hexdump(PPP_DG_ERRREP, mp -> b_rptr, (int)msgdsize(mp),
				dp);
			outputline(dp);
			return;
		}
		outbuf(PPP_DG_RFR, "( ", dp);
		trunchexdump(PPP_DG_RFR, bufp, (int)msgdsize(mp),
				PROTOHEXLIM, dp);
		outbuf(PPP_DG_RFR, ") ", dp);
		bufp = mp-> b_rptr;
		cnt = msgdsize(mp);

		outbuf(PPP_DG_INF, "PPP ASYNC ", dp);
/*
 * Octet count for LQM stuff, add the addr-proto fields
 * and one byte for flag.
 */
		(void) sprintf(valstr, "%u Octets ", cnt+5);

		outbuf(PPP_DG_INF, valstr, dp);
		if (fcs != PPP_GOODFCS) {
			outbuf(PPP_DG_ERR, "{BAD FCS} ", dp);
		}
		if (!bflag) {
			outbuf(PPP_DG_EXT, "NB ", dp);
		}
	} else {
		if (!decompress_frame(mp, &addrcomp, &protocomp,
		    &proto, &fcs, dp)) {
			outbuf(PPP_DG_ERR, "{Bad frame compression} data: ",
			    dp);
			outbuf(PPP_DG_ERRREP, "data: ", dp);
			hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			outputline(dp);
			return;
		}
		outbuf(PPP_DG_RFR, "( ", dp);
		trunchexdump(PPP_DG_RFR, bufp, (int)msgdsize(mp), PROTOHEXLIM,
				dp);
		outbuf(PPP_DG_RFR, ") ", dp);
		outbuf(PPP_DG_INF, "PPP SYNC ", dp);
		bufp = mp-> b_rptr;
		cnt = msgdsize(mp);
/*
 *
 * Octet count for LQM stuff, add the addr-proto fields
 * and one byte for flag.
 */
		(void) sprintf(valstr, "%u Octets ", cnt+5);

		outbuf(PPP_DG_INF, valstr, dp);
	}
	if (addrcomp || protocomp) {
		outbuf(PPP_DG_EXT, "(", dp);
		if (addrcomp)
			outbuf(PPP_DG_EXT, "A", dp);
		if (protocomp)
			outbuf(PPP_DG_EXT, "P", dp);
		outbuf(PPP_DG_EXT, ") ", dp);
	}
	switch (proto) {
		case pppIP_PROTO:
			outbuf(PPP_DG_INF, "IP_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppOSI_PROTO:
			outbuf(PPP_DG_INF, "OSI_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppXNS_PROTO:
			outbuf(PPP_DG_INF, "XNS_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppDECNET_PROTO:
			outbuf(PPP_DG_INF, "DECNET_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppAPPLETALK_PROTO:
			outbuf(PPP_DG_INF, "APPLETALK_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppIPX_PROTO:
			outbuf(PPP_DG_INF, "IPX_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppVJ_COMP_TCP:
			outbuf(PPP_DG_INF, "VJ_COMP_TCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppVJ_UNCOMP_TCP:
			outbuf(PPP_DG_INF, "VJ_UNCOMP_TCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppBRIDGING_PDU:
			outbuf(PPP_DG_INF, "BRIDGING_PDU ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppSTREAM_PROTO:
			outbuf(PPP_DG_INF, "STREAM_PROTO ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppBANYAN_VINES:
			outbuf(PPP_DG_INF, "BANYAN_VINES ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case ppp802_1D:
			outbuf(PPP_DG_INF, "802_1D ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppLUXCOM:
			outbuf(PPP_DG_INF, "LUXCOM ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppSIGMA:
			outbuf(PPP_DG_INF, "SIGMA ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppIP_NCP:
			outbuf(PPP_DG_INF, "IP_NCP ", dp);
			if (process_ipcp(&bufp, &cnt, dp) < 0) {
				hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			}
			break;
		case pppOSI_NCP:
			outbuf(PPP_DG_INF, "OSI_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppXNS_NCP:
			outbuf(PPP_DG_INF, "XNS_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppDECNET_NCP:
			outbuf(PPP_DG_INF, "DECNET_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppAPPLETALK_NCP:
			outbuf(PPP_DG_INF, "APPLETALK_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppIPX_NCP:
			outbuf(PPP_DG_INF, "IPX_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppBRIDGING_NCP:
			outbuf(PPP_DG_INF, "BRIDGING_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppSTREAM_NCP:
			outbuf(PPP_DG_INF, "STREAM_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppBANYAN_NCP:
			outbuf(PPP_DG_INF, "BANYAN_NCP ", dp);
			trunchexdump(PPP_DG_NDA, bufp, cnt, HEXDLIM, dp);
			break;
		case pppLCP:
			outbuf(PPP_DG_INF, "LCP ", dp);
			if (process_lcp(&bufp, &cnt, dp) < 0) {
				hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			}
			break;
		case pppAuthPAP:
			outbuf(PPP_DG_INF, "AuthPAP ", dp);
			if (process_pap(&bufp, &cnt, dp) < 0) {
				hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			}
			break;
		case pppLQM_REPORT:
			outbuf(PPP_DG_INF, "LQM_REPORT ", dp);
			if (process_lqm(&bufp, &cnt, dp) < 0) {
				hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			}
			break;
		case pppCHAP:
			outbuf(PPP_DG_INF, "CHAP ", dp);
			if (process_chap(&bufp, &cnt, dp) < 0) {
				hexdump(PPP_DG_ERRREP, bufp, cnt, dp);
			}
			break;

		default:
			(void) sprintf(valstr, "{Unrecognized protocol: %4x ",
								proto);
			outbuf(PPP_DG_ERR, valstr, dp);
			trunchexdump(PPP_DG_ERRREP, bufp, cnt, HEXDLIM, dp);
			outbuf(PPP_DG_ERR, "} ", dp);
			break;
	}
	outputline(dp);
	if (dp->diag_info.media_type == pppAsync) {
		if (dp->accmvalid && dp->didlcpreq) {
			if (direc == UPSTR) {
				dp->downmask = dp->accmask;
			} else if (direc == DOWNSTR) {
				dp->upmask = dp->accmask;
			}
		}
	}
	freemsg(mp);
}

static int
process_ipcp(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned code, id, len;
	unsigned short tests;
	char idlin[30];

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{Bad ipcp size} ", dp);
		return (-1);
	}
	bcopy(&(*bufp)[2], &tests, sizeof (tests));
	len = ntohs(tests);
	if (len != *cnt) {
		(void) sprintf(idlin, "{Discr. LEN=%d, CNT=%d} ", len, *cnt);
		outbuf(PPP_DG_ERR, idlin, dp);
	}
	code = (unsigned int)(*bufp)[0];
	switch (code) {
		case ConfigureReq:
			outbuf(PPP_DG_INF, "Config-Req ", dp);
			break;
		case ConfigureAck:
			outbuf(PPP_DG_INF, "Config-ACK ", dp);
			break;
		case ConfigureNak:
			outbuf(PPP_DG_INF, "Config-NACK ", dp);
			break;
		case ConfigureRej:
			outbuf(PPP_DG_INF, "Config-REJ ", dp);
			break;
		case TerminateReq:
			outbuf(PPP_DG_INF, "Term-REQ ", dp);
			break;
		case TerminateAck:
			outbuf(PPP_DG_INF, "Term-ACK ", dp);
			break;
		case CodeRej:
			outbuf(PPP_DG_INF, "Code-REJ ", dp);
			break;
		default:
			outbuf(PPP_DG_ERR, "{Unrec ipcp code} ", dp);
			return (-1);
	}
	id = (unsigned)(*bufp)[1] & 0xff;
	(void) sprintf(idlin, " ID=%02x ", id);
	outbuf(PPP_DG_INF, idlin, dp);

	(void) sprintf(idlin, "LEN=%d ", len);
	outbuf(PPP_DG_INF, idlin, dp);
	*bufp += 4;
	*cnt -= 4;
	switch (code) {
		case ConfigureReq:
		case ConfigureAck:
		case ConfigureNak:
		case ConfigureRej:
			if (process_ipcp_opts(bufp, cnt, dp) < 0)
				return (-1);
			break;
		case TerminateReq:
		case TerminateAck:
			if (*cnt > 0) {
				outbuf(PPP_DG_OPT, "Data: ", dp);
				hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
			}
			break;
		case CodeRej:
			outbuf(PPP_DG_OPT, "Rej. pack: ", dp);
			hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
			break;
		default:
			break;
	}
	return (0);

}

static int
process_ipcp_opts(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned type, len;
	char *ptr;
	ushort_t tests;
	short vals;
	unsigned slot;
	char idlin[30];
	int i;

	if (*cnt == 0)
		return (0);
	while (*cnt > 0) {
		if (*cnt < 2) {
			outbuf(PPP_DG_ERR, "{Bad ipcp opts size} ", dp);
			return (-1);
		}
		type = (unsigned)(*bufp)[0] & 0xff;
		len = (unsigned)(*bufp)[1] & 0xff;
		if (*cnt < len) {
			outbuf(PPP_DG_ERR, "{Short option} ", dp);
			return (-1);
		}
		switch (type) {
		case IPAddr:
			if (len != 10) {
				outbuf(PPP_DG_ERR, "{short ip addresses} ",
				    dp);
				return (-1);
			}
			outbuf(PPP_DG_OPT, "SOU_ADDR=", dp);
			(void) sprintf(idlin, "%u.%u.%u.%u ",
			    (uint_t)(*bufp)[2],
			    (uint_t)(*bufp)[3],
			    (uint_t)(*bufp)[4],
			    (uint_t)(*bufp)[5]);

			outbuf(PPP_DG_OPT, idlin, dp);
			outbuf(PPP_DG_OPT, "DEST_ADDR=", dp);
			(void) sprintf(idlin, "%u.%u.%u.%u ",
			    (uint_t)(*bufp)[6],
			    (uint_t)(*bufp)[7],
			    (uint_t)(*bufp)[8],
			    (uint_t)(*bufp)[9]);
			outbuf(PPP_DG_OPT, idlin, dp);
			(*bufp) += len;
			(*cnt) -= len;
			break;
		case IPCompType:
			if (len < 4) {
				outbuf(PPP_DG_ERR, "{short ip comp field} ",
				    dp);
				return (-1);
			}
			ptr = (char *)&tests;
			for (i = 0; i < sizeof (ushort_t); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			vals = ntohs(tests);
			switch (vals) {
			case VJCOMP:
				if (len != 6) {
					outbuf(PPP_DG_ERR, "{bad vjcomp len} ",
					    dp);
					return (-1);
				}
				outbuf(PPP_DG_OPT, "VJCOMP ", dp);
				slot = (unsigned)(*bufp)[4] & 0xff;
				(void) sprintf(idlin, "MAXSID=%u ", slot);
				outbuf(PPP_DG_OPT, idlin, dp);
				slot = (unsigned)(*bufp)[5] & 0xff;
				switch (slot) {
				case 0:
					outbuf(PPP_DG_OPT,
					    "No-sid-comp ", dp);
					break;
				case 1:
					outbuf(PPP_DG_OPT,
					    "Sid-comp-OK ", dp);
					break;
				default:
					outbuf(PPP_DG_ERR,
					    "{Bad comp-slot-id} ", dp);
					break;
				}
				break;
			case VJCOMP_OLD:
				if (len != 4) {
					outbuf(PPP_DG_ERR,
					    "{bad vjcomp_old len} ", dp);
					return (-1);
				}
				outbuf(PPP_DG_OPT, "OLD_VJCOMP ", dp);
				break;
			default:
				(void) sprintf(idlin,
					"{Unrec comp proto %x} ", vals);
				outbuf(PPP_DG_ERR, idlin, dp);
					break;
			}
			(*bufp) += len;
			(*cnt) -= len;
			break;
		case IPAddrNew:
			if (len != 6) {
				outbuf(PPP_DG_ERR, "{bad ip addr} ", dp);
				return (-1);
			}
			(void) sprintf(idlin, "IPADDR=");
			outbuf(PPP_DG_OPT, idlin, dp);
			(void) sprintf(idlin, "%u.%u.%u.%u ",
			    (uint_t)(*bufp)[2],
			    (uint_t)(*bufp)[3],
			    (uint_t)(*bufp)[4],
			    (uint_t)(*bufp)[5]);

			outbuf(PPP_DG_OPT, idlin, dp);

			(*bufp) += len;
			(*cnt) -= len;
			break;
		default:
			outbuf(PPP_DG_ERR, "{Unrec ICPC opt} ", dp);
			return (-1);
		}
	}
	return (0);
}

static int
process_pap(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned code, id, len;
	unsigned short tests;
	int i;
	char idlin[300];
	int plen;

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{Bad pap size} ", dp);
		return (-1);
	}
	bcopy(&(*bufp)[2], &tests, sizeof (tests));
	len = ntohs(tests);
	if (len != *cnt) {
		(void) sprintf(idlin, "{Discr. LEN=%d, CNT=%d} ", len, *cnt);
		outbuf(PPP_DG_ERR, idlin, dp);
	}
	code = (unsigned int)(*bufp)[0];
	switch (code) {
	case Authenticate:
		outbuf(PPP_DG_INF, "Authenticate ", dp);
		break;
	case AuthenticateAck:
		outbuf(PPP_DG_INF, "Auth ACK ", dp);
		break;
	case AuthenticateNak:
		outbuf(PPP_DG_INF, "Auth NACK ", dp);
		break;
	}
	id = (unsigned)(*bufp)[1] & 0xff;
	(void) sprintf(idlin, " ID=%02x ", id);
	outbuf(PPP_DG_INF, idlin, dp);

	(void) sprintf(idlin, "LEN=%d ", len);
	outbuf(PPP_DG_INF, idlin, dp);
	*bufp += 4;
	*cnt -= 4;
	switch (code) {
	case Authenticate:
		if (*cnt < 1) {
			outbuf(PPP_DG_ERR, "{Small auth packet} ", dp);
			return (-1);
		}
		plen = (int)(*bufp)[0];
		(void) sprintf(idlin, "Peer-ID-Length= %d ", plen);
		outbuf(PPP_DG_OPT, idlin, dp);
		(*bufp)++;
		(*cnt)--;
		if (plen > *cnt-1) {
			outbuf(PPP_DG_ERR, "{Bad peer-id size} ", dp);
			return (-1);
		}
		outbuf(PPP_DG_OPT, "Peer-ID: ", dp);
		hexdump(PPP_DG_OPT, *bufp, plen, dp);
		*bufp += plen;
		*cnt -= plen;

		plen = (int)(*bufp)[0];
		(void) sprintf(idlin, "Passwd-Length= %d ", plen);
		outbuf(PPP_DG_OPT, idlin, dp);
		(*bufp)++;
		(*cnt)--;
		if (plen > *cnt) {
			outbuf(PPP_DG_ERR, "{Bad passwd size} ", dp);
			return (-1);
		}
		outbuf(PPP_DG_OPT, "Passwd: ", dp);
		hexdump(PPP_DG_OPT, *bufp, plen, dp);
		break;
	case AuthenticateAck:
	case AuthenticateNak:
		if (*cnt < 1) {
			outbuf(PPP_DG_ERR, "{Small auth packet} ", dp);
			return (-1);
		}
		plen = (int)(*bufp)[0];
		(void) sprintf(idlin, "Msg-Length= %d ", plen);
		outbuf(PPP_DG_OPT, idlin, dp);
		(*bufp)++;
		(*cnt)--;
		if (plen > *cnt) {
			outbuf(PPP_DG_ERR, "{Bad msg length} ", dp);
			return (-1);
		}
#ifdef DOING_ASCII_NAME
		for (i = 0; i < *cnt; i++) {
			idlin[i] = (*bufp)[i];
		}
		idlin[i] = '\0';
		outbuf(PPP_DG_OPT, idlin, dp);
		outbuf(PPP_DG_OPT, " ", dp);
#else
		hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
#endif
		break;
	}
	return (0);
}

static int
process_lqm(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	LQM_pack_t	lqr, templqr;
	char		idlin[100];

	if (*cnt != sizeof (LQM_pack_t)) {
		outbuf(PPP_DG_ERR, "{Bad lqm report size} ", dp);
		return (-1);
	}
	bcopy((*bufp), &templqr, sizeof (lqr));

	lqr = ntoh_LQM_pack_t(templqr);

	(void) sprintf(idlin, "Magic-number=	%08x  ", lqr.magic_num);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "LastOutLQRs=	%u  ", lqr.lastOutLQRs);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "LastOutPackets= %u  ", lqr.lastOutPackets);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "LastOutOctets	%u  ", lqr.lastOutOctets);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerInLQRs=	%u  ", lqr.peerInLQRs);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerInPackets=	%u  ", lqr.peerInPackets);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerInDiscards= %u  ", lqr.peerInDiscards);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerInErrors=	%u  ", lqr.peerInErrors);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerInOctets=	%u  ", lqr.peerInOctets);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerOutLQRs=	%u  ", lqr.peerOutLQRs);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerOutPackets= %u  ", lqr.peerOutPackets);
	outbuf(PPP_DG_OPT, idlin, dp);
	(void) sprintf(idlin, "PeerOutOctets=	%u ", lqr.peerOutOctets);
	outbuf(PPP_DG_OPT, idlin, dp);

	*cnt -= sizeof (LQM_pack_t);
	*bufp += sizeof (LQM_pack_t);

	return (0);
}


static int
process_chap(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned code, id, len;
	unsigned short tests;
	int i;
	char idlin[300];
	int vsize;

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{Bad chap size} ", dp);
		return (-1);
	}
	bcopy(&(*bufp)[2], &tests, sizeof (tests));
	len = ntohs(tests);
	if (len != *cnt) {
		(void) sprintf(idlin, "{Discr. LEN=%d, CNT=%d} ", len, *cnt);
		outbuf(PPP_DG_ERR, idlin, dp);
	}
	code = (unsigned int)(*bufp)[0];
	switch (code) {
	case Challenge:
		outbuf(PPP_DG_INF, "Challenge ", dp);
		break;
	case Response:
		outbuf(PPP_DG_INF, "Response ", dp);
		break;
	case Success:
		outbuf(PPP_DG_INF, "Success ", dp);
		break;
	case Failure:
		outbuf(PPP_DG_INF, "Failure ", dp);
		break;
	}
	id = (unsigned)(*bufp)[1] & 0xff;
	(void) sprintf(idlin, " ID=%02x ", id);
	outbuf(PPP_DG_INF, idlin, dp);

	(void) sprintf(idlin, "LEN=%d ", len);
	outbuf(PPP_DG_INF, idlin, dp);
	*bufp += 4;
	*cnt -= 4;
	switch (code) {
		case Challenge:
		case Response:
			if (*cnt < 1) {
				outbuf(PPP_DG_ERR, "{Small auth packet} ", dp);
				return (-1);
			}
			vsize = (int)(*bufp)[0];
			(void) sprintf(idlin, "Value-size= %d ", vsize);
			outbuf(PPP_DG_OPT, idlin, dp);
			(*bufp)++;
			(*cnt)--;
			if (vsize > *cnt) {
				outbuf(PPP_DG_ERR, "{Bad value size} ", dp);
				return (-1);
			}
			outbuf(PPP_DG_OPT, "Value: ", dp);
			hexdump(PPP_DG_OPT, *bufp, vsize, dp);
			*bufp += vsize;
			*cnt -= vsize;
			outbuf(PPP_DG_OPT, "Name: ", dp);

#ifdef DOING_ASCII_NAME
			for (i = 0; i < *cnt; i++) {
				idlin[i] = (*bufp)[i];
			}
			idlin[i] = '\0';
			outbuf(PPP_DG_OPT, idlin, dp);
#else
				hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
#endif
			break;
		case Success:
		case Failure:
			outbuf(PPP_DG_OPT, "Message: ", dp);
			for (i = 0; i < *cnt; i++) {
				idlin[i] = (*bufp)[i];
			}
			idlin[i] = '\0';
				outbuf(PPP_DG_OPT, idlin, dp);
			break;
		default:
			outbuf(PPP_DG_ERR, "{Unrecognized chap code} ", dp);
			break;
	}
	return (0);
}


static int
process_lcp(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned code, id, len;
	unsigned short tests;
	char idlin[30];

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{Bad lcp size} ", dp);
		return (-1);
	}
	bcopy(&(*bufp)[2], &tests, sizeof (tests));
	len = ntohs(tests);
	if (len != *cnt) {
		(void) sprintf(idlin, "{Discr. LEN=%d, CNT=%d} ", len, *cnt);
		outbuf(PPP_DG_ERR, idlin, dp);
	}
	code = (unsigned int)(*bufp)[0];
	switch (code) {
		case ConfigureReq:
			outbuf(PPP_DG_INF, "Config-Req ", dp);
			if (dp->diag_info.media_type == pppAsync) {
				dp->didlcpreq = 1;
			}
			break;
		case ConfigureAck:
			outbuf(PPP_DG_INF, "Config-ACK ", dp);
			break;
		case ConfigureNak:
			outbuf(PPP_DG_INF, "Config-NACK ", dp);
			break;
		case ConfigureRej:
			outbuf(PPP_DG_INF, "Config-REJ ", dp);
			break;
		case TerminateReq:
			outbuf(PPP_DG_INF, "Term-REQ ", dp);
			break;
		case TerminateAck:
			outbuf(PPP_DG_INF, "Term-ACK ", dp);
			break;
		case CodeRej:
			outbuf(PPP_DG_INF, "Code-REJ ", dp);
			break;
		case ProtoRej:
		    outbuf(PPP_DG_INF, "Proto-REJ ", dp);
			break;
		case EchoReq:
			outbuf(PPP_DG_INF, "Echo-REQ ", dp);
			break;
		case EchoRep:
			outbuf(PPP_DG_INF, "Echo-REPLY ", dp);
			break;
		case DiscardReq:
			outbuf(PPP_DG_INF, "Disc-REQ ", dp);
			break;
	}
	id = (unsigned)(*bufp)[1] & 0xff;
	(void) sprintf(idlin, " ID=%02x ", id);
	outbuf(PPP_DG_INF, idlin, dp);

	(void) sprintf(idlin, "LEN=%d ", len);
	outbuf(PPP_DG_INF, idlin, dp);
	*bufp += 4;
	*cnt -= 4;
	switch (code) {
		case ConfigureReq:
		case ConfigureAck:
		case ConfigureNak:
		case ConfigureRej:
			if (process_lcp_opts(bufp, cnt, dp) < 0)
				return (-1);
			break;
		case TerminateReq:
		case TerminateAck:
			if (*cnt > 0) {
				outbuf(PPP_DG_OPT, "Data: ", dp);
				hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
			}
			break;
		case CodeRej:
			outbuf(PPP_DG_OPT, "Rej. pack: ", dp);
			hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
			break;
		case ProtoRej:
			if (process_proto_rej(bufp, cnt, dp) < 0)
				return (-1);
			break;
		case EchoReq:
		case EchoRep:
			if (process_echo(bufp, cnt, dp) < 0)
				return (-1);
			break;
		case DiscardReq:
			if (process_discard(bufp, cnt, dp) < 0) {
				return (-1);
			}
			break;
		default:
			break;
	}
	return (0);

}

static int
process_discard(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	char *ptr;
	uint_t testl;
	int i;
	char idlin[30];

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{short magic number field} ", dp);
		return (-1);
	}
	for (ptr = (char *)&testl, i = 0; i < sizeof (testl); i++)
		ptr[i] = (*bufp)[i];
	(void) sprintf(idlin, "MAG#=%08x ", ntohl(testl));
	outbuf(PPP_DG_OPT, idlin, dp);
	(*bufp) += 4;
	(*cnt) -= 4;
	if (*cnt > 0) {
		outbuf(PPP_DG_OPT, "Data: ", dp);
		hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
	}
	return (0);
}

static int
process_echo(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	char *ptr;
	uint_t testl;
	int i;
	char idlin[30];

	if (*cnt < 4) {
		outbuf(PPP_DG_ERR, "{short magic number field} ", dp);
		return (-1);
	}
	for (ptr = (char *)&testl, i = 0; i < sizeof (testl); i++)
		ptr[i] = (*bufp)[i];
	(void) sprintf(idlin, "MAG#=%08x ", ntohl(testl));
	outbuf(PPP_DG_OPT, idlin, dp);
	(*bufp) += 4;
	(*cnt) -= 4;
	outbuf(PPP_DG_OPT, "Req_info: ", dp);
	hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
	(*bufp) += *cnt;
	*cnt = 0;
	return (0);
}

static int
process_proto_rej(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	char *ptr;
	ushort_t tests;
	int i;
	char idlin[30];

	if (*cnt < 2) {
		outbuf(PPP_DG_ERR, "{short protocol field} ", dp);
		return (-1);
	}
	ptr = (char *)&tests;
	for (i = 0; i < sizeof (ushort_t); i++) {
		ptr[i] = (*bufp)[2+i];
	}
	(void) sprintf(idlin, "Rej_proto=%2x ", ntohs(tests));
	outbuf(PPP_DG_OPT, idlin, dp);
	(*bufp) += 2;
	(*cnt) -= 2;
	outbuf(PPP_DG_OPT, "Rej_info: ", dp);
	hexdump(PPP_DG_OPT, *bufp, *cnt, dp);
	return (0);
}


static int
process_lcp_opts(uchar_t **bufp, int *cnt, ppp_diag_struct_t *dp)
{
	unsigned type, len;
	char *ptr;
	int i;
	char idlin[30];
	uint_t testl, vall;
	ushort_t tests, vals;

	while (*cnt > 0) {
		if (*cnt < 2) {
			outbuf(PPP_DG_ERR, "{Short option} ", dp);
			return (-1);
		}
		type = (unsigned)(*bufp)[0];
		len = (unsigned)(*bufp)[1] & 0xff;
		if (*cnt < len) {
			outbuf(PPP_DG_ERR, "{Short option} ", dp);
			return (-1);
		}
		switch (type) {
		case MaxReceiveUnit:
			if (len != 4) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
				return (-1);
			}
			ptr = (char *)&tests;
			for (i = 0; i < sizeof (ushort_t); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			(void) sprintf(idlin, "MRU=%d ", ntohs(tests));
			outbuf(PPP_DG_OPT, idlin, dp);
			*bufp += len;
			*cnt -= len;
			break;
		case AsyncControlMap:
			if (len != 6) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
				return (-1);
			}
			ptr = (char *)&testl;
			for (i = 0; i < sizeof (testl); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			(void) sprintf(idlin, "ACCM=%08x ", ntohl(testl));
			outbuf(PPP_DG_OPT, idlin, dp);
			*bufp += len;
			*cnt -= len;
			if (dp -> diag_info.media_type == pppAsync) {
				dp -> accmask = ntohl(testl);
				dp -> accmvalid = 1;
			}
			break;
		case AuthenticationType:
			if (len < 4) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
				return (-1);
			}
			ptr = (char *)&tests;
			for (i = 0; i < sizeof (ushort_t); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			vals = ntohs(tests);
			switch (vals) {
			case pppAuthPAP:
				if (len != 4) {
					outbuf(PPP_DG_ERR,
					    "{Bad PAP opt len}", dp);
					return (-1);
				}
				(void) sprintf(idlin, "Auth=PAP ");
				outbuf(PPP_DG_OPT, idlin, dp);
				break;
			case pppCHAP:
				if (len != 5) {
					outbuf(PPP_DG_ERR,
					    "{Bad CHAP opt len}", dp);
					return (-1);
				}
				outbuf(PPP_DG_OPT, "Auth=CHAP ", dp);
				switch ((uint_t)(*bufp)[4]) {
				case MD5:
					outbuf(PPP_DG_OPT, "Alg=MD5 ", dp);
					break;
				default:
					(void) sprintf(idlin, "alg=%x ",
						(uint_t)(*bufp)[4]);
					outbuf(PPP_DG_OPT, idlin, dp);
					break;
				}
				break;
			default:
				(void) sprintf(idlin, "Unknown Auth=%x ",
					vals);
				outbuf(PPP_DG_ERR, idlin, dp);
			}
			*bufp += len;
			*cnt -= len;
			break;
		case MagicNumber:
			if (len != 6) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
					return (-1);
			}
			ptr = (char *)&testl;
			for (ptr = (char *)&testl, i = 0;
			    i < sizeof (testl); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			(void) sprintf(idlin, "MAG#=%08x ", ntohl(testl));
			outbuf(PPP_DG_OPT, idlin, dp);
			*bufp += len;
			*cnt -= len;
			break;
		case LinkQualityMon:
			if (len < 4) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
				return (-1);
			}
			ptr = (char *)&tests;
			for (i = 0; i < sizeof (ushort_t); i++) {
				ptr[i] = (*bufp)[2+i];
			}
			vals = ntohs(tests);
			switch (vals) {
			case pppLQM_REPORT:
				if (len != 8) {
					outbuf(PPP_DG_ERR,
					    "{Bad LQ type len} ", dp);
					return (-1);
				}
				bcopy(&(*bufp)[4+i], &testl, sizeof (testl));
				vall = ntohl(testl);
				(void) sprintf(idlin,
				    "Qual=LQR Rep-period=%u ", vall);
				outbuf(PPP_DG_OPT, idlin, dp);

				break;
			default:
				(void) sprintf(idlin, "Unknown LQM=%x ", vals);
				outbuf(PPP_DG_ERR, idlin, dp);
			}
			*bufp += len;
			*cnt -= len;
			break;
		case ProtoFieldCompress:
			if (len != 2) {
				outbuf(PPP_DG_ERR, "{Bad opt len} ", dp);
				return (-1);
			}
			outbuf(PPP_DG_OPT, "ProtFCOMP ", dp);
			*bufp += len;
			*cnt -= len;
			break;
		case AddrCtrlCompress:
			if (len != 2) {
				outbuf(PPP_DG_OPT, "{Bad opt len} ", dp);
				return (-1);
			}
			outbuf(PPP_DG_OPT, "AddrCCOMP ", dp);
			*bufp += len;
			*cnt -= len;
			break;
		default:
			(void) sprintf(idlin, "{Unknown OPTION=%x l=%x} ",
						type, len);
			outbuf(PPP_DG_ERR, idlin, dp);
			return (-1);
		}
	}
	return (0);
}

static int
decompress_frame(mblk_t *mp, int *addrcomp, int *protocomp, ushort_t *proto,
		ushort_t *fcs, ppp_diag_struct_t *dp)
{
	uchar_t	*cp, *test;

	uchar_t	*rdp;
	ushort_t tests;
	if (msgdsize(mp) < 4) {
		outbuf(PPP_DG_ERR, "{Invalid ppp packet} ", dp);
		return (-1);
	}
	if (dp->diag_info.media_type == pppAsync) {
		*fcs = PPP_INITFCS;
		for (rdp = mp-> b_rptr; rdp < mp-> b_wptr; rdp++) {
			*fcs = PPP_FCS(*fcs, *rdp);
		}
	}
	cp = mp-> b_rptr;
	if ((*cp == PPP_FRAME_ADDR) && (*(cp+1) == PPP_FRAME_CTRL)) {
			cp++;
			cp++;
			(void) adjmsg(mp, 2);
			*addrcomp = 0;
	} else {
		*addrcomp = 1;
	}
	test = (uchar_t *)&tests;
	if (*cp & 1) {
		*test++ = 0;
		*test++ = *cp++;
		(void) adjmsg(mp, 1);
		*protocomp = 1;
	} else {
		*test++ = *cp++;
		*test++ = *cp++;
		(void) adjmsg(mp, 2);
		*protocomp = 0;
	}
	*proto = ntohs(tests);

	if (dp->diag_info.media_type == pppAsync)
		(void) adjmsg(mp, -sizeof (ushort_t));
	return (1);
}

/*
 * Routines specific to ASYNC diag
 *
 */

static int
filter_esc(int direc, mblk_t *mp, ppp_diag_struct_t *dp)
{
	unsigned int mask;
	int looking = 0;
	char valstr[20];
	uchar_t *sptr, *cptr, val;
	uint_t unescaped = 0, tst;
	int i;

	mask = (direc == UPSTR ? dp->upmask : dp->downmask);
	for (sptr = cptr = mp-> b_rptr; cptr != mp-> b_wptr; cptr++) {
		val = *cptr;
		if (val < 0x20 && ((1 << val) & mask)) {
			unescaped |= (1 << val);
		}
		if (!looking) {
			if (val == 0x7d) {
				looking = 1;
			} else {
				*sptr++ = val;
			}
		} else {
			*sptr++ = val & 0x20 ? val & ~0x20 : val | 0x20;
			looking = 0;
		}
	}
	if (unescaped != 0) {
		outbuf(PPP_DG_ERR, "{Unescaped characters", dp);
		outbuf(PPP_DG_ERRREP, ": ", dp);
		for (tst = 1, i = 0; i < 32; i++) {
			if (unescaped & tst) {
				(void) sprintf(valstr, "%02x ", i);
				outbuf(PPP_DG_ERRREP, valstr, dp);
			}
			tst = tst << 1;
		}
		outbuf(PPP_DG_ERR, "} ", dp);
	}
	mp-> b_wptr = sptr;
	if (looking) {
		outbuf(PPP_DG_ERR, "{ Packet ends with escape }", dp);
		return (-1);
	}
	return (0);
}

LQM_pack_t
ntoh_LQM_pack_t(LQM_pack_t lqr)
{
	LQM_pack_t nlqr;

	nlqr.magic_num = ntohl(lqr.magic_num);
	nlqr.lastOutLQRs = ntohl(lqr.lastOutLQRs);
	nlqr.lastOutPackets = ntohl(lqr.lastOutPackets);
	nlqr.lastOutOctets = ntohl(lqr.lastOutOctets);
	nlqr.peerInLQRs = ntohl(lqr.peerInLQRs);
	nlqr.peerInPackets = ntohl(lqr.peerInPackets);
	nlqr.peerInDiscards = ntohl(lqr.peerInDiscards);
	nlqr.peerInErrors = ntohl(lqr.peerInErrors);
	nlqr.peerInOctets = ntohl(lqr.peerInOctets);
	nlqr.peerOutLQRs = ntohl(lqr.peerOutLQRs);
	nlqr.peerOutPackets = ntohl(lqr.peerOutPackets);
	nlqr.peerOutOctets = ntohl(lqr.peerOutOctets);
	return (nlqr);
}

static void
free_diag_struct(ppp_diag_struct_t *dp)
{
	if (dp->dcp)
		freemsg(dp->dcp);

	if (dp->ucp)
		freemsg(dp->ucp);
/*
 * Don't free dnp, or unp because they are either equal to dcp, ucp or
 * blocks that have been linked to them.
 */

	kmem_free(dp, sizeof (ppp_diag_struct_t));
}
