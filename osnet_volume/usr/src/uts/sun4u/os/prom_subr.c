/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prom_subr.c	1.18	99/04/14 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/cmn_err.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/machsystm.h>
#include <sys/archsystm.h>
#include <sys/x_call.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/privregs.h>
#include <sys/vmem.h>
#include <sys/atomic.h>
#include <sys/panic.h>
#include <sys/cpu_sgnblk_defs.h>

/*
 * Console put and get character routines.
 */
void
cnputs(const char *buf, uint_t bufsize, int device_in_use)
{
	if (device_in_use) {
		int s;
		/*
		 * This means that some other CPU may have a mapping
		 * to the device (framebuffer) that the OBP is about
		 * to render onto.  Some of the fancier framebuffers get upset
		 * about being accessed by more than one CPU - so stop
		 * the others in their tracks.
		 *
		 * This is a somewhat unfortunate 'hackaround' to the general
		 * problem of sharing a device between the OBP and userland.
		 *
		 * This should happen -very- rarely on a running system
		 * provided you have a console window redirecting console
		 * output when running your favourite window system ;-)
		 */
		s = splhi();
		idle_other_cpus();
		prom_writestr(buf, bufsize);
		resume_other_cpus();
		splx(s);
	} else
		prom_writestr(buf, bufsize);
}

void
cnputc(int c, int device_in_use)
{
	int s;

	if (device_in_use) {
		s = splhi();
		idle_other_cpus();
	}

	if (c == '\n')
		prom_putchar('\r');
	prom_putchar((char)c);

	if (device_in_use) {
		resume_other_cpus();
		splx(s);
	}
}

static int
cngetc(void)
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 *
 * XXX	There's no need for both cngetc() and getchar() -- merge 'em
 */
static int
getchar(void)
{
	int c;

	c = cngetc();
	if (c == '\r')
		c = '\n';
	cnputc(c, 0);
	return (c);
}

/*
 * Get a line from the console.
 */
void
gets(char *cp)
{
	char *lp;
	int c;

	lp = cp;
	for (;;) {
		c = getchar() & 0177;
		switch (c) {

		case '\n':
			*lp++ = '\0';
			return;

		case 0177:
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;

		case 'u'&037:
			lp = cp;
			cnputc('\n', 0);
			continue;

		case '@':	/* used in physical device names! */
		default:
			*lp++ = (char)c;
		}
	}
}

/*
 * We are called with a pointer to a cell-sized argument array.
 * The service name (the first element of the argument array) is
 * the name of the callback being invoked.  When called, we are
 * running on the firmwares trap table as a trusted subroutine
 * of the firmware.
 *
 * We define entry points to allow callback handlers to be dynamically
 * added and removed, to support obpsym, which is a separate module
 * and can be dynamically loaded and unloaded and registers its
 * callback handlers dynamically.
 *
 * Note: The actual callback handler we register, is the assembly lang.
 * glue, callback_handler, which takes care of switching from a 64
 * bit stack and environment to a 32 bit stack and environment, and
 * back again, if the callback handler returns. callback_handler calls
 * vx_handler to process the callback.
 */

int vx_entered;
static kmutex_t vx_cmd_lock;	/* protect vx_cmd table */

#define	VX_CMD_MAX	10
#define	ENDADDR(a)	&a[sizeof (a) / sizeof (a[0])]
#define	vx_cmd_end	((struct vx_cmd *)(ENDADDR(vx_cmd)))

static struct vx_cmd {
	char	*service;	/* Service name */
	int	take_tba;	/* If Non-zero we take over the tba */
	void	(*func)(cell_t *argument_array);
} vx_cmd[VX_CMD_MAX+1];

void
init_vx_handler(void)
{
	/*
	 * initialize the lock protecting additions and deletions from
	 * the vx_cmd table.  At callback time we don't need to grab
	 * this lock.  Callback handlers do not need to modify the
	 * callback handler table.
	 */
	mutex_init(&vx_cmd_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Add a kernel callback handler to the kernel's list.
 * The table is static, so if you add a callback handler, increase
 * the value of VX_CMD_MAX. Find the first empty slot and use it.
 */
void
add_vx_handler(char *name, int flag, void (*func)(cell_t *))
{
	struct vx_cmd *vp;

	mutex_enter(&vx_cmd_lock);
	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == NULL) {
			vp->service = name;
			vp->take_tba = flag;
			vp->func = func;
			mutex_exit(&vx_cmd_lock);
			return;
		}
	}
	mutex_exit(&vx_cmd_lock);

#ifdef	DEBUG

	/*
	 * There must be enough entries to handle all callback entries.
	 * Increase VX_CMD_MAX if this happens. This shouldn't happen.
	 */
	cmn_err(CE_PANIC, "add_vx_handler <%s>", name);
	/* NOTREACHED */

#else	/* DEBUG */

	cmn_err(CE_WARN, "add_vx_handler: Can't add callback hander <%s>",
	    name);

#endif	/* DEBUG */

}

/*
 * Remove a vx_handler function -- find the name string in the table,
 * and clear it.
 */
void
remove_vx_handler(char *name)
{
	struct vx_cmd *vp;

	mutex_enter(&vx_cmd_lock);
	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == NULL)
			continue;
		if (strcmp(vp->service, name) != 0)
			continue;
		vp->service = 0;
		vp->take_tba = 0;
		vp->func = 0;
		mutex_exit(&vx_cmd_lock);
		return;
	}
	mutex_exit(&vx_cmd_lock);
	cmn_err(CE_WARN, "remove_vx_handler: <%s> not found", name);
}

int
vx_handler(cell_t *argument_array)
{
	char *name;
	struct vx_cmd *vp;
	void *old_tba;
	extern struct scb trap_table;

	name = p1275_cell2ptr(*argument_array);

	for (vp = vx_cmd; vp < vx_cmd_end; vp++) {
		if (vp->service == (char *)0)
			continue;
		if (strcmp(vp->service, name) != 0)
			continue;
		if (vp->take_tba != 0)  {
			reestablish_curthread();
			if (tba_taken_over != 0)
				old_tba = set_tba((void *)&trap_table);
		}
		vp->func(argument_array);
		if ((vp->take_tba != 0) && (tba_taken_over != 0))
			(void) set_tba(old_tba);
		return (0);	/* Service name was known */
	}

	return (-1);		/* Service name unknown */
}


/*
 * These routines are called immediately before and
 * immediately after calling into the firmware.  The
 * firmware is significantly confused by preemption -
 * particularly on MP machines - but also on UP's too.
 *
 * Cheese alert.
 *
 * We have to handle the fact that when slave cpus start, they
 * aren't yet read for mutex's (i.e. they are still running on
 * the prom's tlb handlers, so they will fault if they touch
 * curthread).
 *
 * To handle this, the cas on prom_cpu is the actual lock, the
 * mutex is so "adult" cpus can cv_wait/cv_signal themselves.
 * This routine degenerates to a spin lock anytime a "juvenile"
 * cpu has the lock.
 */
struct cpu *prom_cpu;
kmutex_t prom_mutex;
kcondvar_t prom_cv;

void
kern_preprom(void)
{
	struct cpu *cp, *prcp;

	for (;;) {
		cp = cpu[getprocessorid()];
		if (CPU_IN_SET(cpu_ready_set, getprocessorid()) &&
				cp->cpu_m.mutex_ready) {
			/*
			 * Disable premption, and re-validate cp.  We can't
			 * move from a mutex_ready cpu to a non mutex_ready
			 * cpu, so just getting the current cpu is ok.
			 *
			 * Try the lock.  If we dont't get the lock,
			 * re-enable preemption and see if we should
			 * sleep.
			 */
			kpreempt_disable();
			cp = CPU;
			if (casptr(&prom_cpu, NULL, cp) == NULL)
				break;
			kpreempt_enable();
			/*
			 * We have to be very careful here since both
			 * prom_cpu and prcp->cpu_m.mutex_ready can
			 * be changed at any time by a non mutex_ready
			 * cpu.
			 *
			 * If prom_cpu is mutex_ready, prom_mutex
			 * protects prom_cpu being cleared on us.
			 * If prom_cpu isn't mutex_ready, we only know
			 * it will change prom_cpu before changing
			 * cpu_m.mutex_ready, so we invert the check
			 * order with a membar in between to make sure
			 * the lock holder really will wake us.
			 */
			mutex_enter(&prom_mutex);
			prcp = prom_cpu;
			if (prcp != NULL && prcp->cpu_m.mutex_ready != 0) {
				membar_consumer();
				if (prcp == prom_cpu)
					cv_wait(&prom_cv, &prom_mutex);
			}
			mutex_exit(&prom_mutex);
			/*
			 * Check for panicking.
			 */
			if (panicstr)
				return;
		} else {
			/*
			 * Non mutex_ready cpus just grab the lock
			 * and run with it.
			 */
			ASSERT(getpil() == PIL_MAX);
			if (casptr(&prom_cpu, NULL, cp) == NULL)
				break;
		}
	}
}

void
kern_postprom(void)
{
	struct cpu *cp;

	cp = cpu[getprocessorid()];
	ASSERT(prom_cpu == cp || panicstr);
	prom_cpu = NULL;
	membar_producer();
	if (CPU_IN_SET(cpu_ready_set, getprocessorid()) &&
			cp->cpu_m.mutex_ready) {
		mutex_enter(&prom_mutex);
		cv_signal(&prom_cv);
		mutex_exit(&prom_mutex);
		kpreempt_enable();
	}
}

/*
 * This routine is a special form of pause_cpus().  It ensures that
 * prom functions are callable while the cpus are paused.
 */
void
promsafe_pause_cpus(void)
{
	pause_cpus(NULL);

	/* If some other cpu is entering or is in the prom, spin */
	while (prom_cpu || mutex_owner(&prom_mutex)) {

		start_cpus();
		mutex_enter(&prom_mutex);

		/* Wait for other cpu to exit prom */
		while (prom_cpu)
			cv_wait(&prom_cv, &prom_mutex);

		mutex_exit(&prom_mutex);
		pause_cpus(NULL);
	}

	/* At this point all cpus are paused and none are in the prom */
}


#if defined(PROM_32BIT_ADDRS)

#include <sys/promimpl.h>
#include <vm/seg_kmem.h>
#include <sys/kmem.h>
#include <sys/bootconf.h>

/*
 * These routines are only used to workaround "poor feature interaction"
 * in OBP.  See bug 4115680 for details.
 *
 * Many of the promif routines need to allocate temporary buffers
 * with 32-bit addresses to pass in/out of the CIF.  The lifetime
 * of the buffers is extremely short, they are allocated and freed
 * around the CIF call.  We use vmem_alloc() to cache 32-bit memory.
 */
static vmem_t *promplat_arena;

void *
promplat_alloc(size_t size)
{
	if (promplat_arena == NULL)
		promplat_arena = vmem_create("promplat", NULL, 0, 8,
		    segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);

	return (vmem_alloc(promplat_arena, size, VM_NOSLEEP));
}

void
promplat_free(void *p, size_t size)
{
	vmem_free(promplat_arena, p, size);
}

void
promplat_bcopy(const void *src, void *dst, size_t count)
{
	bcopy(src, dst, count);
}

#endif /* PROM_32BIT_ADDRS */

void
sync_handler(void)
{
	int i;

	/*
	 * Avoid trying to talk to the other CPUs since they are
	 * already sitting in the prom and won't reply.
	 */
	for (i = 0; i < NCPU; i++) {
		if ((i != CPU->cpu_id) && CPU_XCALL_READY(i)) {
			cpu[i]->cpu_flags &= ~CPU_READY;
			cpu[i]->cpu_flags |= CPU_QUIESCED;
			CPUSET_DEL(cpu_ready_set, cpu[i]->cpu_id);
		}
	}

#ifdef _STARFIRE
	/*
	 * If we're re-entering the panic path, update the Starfire signature
	 * block so that the SSP knows we're in the second part of panic.
	 */
	if (panicstr)
		SGN_UPDATE_ALL_OS_RUN_PANIC2();
#endif

	nopanicdebug = 1; /* do not perform debug_enter() prior to dump */
	panic("zero");
}
