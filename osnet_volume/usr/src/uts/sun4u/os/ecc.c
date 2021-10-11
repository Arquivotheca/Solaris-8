/*
 * Copyright (c) 1989-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ecc.c	1.93	99/07/28 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/kmem.h>
#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/machthread.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/hat.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/vmsystm.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/cmn_err.h>
#include <sys/async.h>
#include <sys/spl.h>
#include <sys/trap.h>
#include <sys/machtrap.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/debug.h>
#include <sys/x_call.h>
#include <sys/membar.h>
#include <sys/ivintr.h>
#include <sys/cred.h>
#include <sys/atomic_prim.h>
#include <sys/cpu_module.h>
#include <sys/ftrace.h>

static void ecc_error_init(void);

static int handle_ce_error(void);
void ce_count_unum(int status, int len, char *unum);
void ce_log_status(int status, char *unum);

static int handle_ue_error(void);

static int handle_bto_error(void);
static void kill_proc(struct proc *up, caddr_t addr);


#define	MAX_CE_ERROR	255
#define	MAX_SIMM	8
#define	MAX_CE_FLTS	10
#define	MAX_UE_FLTS	2
#define	MAX_BTO_FLTS	2

struct ce_info {
	char    name[UNUM_NAMLEN];
	short	intermittent_cnt;
	short	persistent_cnt;
	short	sticky_cnt;
};

#define	MAX_BUS_FUNCS	120 /* XXX - 30 max sysio/pci devices on sunfire */
static struct bus_func register_func[MAX_BUS_FUNCS];
static int nfunc = 0;

struct ce_info  *mem_ce_simm = NULL;
int mem_ce_simm_size = 0;
short	max_ce_err = MAX_CE_ERROR;

struct	async_flt *ce_flt_in = NULL;	/* correctable errors in queue */
struct	async_flt *ce_flt_out = NULL;	/* correctable errors out queue */
int	ce_flt_size = 0;
int	nce = 0;
int	oce = 0;
u_int	ce_inum, ce_pil = PIL_1;
kmutex_t ce_spin_mutex;
kmutex_t ce_mutex;
u_int	dropped_ces;			/* dropped events due to q overflow */

struct	async_flt *ue_flt_in = NULL;	/* uncorrectable errors in queue */
struct	async_flt *ue_flt_out = NULL;	/* uncorrectable errors out queue */
int	ue_flt_size = 0;
int	nue = 0;
int	oue = 0;
u_int	ue_inum, ue_pil = PIL_2;
kmutex_t ue_spin_mutex;
kmutex_t ue_mutex;
u_int	dropped_ues;			/* dropped events due to q overflow */

struct	async_flt *to_flt_in = NULL;	/* bus/timeout errors in queue */
struct	async_flt *to_flt_out = NULL;	/* bus/timeout errors out queue */
int	to_flt_size = 0;
int	nto = 0;
int	oto = 0;
u_int	to_inum, to_pil = PIL_1;
kmutex_t bto_spin_mutex;
kmutex_t bto_mutex;
u_int	dropped_btos;			/* dropped events due to q overflow */

int	ce_verbose = 0;
int	ce_enable_verbose = 0;
int	ce_show_data = 0;
int	ce_debug = 0;
int	ue_debug = 0;
int	to_debug = 0;
int	reset_debug = 0;


/*
 * Allocate error arrays based on max_ncpus.  max_ncpus is set just
 * after ncpunode has been determined.  ncpus is set in start_other_cpus
 * which is called after error_init() but may change dynamically.
 */
void
error_init(void)
{
	char tmp_name[MAXSYSNAME];
	dnode_t node;
	size_t size;
	extern int max_ncpus;

	if ((mem_ce_simm == NULL) &&
	    (ce_flt_in == NULL) && (ce_flt_out == NULL) &&
	    (ue_flt_in == NULL) && (ue_flt_out == NULL))
		ecc_error_init();

	if ((to_flt_in == NULL) || (to_flt_out == 0)) {
		to_flt_size = MAX_BTO_FLTS * max_ncpus;
		size = ((sizeof (struct async_flt)) * to_flt_size);
		to_flt_in = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
		to_flt_out = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
		if ((to_flt_in == NULL) || (to_flt_out == NULL)) {
			cmn_err(CE_PANIC,
			    "No space for BTO error initialization");
		}
		to_inum = add_softintr(to_pil, (intrfunc)handle_bto_error,
		    NULL);
		mutex_init(&bto_mutex, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&bto_spin_mutex, NULL, MUTEX_SPIN, (void *)PIL_15);
	}

	node = prom_rootnode();
	if ((node == OBP_NONODE) || (node == OBP_BADNODE)) {
		cmn_err(CE_CONT, "error_init: node 0x%x\n", (u_int)node);
		return;
	}
	if (((size = prom_getproplen(node, "reset-reason")) != -1) &&
	    (size <= MAXSYSNAME) &&
	    (prom_getprop(node, "reset-reason", tmp_name) != -1)) {
		if (reset_debug) {
			cmn_err(CE_CONT, "System booting after %s\n", tmp_name);
		} else if (strncmp(tmp_name, "FATAL", 5) == 0) {
			cmn_err(CE_CONT,
			    "System booting after fatal error %s\n", tmp_name);
		}
	}
}

/*
 * Allocate error arrays based on max_ncpus.
 */
static void
ecc_error_init(void)
{
	size_t size;
	extern int max_ncpus;

	mem_ce_simm_size = MAX_SIMM * max_ncpus;
	size = ((sizeof (struct ce_info)) * mem_ce_simm_size);
	mem_ce_simm = (struct ce_info *)kmem_zalloc(size, KM_SLEEP);
	if (mem_ce_simm == NULL)
		cmn_err(CE_PANIC, "No space for CE unum initialization");

	ce_flt_size = MAX_CE_FLTS * max_ncpus;
	size = ((sizeof (struct async_flt)) * ce_flt_size);
	ce_flt_in = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
	ce_flt_out = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
	if ((ce_flt_in == NULL) || (ce_flt_out == NULL))
		cmn_err(CE_PANIC, "No space for CE error initialization");
	ce_inum = add_softintr(ce_pil, (intrfunc)handle_ce_error, NULL);
	mutex_init(&ce_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ce_spin_mutex, NULL, MUTEX_SPIN, (void *)PIL_14);

	ue_flt_size = MAX_UE_FLTS * max_ncpus;
	size = ((sizeof (struct async_flt)) * ue_flt_size);
	ue_flt_in = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
	ue_flt_out = (struct async_flt *)kmem_zalloc(size, KM_NOSLEEP);
	if ((ue_flt_in == NULL) || (ue_flt_out == NULL))
		cmn_err(CE_PANIC, "No space for UE error initialization");
	ue_inum = add_softintr(ue_pil, (intrfunc)handle_ue_error, NULL);
	mutex_init(&ue_mutex, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ue_spin_mutex, NULL, MUTEX_SPIN, (void *)PIL_15);
}

/*
 * Turn off all error detection, normally only used for panics.
 */
void
error_disable(void)
{
	int n, nf;
	caddr_t arg;
	afunc errdis_func;

	cpu_disable_errors();
	nf = nfunc;
	for (n = 0; n < nf; n++) {
		if (register_func[n].ftype != DIS_ERR_FTYPE)
			continue;
		errdis_func = register_func[n].func;
		ASSERT(errdis_func != NULL);
		arg = register_func[n].farg;
		(void) (*errdis_func)(arg);
	}
}

/*
 * Queue bus error and timeout event for softint handling.
 * Assumes fatal errors have already been handled.
 */
void
bto_error(struct async_flt *bto)
{
	int tn;

	ASSERT(to_flt_in != NULL);	/* ring buffer not initialized */
	if (to_flt_in == NULL)		/* during early bootup code */
		return;

	mutex_enter(&bto_spin_mutex);
	if (to_flt_in[nto].flt_in_use != 0) {	/* ring buffer totally full */
		dropped_btos++;
		mutex_exit(&bto_spin_mutex);	/* do not overwrite data */
		return;
	}
	tn = nto;				/* current slot */
	if (++nto >= to_flt_size)		/* next (empty?) slot */
		nto = 0;
	bto->flt_in_use = 1;
	to_flt_in[tn] = *bto;
	mutex_exit(&bto_spin_mutex);

	setsoftint(to_inum);
}

/*
 * Softint handler for non-fatal bus errors and timeouts
 */
static int
handle_bto_error(void)
{
	int snto;
	struct async_flt bto;
	int i, no;

	/*
	 * If another cpu has the mutex then it may handle it, but
	 * let's get the mutex anyway to handle race conditions
	 * on the processing of the to_flt_out buffer.
	 */
	mutex_enter(&bto_mutex);
	mutex_enter(&bto_spin_mutex);
	snto = nto;			/* next (empty?) slot */
	bcopy(to_flt_in, to_flt_out,
	    ((sizeof (struct async_flt)) * to_flt_size));
	bzero(to_flt_in, ((sizeof (struct async_flt)) * to_flt_size));
	mutex_exit(&bto_spin_mutex);

	if (oto < snto)			/* play catch up to nto */
		no = snto - oto;
	else
		no = (to_flt_size - oto) + snto;

	/* get all queued up softint bto errors */
	for (i = 0; i < no; i++) {
		bto = to_flt_out[oto];
		if (++oto >= to_flt_size)
			oto = 0;
		if (bto.flt_in_use == 0)
			continue;		/* already handled */
		if (to_debug) {
			cmn_err(CE_CONT, "BTO Error: CPU %d proc %p pc %p\n",
			    bto.flt_inst, (void *)bto.flt_proc,
			    (void *)bto.flt_pc);
		}

		kill_proc(bto.flt_proc, bto.flt_pc);
	}

	mutex_exit(&bto_mutex);
	return (DDI_INTR_CLAIMED);
}

/*
 * Note that we cannot just call trapsig, because due to the softint,
 * the process which caused the signal may not be the curthread.
 */
static void
kill_proc(proc_t *up, caddr_t addr)
{
	kthread_id_t t = proctot(up);
	kthread_id_t tp = curthread;
	int sig = SIGBUS;
	int found = 0;
	sigqueue_t *sqp = kmem_zalloc(sizeof (sigqueue_t), KM_NOSLEEP);

	ASSERT((up != NULL) && (sqp != NULL));
	sqp->sq_info.si_signo = sig;
	sqp->sq_info.si_code = FC_HWERR;
	sqp->sq_info.si_addr = addr;
	do {
		if (tp == t) {
			found = 1;
			break;
		}
	} while ((tp = tp->t_next) != curthread && found == 0);
	if (!found)	/* may have been killed off by other means */
		return;

	mutex_enter(&up->p_lock);
	/*
	 * Avoid an infinite loop if the signal is being
	 * ignored by the process.
	 */
	if (up->p_user.u_signal[sig-1] == SIG_IGN) {
		sigdelset(&t->t_hold, sig);
		up->p_user.u_signal[sig-1] = SIG_DFL;
		sigdelset(&up->p_ignore, sig);
	}
	sigaddqa(up, t, sqp);
	mutex_exit(&up->p_lock);
}

/*
 * Queue a UE event for softint handling.
 * Assumes fatal UEs have already been handled.
 */
void
ue_error(struct async_flt *ecc)
{
	uint64_t t_afsr = ecc->flt_stat;
	uint64_t t_afar = ecc->flt_addr;

	if (ue_flt_in == NULL) {	/* ring buffer not initialized */
		cmn_err(CE_PANIC, "UE Error init: AFSR 0x%08x.%08x "
		    "AFAR 0x%08x.%08x Synd 0x%x Id %d Inst %d",
		    (uint32_t)(t_afsr >> 32), (uint32_t)t_afsr,
		    (uint32_t)(t_afar >> 32), (uint32_t)t_afar,
		    ecc->flt_synd, ecc->flt_bus_id, ecc->flt_inst);
	}

	mutex_enter(&ue_spin_mutex);

	/*
	 * more recent UEs overwrite older UEs
	 */
	if (ue_flt_in[nue].flt_in_use != 0)
		dropped_ues++;

	ecc->flt_in_use = 1;
	ue_flt_in[nue] = *ecc;
	if (++nue >= ue_flt_size)
		nue = 0;
	mutex_exit(&ue_spin_mutex);

	setsoftint(ue_inum);
}

/*
 * Softint handler for non-fatal uncorrectable ecc errors
 */
static int
handle_ue_error(void)
{
	int snue;
	char unum[UNUM_NAMLEN];
	struct async_flt ecc;
	int i, noue;
	int ce = CE_PANIC;

	/*
	 * If another cpu has the mutex then it may handle it, but
	 * let's get the mutex anyway to handle race conditions
	 * on the processing of the ue_flt_out buffer.
	 */
	mutex_enter(&ue_mutex);
	mutex_enter(&ue_spin_mutex);
	snue = nue;
	bcopy(ue_flt_in, ue_flt_out,
	    ((sizeof (struct async_flt)) * ue_flt_size));
	bzero(ue_flt_in, ((sizeof (struct async_flt)) * ue_flt_size));
	mutex_exit(&ue_spin_mutex);

	if (oue < snue)			/* play catch up to nue */
		noue = snue - oue;
	else
		noue = (ue_flt_size - oue) + snue;

	/* look for all queued up softint ue errors */
	for (i = 0; i < noue; i++) {
		ecc = ue_flt_out[oue];
		if (++oue >= ue_flt_size)
			oue = 0;
		if (ecc.flt_in_use == 0)	/* other cpu handled it */
			continue;

		if (cpu_ue_log_err(&ecc, unum) == UE_DEBUG)
			ce = CE_WARN;

		cmn_err(ce, "UE Error: AFSR 0x%08x.%08x "
		    "AFAR 0x%08x.%08x Id %d Inst %d MemMod %s\n",
		    (uint32_t)(ecc.flt_stat >> 32), (uint32_t)ecc.flt_stat,
		    (uint32_t)(ecc.flt_addr >> 32), (uint32_t)ecc.flt_addr,
		    ecc.flt_bus_id, ecc.flt_inst, unum);
	}

	mutex_exit(&ue_mutex);
	return (DDI_INTR_CLAIMED);
}

/*
 * check the registered iobus UE_ECC_FTYPE functions for any
 * non interrupting or trapping errors that would cause a UE
 */
int
ue_check_buses(void)
{
	afunc ue_func;
	caddr_t arg;
	int n, nf;
	int fatal = 0;

	nf = nfunc;
	for (n = 0; n < nf; n++) {
		if (register_func[n].ftype != UE_ECC_FTYPE)
			continue;
		ue_func = register_func[n].func;
		ASSERT(ue_func != NULL);
		arg = register_func[n].farg;
		fatal = (*ue_func)(arg);
	}
	return (fatal);
}

/*
 * Scrub a non-fatal correctable ecc error.
 * Queue the correctable ecc event for softint handling.
 */
void
ce_error(struct async_flt *ecc)
{
	int tnce;

	/*
	 * Can't log ftrace point here; we're at high PIL.
	 * Do it from the soft interrupt handler.
	 */
	ASSERT(ce_flt_in != NULL);	/* ring buffer not initialized */
	if (ce_flt_in == NULL)		/* during early bootup code */
		return;

/* TODO: why is this mutex_enter needed? what does the comment mean? */

	mutex_enter(&ce_spin_mutex);	/* scrub within mutex to ensure */
					/* ecc errors remain enabled */

	if (ecc->flt_in_memory)
		cpu_ce_scrub_mem_err(ecc);

	if (ce_flt_in[nce].flt_in_use != 0) {	/* ring buffer totally full */
		dropped_ces++;
		mutex_exit(&ce_spin_mutex);	/* do not overwrite data */
		return;
	}
	tnce = nce;				/* current slot */
	if (++nce >= ce_flt_size)		/* next (empty?) slot */
		nce = 0;
	ecc->flt_in_use = 1;
	ce_flt_in[tnce] = *ecc;
	mutex_exit(&ce_spin_mutex);

	setsoftint(ce_inum);
}

/*
 * Softint handler for correctable ecc errors
 */
static int
handle_ce_error(void)
{
	int snce;
	struct async_flt ecc;
	int i, noce;

	/*
	 * If another cpu has the mutex then it may handle it, but
	 * let's get the mutex anyway to handle race conditions
	 * on the processing of the ce_flt_out buffer.
	 */
	mutex_enter(&ce_mutex);
	mutex_enter(&ce_spin_mutex);
	snce = nce;			 /* next (empty?) slot */
	bcopy(ce_flt_in, ce_flt_out,
	    ((sizeof (struct async_flt)) * ce_flt_size));
	bzero(ce_flt_in, ((sizeof (struct async_flt)) * ce_flt_size));
	mutex_exit(&ce_spin_mutex);

	/*
	 * process the queue of CEs
	 */
	if (oce < snce)		/* play catch up to nce */
		noce = snce - oce;
	else
		noce = (ce_flt_size - oce) + snce;

	/* get all queued up softint ce errors */
	for (i = 0; i < noce; i++) {
		ecc = ce_flt_out[oce];
		if (++oce >= ce_flt_size)
			oce = 0;
		if (ecc.flt_in_use == 0)
			continue;
		cpu_ce_log_err(&ecc);
	}

	mutex_exit(&ce_mutex);
	return (DDI_INTR_CLAIMED);
}

/*
 * Called by correctable ecc error logging code to keep
 * a running count of the ce errors on a per-unum basis.
 *
 * Errors are saved in three buckets per-unum:
 * (1) sticky - scrub was unsuccessful, cannot be scrubbed
 * (2) persistent - was successfully scrubbed
 * (3) intermittent - may have originated from the cpu or upa/safari bus,
 *     and does not necessarily indicate any problem with the dimm itself,
 *     is critical information for debugging new hardware.
 */
void
ce_count_unum(int status, int len, char *unum)
{
	int i;
	struct ce_info *psimm = mem_ce_simm;

	ASSERT(psimm != NULL);
	if (len > 0) {
	    for (i = 0; i < mem_ce_simm_size; i++) {
		if (psimm[i].name[0] == NULL) {
			(void) strncpy(psimm[i].name, unum, len);
			if (status & ECC_STICKY) {
				psimm[i].intermittent_cnt = 0;
				psimm[i].persistent_cnt = 0;
				psimm[i].sticky_cnt = 1;
			} else if (status & ECC_PERSISTENT) {
				psimm[i].intermittent_cnt = 0;
				psimm[i].persistent_cnt = 1;
				psimm[i].sticky_cnt = 0;
			} else {
				psimm[i].intermittent_cnt = 1;
				psimm[i].persistent_cnt = 0;
				psimm[i].sticky_cnt = 0;
			}
			break;
		} else if (strncmp(unum, psimm[i].name, len) == 0) {
			if (status & ECC_STICKY) {
				psimm[i].sticky_cnt += 1;
			} else if (status & ECC_PERSISTENT) {
				psimm[i].persistent_cnt += 1;
			} else {
				psimm[i].intermittent_cnt += 1;
			}
			if ((psimm[i].persistent_cnt +
			    psimm[i].intermittent_cnt +
			    psimm[i].sticky_cnt) > max_ce_err) {
				cmn_err(CE_CONT, "Multiple Softerrors: ");
				cmn_err(CE_CONT,
				    "%d Intermittent, %d Persistent, and %d "
				    "Sticky Softerrors accumulated ",
				    psimm[i].intermittent_cnt,
				    psimm[i].persistent_cnt,
				    psimm[i].sticky_cnt);
				cmn_err(CE_CONT, "from Memory Module %s\n",
				    unum);
				if ((ce_verbose == 0) && (ce_enable_verbose)) {
				    cmn_err(CE_CONT, "\tEnabling verbose CE "
					"messages.\n");
				    ce_verbose = 1;
				    max_ce_err = MAX_CE_ERROR;
				} else if (ce_enable_verbose == 0) {
				    cmn_err(CE_CONT, "\tCONSIDER REPLACING "
					"THE MEMORY MODULE.\n");
				}
				psimm[i].intermittent_cnt = 0;
				psimm[i].persistent_cnt = 0;
				psimm[i].sticky_cnt = 0;
			}
			break;
		}
	    }
	    if (i >= mem_ce_simm_size)
		    cmn_err(CE_CONT, "Softerror: mem_ce_simm[] out of "
			"space.\n");
	}
}

/*
 * Called by correctable ecc error logging code to print out
 * the stick/persistent/intermittent status of the error.
 */
void
ce_log_status(int status, char *unum)
{
	char *status1_str = "Intermittent";
	char *status2_str = "Memory";

	if (status & ECC_ECACHE)
		status2_str = "Ecache";
	if (status & ECC_STICKY)
		status1_str = "Sticky";
	else if (status & ECC_PERSISTENT)
		status1_str = "Persistent";

	cmn_err(CE_CONT, "Softerror: %s ECC %s Error, %s\n",
	    status1_str, status2_str, unum);
}

/*
 * Called by non-cpu drivers, i.e. iobus nexus, to register callback
 * routines for the following two functions:
 * (1) UE_ECC_FTYPE - uncorrectable ecc errors that do not cause an interrupt.
 * (2) DIS_ERR_FTYPE - disable all error interrupts.
 */
void
register_bus_func(short type, afunc func, caddr_t arg)
{
	int nidx, curidx;

	ASSERT(register_func != NULL);
	do {
		curidx = nfunc;
		nidx = atinc_cidx_word(&nfunc, MAX_BUS_FUNCS - 1);
		if (nidx == 0) {	/* wrapped, queue is fullup */
			cmn_err(CE_WARN, "Bus function queue wrapped\n");
			return;
		}
	} while (nidx != (curidx + 1));
	register_func[curidx].ftype = type;
	register_func[curidx].func = func;
	register_func[curidx].farg = arg;
}

/*
 * Called by non-cpu drivers, i.e. iobus nexus, to unregister
 * callback routines.
 */
void
unregister_bus_func(caddr_t arg)
{
	int n;

	for (n = 0; n < nfunc; n++) {
		if (register_func[n].farg == arg) {
			/* take care of boundry condition */
			if (n != nfunc - 1) {
				register_func[n].ftype =
				    register_func[nfunc - 1].ftype;
				register_func[n].func =
				    register_func[nfunc - 1].func;
				register_func[n].farg =
				    register_func[nfunc - 1].farg;
				register_func[nfunc - 1].ftype = 0;
				register_func[nfunc - 1].func = 0;
				register_func[nfunc - 1].farg = 0;
				n--;
			} else {
				register_func[n].ftype = 0;
				register_func[n].func = 0;
				register_func[n].farg = 0;
			}
			nfunc--;
		}
	}
}
