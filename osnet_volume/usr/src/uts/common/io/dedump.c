/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dedump.c	1.19	99/03/21 SMI"	/* SVr3.2H 	*/

/*
 * Dump streams module.  Could be used anywhere on the stream to
 * print all message headers and data on to the console.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>

#include <sys/conf.h>
#include <sys/modctl.h>

static void
dedump_hex(mblk_t *mp)
{
	int togo = 30;
	u_char *cp = mp->b_rptr;

	if (mp->b_rptr + togo >= mp->b_wptr)
		togo = mp->b_wptr - mp->b_rptr;

	while (togo-- > 0)
		(void) printf("%02x ", *cp++);

	(void) printf("\n");
}

static void
dedump_char(mblk_t *mp)
{
	(void) printf("0x%x\n", *(char *)mp->b_rptr);
}

static void
dedump_int(mblk_t *mp)
{
	(void) printf("0x%x\n", *(int *)mp->b_rptr);
}

static void
dedump_ssize(mblk_t *mp)
{
	(void) printf("0x%lx\n", *(ssize_t *)mp->b_rptr);
}

static void
dedump_text(mblk_t *mp)
{
	int togo = 30;
	u_char *cp = mp->b_rptr;

	if (mp->b_rptr + togo >= mp->b_wptr)
		togo = mp->b_wptr - mp->b_rptr;

	while (togo-- > 0) {
		u_char c = *cp++;
		if (c >= 32 && c <= 126)
			(void) printf("%c ", (char)c);
		else
			(void) printf("%02x ", c);
	}

	(void) printf("\n");
}

static void
dedump_iocblk(mblk_t *mp)
{
	struct iocblk *ic = (struct iocblk *)mp->b_rptr;

	(void) printf("cmd %x cred %p id %u flag %x count %lu rval %d err %d\n",
	    ic->ioc_cmd, (void *)ic->ioc_cr, ic->ioc_id, ic->ioc_flag,
	    ic->ioc_count, ic->ioc_rval, ic->ioc_error);
}

static void
dedump_stroptions(mblk_t *mp)
{
	struct stroptions *so = (struct stroptions *)mp->b_rptr;

	(void) printf("flag %x readopt %d wroff %u\n",
	    so->so_flags, so->so_readopt, so->so_wroff);

	(void) printf("minpsz %ld maxpsz %ld hiwat %lu lowat %lu\n",
	    so->so_minpsz, so->so_maxpsz, so->so_hiwat, so->so_lowat);

	(void) printf("band %u erropt %u maxblk %ld copyopt %u\n",
	    so->so_band, so->so_erropt, so->so_maxblk, so->so_copyopt);
}

static void
dedump_copyreq(mblk_t *mp)
{
	struct copyreq *cq = (struct copyreq *)mp->b_rptr;

	(void) printf("cmd %d cred %p id %u flag %x priv %p addr %p size %lu\n",
	    cq->cq_cmd, (void *)cq->cq_cr, cq->cq_id, cq->cq_flag,
	    (void *)cq->cq_private, (void *)cq->cq_addr, cq->cq_size);
}

static void
dedump_copyresp(mblk_t *mp)
{
	struct copyresp *cp = (struct copyresp *)mp->b_rptr;

	(void) printf("cmd %d cred %p id %u flag %x priv %p rval %p\n",
	    cp->cp_cmd, (void *)cp->cp_cr, cp->cp_id, cp->cp_flag,
	    (void *)cp->cp_private, (void *)cp->cp_rval);
}

typedef struct msgfmt {
	u_char	m_type;
	char	m_desc[15];
	void	(*m_print)(mblk_t *);
} msgfmt_t;

static msgfmt_t msgfmt[256] = {
	{	M_DATA,		"M_DATA", 	dedump_text		},
	{	M_PROTO,	"M_PROTO", 	dedump_hex		},
	{	M_BREAK,	"M_BREAK", 	dedump_hex		},
	{	M_PASSFP,	"M_PASSFP", 	dedump_hex		},
	{	M_EVENT,	"M_EVENT", 	dedump_hex		},
	{	M_SIG,		"M_SIG", 	dedump_char		},
	{	M_DELAY,	"M_DELAY", 	dedump_int		},
	{	M_CTL,		"M_CTL", 	dedump_hex		},
	{	M_IOCTL,	"M_IOCTL", 	dedump_iocblk		},
	{	M_SETOPTS,	"M_SETOPTS", 	dedump_stroptions	},
	{	M_RSE,		"M_RSE", 	dedump_hex		},
	{	M_IOCACK,	"M_IOCACK", 	dedump_iocblk		},
	{	M_IOCNAK,	"M_IOCNAK", 	dedump_iocblk		},
	{	M_PCPROTO,	"M_PCPROTO", 	dedump_hex		},
	{	M_PCSIG,	"M_PCSIG", 	dedump_char		},
	{	M_READ,		"M_READ", 	dedump_ssize		},
	{	M_FLUSH,	"M_FLUSH", 	dedump_char		},
	{	M_STOP,		"M_STOP", 	dedump_hex		},
	{	M_START,	"M_START", 	dedump_hex		},
	{	M_HANGUP,	"M_HANGUP", 	dedump_hex		},
	{	M_ERROR,	"M_ERROR", 	dedump_char		},
	{	M_COPYIN,	"M_COPYIN", 	dedump_copyreq		},
	{	M_COPYOUT,	"M_COPYOUT", 	dedump_copyreq		},
	{	M_IOCDATA,	"M_IOCDATA", 	dedump_copyresp		},
	{	M_PCRSE,	"M_PCRSE", 	dedump_hex		},
	{	M_STOPI,	"M_STOPI", 	dedump_hex		},
	{	M_STARTI,	"M_STARTI", 	dedump_hex		},
	{	M_PCEVENT,	"M_PCEVENT", 	dedump_hex		},
	{	M_UNHANGUP,	"M_UNHANGUP", 	dedump_hex		},
};

/*ARGSUSED1*/
static int
dedumpopen(queue_t *q, dev_t *devp, int oflag, int sflag, cred_t *crp)
{
	if (!sflag)
		return (ENXIO);

	if (q->q_ptr)
		return (0);		/* already attached */

	qprocson(q);
	return (0);
}

/*ARGSUSED1*/
static int
dedumpclose(queue_t *q, int flag, cred_t *crp)
{
	qprocsoff(q);
	return (0);
}

/*
 * Common put procedure for upstream and downstream.
 */
static int
dedumpput(queue_t *q, mblk_t *mp)
{
	unsigned char type = mp->b_datap->db_type;

	(void) printf("%s %p %10s ", (q->q_flag & QREADR) ? "RD" : "WR",
	    (void *)q, msgfmt[type].m_desc);

	msgfmt[type].m_print(mp);

	putnext(q, mp);
	return (0);
}

struct module_info dedump_minfo = {
	0xaaa, "dedump", 0, INFPSZ, (size_t)INFPSZ, (size_t)INFPSZ
};

struct qinit dedumprinit = {
	dedumpput, NULL, dedumpopen, dedumpclose, NULL, &dedump_minfo, NULL
};

struct qinit dedumpwinit = {
	dedumpput, NULL, NULL, NULL, NULL, &dedump_minfo, NULL
};

struct streamtab dedumpinfo = {
	&dedumprinit, &dedumpwinit, NULL, NULL,
};

static struct fmodsw fsw = {
	"dedump",
	&dedumpinfo,
	D_NEW | D_MP | D_MTPERMOD	/* just to serialize printfs */
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "dump streams module", &fsw
};

static struct modlinkage modlinkage = {
	MODREV_1, &modlstrmod, NULL
};

int
_init(void)
{
	u_char i;
	msgfmt_t mf;

	/*
	 * Sort msgfmt[] so that msgfmt[n] describes message type n.
	 */
	for (i = 255; i != 0; i--) {
		mf = msgfmt[i];
		msgfmt[i].m_type = i;
		(void) sprintf(msgfmt[i].m_desc, "M_BOGUS_0x%x", i);
		msgfmt[i].m_print = dedump_hex;
		if (mf.m_desc[0] != 0)
			msgfmt[mf.m_type] = mf;
	}

	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
