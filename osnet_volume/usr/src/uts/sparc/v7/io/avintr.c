/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)avintr.c	1.24	99/04/14 SMI"	/* SVr4 */

/*
 * Autovectored Interrupt Configuration and Deconfiguration (tread carefully)
 */
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/scb.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/t_lock.h>
#include <sys/atomic.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/cpuvar.h>
#include <sys/sunddi.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/debug.h>

kmutex_t av_lock;

int
not_serviced(int *counter, int level,  char *bus)
{
	if (++(*counter) % 100 == 0)
		cmn_err(CE_WARN, "processor level %d %sinterrupt not serviced",
		    level, bus);
	return (AV_INT_SPURIOUS);
}

/*
 * Arrange for a driver to be called when a particular
 * auto-vectored interrupt occurs.
 * NOTE: if a device can generate interrupts on more than
 * one level, or if a driver services devices that interrupt
 * on more than one level, then the driver should install
 * itself on each of those levels.
 */
static char averrmsg[] =
	{"add_avintr: %s%d: autovectored interrupts at level 0x%x %s"};
static char nochange[] =
	{"add_avintr: %s%d: cannot change level 0x%x interrupt handler"};
static char nohandler[] =
	{"add_avintr: %s%d: cannot add null level 0x%x interrupt handler!"};

int
add_avintr(dev_info_t *devi, int lvl, avfunc xxintr, caddr_t arg)
{
	register avfunc f;
	volatile struct autovec *levelp;
	register int i;
	register char *name = ddi_get_name(devi);
	register int inst = ddi_get_instance(devi);

	if ((f = xxintr) == NULL) {
		cmn_err(CE_WARN, nohandler, name, inst, lvl);
		return (0);
	}
	if (lvl >= maxautovec) {
		cmn_err(CE_WARN, averrmsg, name, inst, lvl,
		    "level out of range");
		return (0);
	}
	if (exclude_level(lvl)) {
		cmn_err(CE_WARN, averrmsg, name, inst, lvl,
		    "not supported on this processor");
		return (0);
	}

	levelp = vectorlist[lvl];
	if (levelp == (struct autovec *)0) {
		cmn_err(CE_WARN, averrmsg, name, inst, lvl,
		    "are not supported");
		return (0);
	}

	mutex_enter(&av_lock);

	/*
	 * If fast != 0, then a settrap() claimed this level
	 */

	if (levelp->av_fast) {
		mutex_exit(&av_lock);
		cmn_err(CE_WARN, averrmsg, name, inst, lvl,
		    "reserved for fast trap");
		return (0);
	}
	for (i = 0; i < NVECT; i++) {
		if (levelp->av_vector == xxintr && levelp->av_devi == devi) {
			mutex_exit(&av_lock);
			cmn_err(CE_WARN, nochange, name, inst, lvl);
			return (0);
		}
		if (levelp->av_vector == NULL)	/* end of list found */
			break;
		levelp++;
	}

	/*
	 * Because the locore code (and rem_avintr) depend on finding
	 * a NULL av_vector entry to mark the end of the list, we have to
	 * stop before we fill the list up.
	 */

	if (i >= NVECT - 1) {
		mutex_exit(&av_lock);
		cmn_err(CE_WARN, averrmsg, name, inst, lvl,
		    "has too many devices");
		return (0);
	}

	/*
	 * The order of these writes is important!
	 */
	levelp->av_devi = devi;
	membar_producer();
	levelp->av_intarg = arg;
	membar_producer();
	levelp->av_vector = f;
	mutex_exit(&av_lock);
	return (1);
}

/*
 * Remove a driver from the autovector list.
 *
 * NOTE: if a driver has installed itself on more than one level,
 * then the driver should remove itself from each of those levels.
 */
static char notinst[] =
	{"rem_avintr: %s%d: level 0x%x interrupt handler not installed"};

/*
 * After having made a change to an autovector list, wait until we have
 * seen each cpu not executing an interrupt at that level--so we know our
 * change has taken effect completely (no old state in registers, etc).
 *
 * XXX this test does not work if release_intr has been called by the
 * XXX driver we're attempting to unload!
 *
 * This function is now also used elsewhere (after setting up an async fault
 * handler).
 */
void
wait_till_seen(int ipl)
{
	register int cpus_to_check, cpu_in_chain, cix;
	register struct cpu *cpup;

	CPUSET_ALL(cpus_to_check);
	do {
		cpu_in_chain = 0;
		for (cix = 0; cix < NCPU; cix++) {
			cpup = cpu[cix];
			if (cpup != NULL && CPU_IN_SET(cpus_to_check, cix)) {
				if (intr_active(cpup, ipl)) {
					cpu_in_chain = 1;
				} else {
					CPUSET_DEL(cpus_to_check, cix);
				}
			}
		}
	} while (cpu_in_chain);
}

/*ARGSUSED*/
u_int
nullintr(caddr_t intrg)
{
	return (DDI_INTR_UNCLAIMED);
}


/*
 * This routine assumes that it is not called from interrupt context
 */
void
rem_avintr(dev_info_t *devi, int lvl, avfunc xxintr)
{
	volatile struct autovec *levelp, *endp;
	int i, found, ipl;
	char *name = ddi_get_name(devi);
	int inst = ddi_get_instance(devi);

	ASSERT(xxintr != NULL && lvl < maxautovec);

	levelp = vectorlist[lvl];

	ASSERT(levelp != (struct autovec *)0);

	mutex_enter(&av_lock);

	/*
	 * First, find the end of the list
	 */
	for (endp = levelp; endp->av_vector != NULL; endp++)
		;
	if (endp == levelp) {
		mutex_exit(&av_lock);
		cmn_err(CE_WARN, notinst, name, inst, lvl);
		return;
	}

	--endp;

	ipl = INT_IPL(lvl);

	found = 0;

	for (i = 0; i < NVECT; i++, levelp++) {
		if (levelp->av_vector == NULL) {
			break;
		}
		if (levelp->av_vector == xxintr &&
		    levelp->av_devi == devi) {
			found = 1;
			/*
			 * The order of these writes is important!
			 */
			levelp->av_vector = nullintr;
			/*
			 * protect against calling the handler being removed
			 * with the arg meant for the handler at the end of
			 * the list
			 */
			wait_till_seen(ipl);
			levelp->av_devi = endp->av_devi;
			levelp->av_intarg = endp->av_intarg;
			levelp->av_vector = endp->av_vector;
			/*
			 * protect against removing the handler at the end of
			 * the list after some cpu has already passed (missed)
			 * the new copy of it in the list
			 * (prevents spurious interrupt message)
			 */
			wait_till_seen(ipl);
			endp->av_vector = NULL;
			/*
			 * protect against passing the arg meant for a
			 * new handler being added to the list (by
			 * add_avintr) to the handler we've just removed
			 * from the end of the list
			 */
			wait_till_seen(ipl);
			break;
		}
	}

	mutex_exit(&av_lock);
	if (found)
		return;
	/*
	 * One way or another, it isn't in the list.
	 */
	cmn_err(CE_WARN, notinst, name, inst, lvl);
}

/*
 * Opcodes for instructions in scb entries
 */

#define	MOVPSRL0	0xa1480000	/* mov	%psr, %l0	*/
#define	MOVL4		0xa8102000	/* mov	(+/-0xfff), %l4	*/
#define	MOVL6		0xac102000	/* mov	(+/-0xfff), %l6	*/
#define	BRANCH		0x10800000	/* ba	(+/-0x1fffff)*4	*/
#define	SETHIL3		0x27000000	/* sethi %hi(0xfffffc00), %l3	*/
#define	JMPL3		0x81c4e000	/* jmp	%l3+%lo(0x3ff)	*/
#define	NO_OP		0x01000000	/* nop			*/

#define	BRANCH_RANGE	0x001fffff	/* max branch displacement */
#define	BRANCH_MASK	0x003fffff	/* branch displacement mask */
#define	SETHI_HI(x)	(((x) >> 10) & 0x003fffff)	/* %hi(x) */
#define	JMP_LO(x)	((x) & 0x000003ff)		/* %lo(x) */

/*
 * Dynamically set a hard trap vector in the scb table.
 * This routine allows device drivers, such as fd and audio_79C30,
 * to dynamically configure assembly-language interrupt handlers.
 * If 'xxintr' is NULL, the trap vector is set back to the default system trap.
 *
 * If this trap can be autovectored, set the av_fast field when a
 * direct trap vector is set, so that two devices can't claim the same
 * interrupt if one of them is a fast trap.
 *
 * Return 0 if the trap vector could not be set according to the request.
 * Otherwise, return 1.
 */

static char fbusy[] =
	{"settrap: %s%d: level 0x%x fast trap already busy"};
static char fvec[] =
	{"settrap: %s%d: level 0x%x autovectored--cannot add fast intr"};
static char fnbusy[] =
	{"settrap: %s%d: level 0x%x fast trap not set"};

int
settrap(dev_info_t *devi, int lvl, avfunc xxintr)
{
	extern interrupt_prologue();
	extern int	nwindows;	/* number of register windows */
	struct autovec *levelp;
	trapvec vec;
	int offset, i;
	char *name = ddi_get_name(devi);
	int inst = ddi_get_instance(devi);

	/*
	 * Find the appropriate autovector table
	 */

	if (lvl >= maxautovec) {
		cmn_err(CE_WARN,
		    "settrap: %s%d: vector number 0x%x out of range",
		    name, inst, lvl);
		return (0);
	}
	lvl = INT_IPL(lvl);

	if (exclude_settrap(lvl)) {
		return (0);
	}
	levelp = vectorlist[lvl];

	/*
	 * Check to see whether trap is available.
	 * If fast is non-zero, this is a direct trap.
	 * NOTE: We only use the first set (level) of autovectors to keep
	 * track of direct traps, but we have to check all levels to see if
	 * the trap is already in use.  Since this is an infrequent
	 * operation, it seemed better than trying to do complicated
	 * bookkeeping at add interrupt time.
	 */
	mutex_enter(&av_lock);
	if (levelp != NULL) {
		if (xxintr != NULL) {
			/* If trying to set a direct trap handler */
			if (levelp->av_fast) {
				mutex_exit(&av_lock);
				cmn_err(CE_WARN, fbusy, name, inst, lvl);
				return (0);
			}
		} else {
			if (!levelp->av_fast) {
				mutex_exit(&av_lock);
				cmn_err(CE_WARN, fnbusy, name, inst, lvl);
				return (0);
			}
		}
	} else {
		cmn_err(CE_PANIC, "%s%d: settrap level 0x%x", name, inst, lvl);
		/*NOTREACHED*/
	}
	for (i = 0; i < maxautovec; i += MAXIPL) {
		struct autovec *wrklevelp = vectorlist[i+lvl];
		if (wrklevelp != NULL && xxintr != NULL) {
			/* If trying to set a direct trap handler */
			if (wrklevelp->av_vector != 0) {
				mutex_exit(&av_lock);
				cmn_err(CE_WARN, fvec, name, inst, lvl);
				return (0);
			}
		}
	}
	/*
	 * If we're still here, then we can do it
	 */
	if (xxintr == NULL) {
		/* Setting back to system trap */
		levelp->av_fast = 0;

		/*
		 * Reset the trap vector to a default system trap
		 */

		vec.instr[0] = MOVPSRL0;

		/* Construct a load instruction */
		vec.instr[1] = MOVL4 | lvl;

		/*
		 * Branch to 'interrupt_prologue'
		 */

		vec.instr[2] = BRANCH |
		    (BRANCH_MASK & (((int)interrupt_prologue -
		    (int)&scb.interrupts[lvl - 1].instr[2]) >> 2));

		vec.instr[3] = MOVL6 | (nwindows - 1);

	} else {
		/* don't autovector */
		levelp->av_fast = 1;

		vec.instr[0] = MOVPSRL0;

		/*
		 * Construct a branch instruction
		 * with the given trap address.
		 */

		offset = (((int)xxintr -
		    (int)&scb.interrupts[lvl - 1].instr[1]) >> 2);

		/*
		 * Check that branch displacement is within range
		 */
		if (((offset & ~BRANCH_RANGE) == 0) ||
		    ((offset & ~BRANCH_RANGE) == ~BRANCH_RANGE)) {
			vec.instr[1] = BRANCH | (offset & BRANCH_MASK);
			vec.instr[2] = MOVL6 | (nwindows - 1);
		} else {
			/* Branch is too far, so use a jump */
			vec.instr[1] = SETHIL3 | SETHI_HI((int)xxintr);
			vec.instr[2] = JMPL3 | JMP_LO((int)xxintr);
			vec.instr[3] = MOVL6 | (nwindows - 1);
		}
	}
	write_scb_int(lvl, &vec);	/* set the new vector */
	mutex_exit(&av_lock);

	return (1);
}
