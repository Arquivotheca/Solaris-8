/*
 * Copyright (c) 1997-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)encrdes.c	1.4	99/12/06 SMI"

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <netinet/in.h>
#include <net/pfkeyv2.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <netinet/ip6.h>
#include <inet/ip.h>
#include <inet/nd.h>
#include <inet/ip_ire.h>
#include <inet/ipsec_info.h>
#include <inet/ipsecesp.h>
#include <inet/sadb.h>


static int encrdes_open(queue_t *, dev_t *, int, int, cred_t *);
static int encrdes_close(queue_t *);
static void encrdes_rput(queue_t *, mblk_t *);
static void encrdes_wput(queue_t *, mblk_t *);

static struct module_info info = {
	5139, "encrdes", 0, INFPSZ, 65536, 1024
};

static struct qinit rinit = {
	(pfi_t)encrdes_rput, NULL, encrdes_open, encrdes_close, NULL, &info,
	NULL
};

static struct qinit winit = {
	(pfi_t)encrdes_wput, NULL, NULL, NULL, NULL, &info, NULL
};

struct streamtab encrdesinfo = {
	&rinit, &winit, NULL, NULL
};

static struct fmodsw _mi_module_fmodsw = {
	"encrdes",
	&encrdesinfo,
	(D_NEW | D_MP | D_MTPERMOD | D_MTPUTSHARED)
};

static struct modlstrmod modlstrmod = {
	&mod_strmodops, "DES-CBC Encryption PI STREAMS module",
	&_mi_module_fmodsw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *) &modlstrmod,
	NULL
};


int
_init(void)
{
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


/* ARGSUSED */
static int
encrdes_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{

	if (q->q_ptr)
		return (0);  /* Re-open of an already open instance. */

	if (sflag != MODOPEN)
		return (EINVAL);

	q->q_ptr = (void *)1;  /* Just so I know I'm open. */

	qprocson(q);

	return (0);
}

/* ARGSUSED */
static int
encrdes_close(queue_t *q)
{
	/* Everything will attend to itself. */
	qprocsoff(q);
	return (0);
}


/* ARGSUSED */
static void
encrdes_rput(queue_t *q, mblk_t *mp)
{

	freemsg(mp);
}

/*
 * I really shouldn't get any data coming down this path.
 * Poison the stream with M_ERROR.
 */
static void
encrdes_wput(queue_t *q, mblk_t *mp)
{
	freemsg(mp);
	mp = allocb(2, BPRI_HI);
	if (mp == NULL) {
		(void) strlog(info.mi_idnum, 0, 0,
		    SL_ERROR | SL_WARN | SL_CONSOLE,
		    "keysock_wput: can't alloc M_ERROR\n");
		return;
	}
	mp->b_datap->db_type = M_ERROR;
	mp->b_wptr = mp->b_rptr + 1;
	*(mp->b_rptr) = EPERM;
	qreply(q, mp);
}
