/*
 * Copyright (c) 1993-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uppc.c	1.7	98/01/23 SMI"

#include <sys/psm.h>
#include <sys/pit.h>

/*
 * External References
 */
extern void uppc_setspl();
extern int uppc_intr_enter();
extern void uppc_intr_exit();
extern hrtime_t uppc_gethrtime();

/*
 * Local Function Prototypes
 */
static void uppc_softinit(void);
static void uppc_picinit();
static void uppc_clkinit(int);
static int uppc_addspl(int irqno, int ipl, int min_ipl, int max_ipl);
static int uppc_delspl(int irqno, int ipl, int min_ipl, int max_ipl);
static processorid_t uppc_get_next_processorid(processorid_t cpu_id);
static int uppc_get_clockirq(int ipl);
static int uppc_probe(void);

/*
 * Global Data
 */
struct standard_pic pics0;

/*
 * Local Static Data
 */
#ifdef UPPC_DEBUG
#define	DENT	0x0001

static	int	uppc_debug = 0;
#endif

static struct	psm_ops uppc_ops = {
	uppc_probe,				/* psm_probe		*/

	uppc_softinit,				/* psm_init		*/
	uppc_picinit,				/* psm_picinit		*/
	uppc_intr_enter,			/* psm_intr_enter	*/
	uppc_intr_exit,				/* psm_intr_exit	*/
	uppc_setspl,				/* psm_setspl		*/
	uppc_addspl,				/* psm_addspl		*/
	uppc_delspl,				/* psm_delspl		*/
	(int (*)(processorid_t))NULL,		/* psm_disable_intr	*/
	(void (*)(processorid_t))NULL,		/* psm_enable_intr	*/
	(int (*)(int))NULL,			/* psm_softlvl_to_irq	*/
	(void (*)(int))NULL,			/* psm_set_softintr	*/
	(void (*)(processorid_t))NULL,		/* psm_set_idlecpu	*/
	(void (*)(processorid_t))NULL,		/* psm_unset_idlecpu	*/

	uppc_clkinit,				/* psm_clkinit		*/
	uppc_get_clockirq,			/* psm_get_clockirq	*/
	(void (*)(void))NULL,			/* psm_hrtimeinit	*/
	uppc_gethrtime,				/* psm_gethrtime	*/

	uppc_get_next_processorid,		/* psm_get_next_processorid */
	(void (*)(processorid_t, caddr_t))NULL,	/* psm_cpu_start	*/
	(int (*)(void))NULL,			/* psm_post_cpu_start	*/
	(void (*)(void))NULL,			/* psm_shutdown		*/
	(int (*)(int, int))NULL,		/* psm_get_ipivect	*/
	(void (*)(processorid_t, int))NULL,	/* psm_send_ipi		*/

	(int (*)(dev_info_t *, int))NULL,	/* psm_translate_irq	*/

	(int (*)(todinfo_t *))NULL,		/* psm_tod_get		*/
	(int (*)(todinfo_t *))NULL,		/* psm_tod_set		*/

	(void (*)(int, char *))NULL,		/* psm_notify_error	*/
};

static struct	psm_info uppc_info = {
	PSM_INFO_VER01_1,	/* version				*/
	PSM_OWN_SYS_DEFAULT,	/* ownership				*/
	(struct psm_ops *)&uppc_ops, /* operation			*/
	"uppc",			/* machine name				*/
	"UniProcessor PC",	/* machine descriptions			*/
};

/*
 * Configuration Data
 */

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

static void *uppc_hdlp;

int
_init(void)
{
	return (psm_mod_init(&uppc_hdlp, &uppc_info));
}

int
_fini(void)
{
	return (psm_mod_fini(&uppc_hdlp, &uppc_info));
}

int
_info(struct modinfo *modinfop)
{
	return (psm_mod_info(&uppc_hdlp, &uppc_info, modinfop));
}

/*
 * Autoconfiguration Routines
 */

static int
uppc_probe(void)
{
	return (PSM_SUCCESS);
}

static void
uppc_softinit(void)
{
	register struct standard_pic *pp;
	register int	i;

	pp = &pics0;

	/* initialize the ipl mask					*/
	for (i = 0; i < (MAXIPL << 1); i += 2) {
		/* enable slave lines on master */
		pp->c_iplmask[i] = 0xff;
		pp->c_iplmask[i+1] = (0xff & ~(1 << MASTERLINE));
	}
}

/*ARGSUSED*/
static void
uppc_clkinit(int hertz)
{
	/* program timer 0 					*/
	u_long clkticks = PIT_HZ / hz;
	outb(PITCTL_PORT, (PIT_C0|PIT_NDIVMODE|PIT_READMODE));
	outb(PITCTR0_PORT, (u_char)clkticks);
	outb(PITCTR0_PORT, (u_char)(clkticks>>8));
}

static void
uppc_picinit()
{
	picsetup();
}

/*ARGSUSED3*/
static int
uppc_addspl(int irqno, int ipl, int min_ipl, int max_ipl)
{
	register struct standard_pic *pp;
	register int	i;
	register int	startidx;
	u_char	vectmask;

	if (ipl != min_ipl)
		return (0);

	if (irqno > 7) {
		vectmask = 1 << (irqno - 8);
		startidx = (ipl << 1);
	} else {
		vectmask = 1 << irqno;
		startidx = (ipl << 1) + 1;
	}

	/*
	 *	mask intr same or above ipl
	 *	level MAXIPL has all intr off as init. default
	 */
	pp = &pics0;
	for (i = startidx; i < (MAXIPL << 1); i += 2) {
		if (pp->c_iplmask[i] & vectmask)
			break;
		pp->c_iplmask[i] |= vectmask;
	}

	/*	unmask intr below ipl					*/
	for (i = startidx-2; i >= 0; i -= 2) {
		if (!(pp->c_iplmask[i] & vectmask))
			break;
		pp->c_iplmask[i] &= ~vectmask;
	}
	return (0);
}

static int
uppc_delspl(int irqno, int ipl, int min_ipl, int max_ipl)
{
	register struct standard_pic *pp;
	register int	i;
	u_char	vectmask;

	/*
	 * skip if we are not deleting the last handler
	 * and the ipl is higher than minimum
	 */
	if ((max_ipl != PSM_INVALID_IPL) && (ipl >= min_ipl))
		return (0);

	if (irqno > 7) {
		vectmask = 1 << (irqno - 8);
		i = 0;
	} else {
		vectmask = 1 << irqno;
		i = 1;
	}

	pp = &pics0;

	/*	check any handlers left for this irqno			*/
	if (max_ipl != PSM_INVALID_IPL) {
		/* unmasks all levels below the lowest priority 	*/
		i += ((min_ipl - 1) << 1);
		for (; i >= 0; i -= 2) {
			if (!(pp->c_iplmask[i] & vectmask))
				break;
			pp->c_iplmask[i] &= ~vectmask;
		}
	} else {
		/* set mask to all levels				*/
		for (; i < (MAXIPL << 1); i += 2) {
			if (pp->c_iplmask[i] & vectmask)
				break;
			pp->c_iplmask[i] |= vectmask;
		}
	}
	return (0);
}

static processorid_t
uppc_get_next_processorid(processorid_t cpu_id)
{
	if (cpu_id == -1)
		return (0);
	return (-1);
}

/*ARGSUSED*/
static int
uppc_get_clockirq(int ipl)
{
	return (CLOCK_VECTOR);
}
