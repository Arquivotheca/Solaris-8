/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machstate.c	1.6	99/05/25 SMI"

/*
 * Machine state management functions for kadb on sun4u
 */

#include <sys/types.h>
#include <sys/debug/debugger.h>
#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/fsr.h>
#include <sys/machparam.h>
#include <sys/cpuvar.h>
#include <sys/cpu_sgnblk_defs.h>
#include <sys/promif.h>
#include <sys/spitregs.h>
#include <sys/consdev.h>

#include "adb.h"
#include "allregs.h"
#include "cpusave.h"
#include "sparc.h"

lock_t kadblock = 0;		/* MP lock used by kadb */

int cur_cpuid;
struct cpu_regsave cpusave[NCPU];	/* per-CPU register save area */
int Cpudelay;

/*
 * Kernel callbacks for polled I/O
 */
cons_polledio_t	polled_io;

extern int nwindows;
extern int vac_size;

extern char *sprintf();		/* standalone lib uses char * */
extern int printf();

extern int obtain_lock(lock_t *);
extern int lock_held(lock_t *);
extern void release_lock(lock_t *);

extern int kadb_send_mondo(uint_t, func_t, uint_t, uint_t);
extern void save_cpu_state(void *);
extern void reload_prom_callback(void);

extern unsigned long readstackreg(struct stackpos *, int);

#define	MAX_IDLE_RETRIES	0x10

/*
 *  Called to idle all other CPUs before we do anything.
 */
static void
idle_other_cpus(void)
{
	unsigned i;

	KADB_SGN_UPDATE_OBP();

	for (i = 0; i < NCPU; i++) {
		unsigned    retries = 0;

		while (cpusave[i].cpu_status == CPU_STATUS_RUNNING) {
			unsigned    ack;

			if (retries > MAX_IDLE_RETRIES) {
				printf("No response from cpu %u\n", i);
				break;
			}

			/*
			 *  Deliver a 'mondo' interrupt to the CPU we want
			 *  to idle, passing the address of a routine that
			 *  will force it to idle till we're done.
			 */

			db_printf(1, "idling cpu %d, status %d",
					i, cpusave[i].cpu_status);

			ack = kadb_send_mondo(
				    CPUID_TO_UPAID(i),
				    (func_t)&save_cpu_state,
				    (uintptr_t)&cpusave[i],
				    (uintptr_t)(void *)0);
			if (ack)
				break;

			retries++;
		}
	}
}


static void
resume_other_cpus(void)
{
	unsigned i;

	KADB_SGN_UPDATE_OS();

	/*
	 *  We need to be able to distinguish cpus entering or
	 *  stopped in the debugger from those on their way out.  To
	 *  make sure we don't accidentally conclude that a cpu is
	 *  in the debugger when it's actually leaving, we clear
	 *  each cpu's status before we clear the lock.
	 *
	 *  This code is mostly defensive programming; it is only
	 *  needed if it is possible for one processor to exit and
	 *  re-enter the debugger before some other processor exits.
	 *  Probably that isn't possible.
	 */

	for (i = 0; i < NCPU; i++) {
		if (cpusave[i].cpu_status == CPU_STATUS_SLAVE ||
		    cpusave[i].cpu_status == CPU_STATUS_MASTER)
			cpusave[i].cpu_status = CPU_STATUS_RUNNING;
	}

	release_lock(&kadblock);
}


/*
 * This is the main entry point for any cpu entering the debugger
 * not because it was cross called, but because something
 * "interesting" happened (e.g. a breakpoint).
 *
 * More than one cpu can be here at a time, but only one cpu at
 * a time is allowed to call cmd(), because cmd() calls doswitch(),
 * and only one cpu at a time can be executing on the debugger
 * stack.
 *
 * The static variable "master" specifies which cpu currently holds
 * "kadblock."
 */
void
kadb_master_entry(unsigned this_cpuid)
{
	enum stepstate { nostep, request, acknowledge };
	static volatile enum stepstate stepping = nostep;
	static volatile unsigned master;

	reload_prom_callback();

	if (obtain_lock(&kadblock)) {

		cur_cpuid = master = this_cpuid;
		idle_other_cpus();

	} else if (stepping == acknowledge &&
		    this_cpuid == master) {

		/* assert:  cur_cpuid == this_cpuid */
		stepping = nostep;

	} /* else TBD: clear any pending x-call interrupt */


	while (lock_held(&kadblock)) {
		if (this_cpuid != master) continue;

		if (stepping == request) {
			stepping = acknowledge;
			break;
		}

		cmd();
		if (dotrace) {
			/* N.B.  Order of these two assignments matters */
			stepping = request;
			master = cur_cpuid;
		} else {
			resume_other_cpus();
		}
	}
}


/*
 * Switch the "active" CPU (i.e. the default for step and register
 * ops) to that specified by the user.
 */
void
switch_cpu(int to)
{
	if (cur_cpuid == to ||
	    cpusave[to].cpu_status == CPU_STATUS_INACTIVE) {
		printf("%d is not a valid CPU number\n", to);
		return;
	}
	db_printf(2, "switch_cpu: old %X, new %X", cur_cpuid, to);

	cur_cpuid = to;
	dorun = dotrace = 0;
}


int
canstep(void)
{
	return (cpusave[cur_cpuid].cpu_status == CPU_STATUS_MASTER);
}


#define	LSU_VM_SHIFT	(25)
#define	LSU_PM_SHIFT	(33)

#define	VAWP_BITS	((0xffL << LSU_VM_SHIFT) | LSU_VR | LSU_VW)
#define	PAWP_BITS	((0xffL << LSU_PM_SHIFT) | LSU_PR | LSU_PW)

caddr_t		wp_vaddress;
caddr_t		wp_paddress;
unsigned long	wp_lsucr;


void
wp_vaccess(caddr_t addr, int mask)
{
	wp_vaddress = addr;
	mask = (unsigned char)mask;
	wp_lsucr = (wp_lsucr & ~VAWP_BITS) |
		    (((unsigned long)mask << LSU_VM_SHIFT) | LSU_VR | LSU_VW);
}


void
wp_vread(caddr_t addr, int mask)
{
	wp_vaddress = addr;
	mask = (unsigned char)mask;
	wp_lsucr = (wp_lsucr & ~VAWP_BITS) |
		    (((unsigned long)mask << LSU_VM_SHIFT) | LSU_VR);
}


void
wp_vwrite(caddr_t addr, int mask)
{
	wp_vaddress = addr;
	mask = (unsigned char)mask;
	wp_lsucr = (wp_lsucr & ~VAWP_BITS) |
		    (((unsigned long)mask << LSU_VM_SHIFT) | LSU_VW);
}


void
wp_paccess(caddr_t addr, int mask)
{
	wp_paddress = addr;
	mask = (unsigned char)mask;
	wp_lsucr = (wp_lsucr & ~PAWP_BITS) |
		    (((unsigned long)mask << LSU_PM_SHIFT) | LSU_PR | LSU_PW);
}


void
wp_clrall(void)
{
	wp_lsucr = 0;
}


void
wp_off(caddr_t addr)
{
	uintptr_t a = (uintptr_t)addr & ~0x7;

	if (a == (((uintptr_t)wp_vaddress) & ~0x7))
		wp_lsucr &= ~VAWP_BITS;
	else if (a == (((uintptr_t)wp_paddress) & ~0x7))
		wp_lsucr &= ~PAWP_BITS;
}



u_longlong_t saved_tstate;
u_longlong_t saved_pc, saved_npc;
int saved_tt;
u_longlong_t saved_g1, saved_g2, saved_g3, saved_g4;
u_longlong_t saved_g5, saved_g6, saved_g7;

char kadb_startup_hook[] = " ['] kadb_callback init-debugger-hook ";
char kadb_prom_hook[] = " ['] kadb_callback is debugger-hook ";

/*
 * Format the Forth word which tells the prom how to save state for
 * giving control to us.
 */
char *
format_prom_callback(void)
{
	static const char kadb_defer_word[] =
	    ": kadb_callback "
	    "  %%pc "
	    "  dup f000.0000 ffff.ffff between if drop exit then "
	    "  h# %x  x!"
	    "  %%npc h# %x  x!"
	    "  %%g1 h# %x  x!"
	    "  %%g2 h# %x  x!"
	    "  %%g3 h# %x  x!"
	    "  %%g4 h# %x  x!"
	    "  %%g5 h# %x  x!"
	    "  %%g6 h# %x  x!"
	    "  %%g7 h# %x  x!"
	    "  1 %%tstate h# %x  x!"
	    "  1 %%tt h# %x  l!"
	    "  h# %x   set-pc "
	    "    go "
	    "; ";

	static char prom_str[512];

	(void) sprintf(prom_str, kadb_defer_word,
			&saved_pc, &saved_npc,
				    &saved_g1, &saved_g2, &saved_g3,
			&saved_g4, &saved_g5, &saved_g6, &saved_g7,
			&saved_tstate, &saved_tt,
			trap);

	return (prom_str);
}

/*
 * Inform the PROM of the address to jump to when it takes a breakpoint
 * trap.
 */
void
set_prom_callback(void)
{
	caddr_t str = format_prom_callback();

	prom_interpret(str, 0, 0, 0, 0, 0);
	prom_interpret(kadb_startup_hook, 0, 0, 0, 0, 0);
}

/*
 * For CPR.  Just arm the callback, since the required prom words have
 * already been prom_interpreted in the cpr boot program.
 */
void
arm_prom_callback(void)
{
	prom_interpret(kadb_startup_hook, 0, 0, 0, 0, 0);
}

/*
 * Reload the PROM's idea of "debugger-hook" for this CPU. The PROM
 * reinitializes the hook each time it is used, so we must re-arm it
 * every time a trap is taken.
 */
void
reload_prom_callback()
{
	prom_interpret(kadb_prom_hook, 0, 0, 0, 0, 0);
}

/*
 * Allows cpr to access the words which kadb defines to the prom.
 */
void
callb_format(void *arg)
{
	extern int elf64mode;
	if (elf64mode)
		*((caddr_t *)arg) = (caddr_t)format_prom_callback();
	else
		*((caddr32_t *)arg) = (caddr32_t)format_prom_callback();
}

/*
 * Allows cpr to arm the prom to use the words which kadb defines to the prom.
 */
void
callb_arm(void)
{
	arm_prom_callback();
}

/*
 * Called to notify kadb that a cpu is not accepting cross-traps.
 */
void
callb_cpu_change(int cpuid, kadb_cpu_attr_t what, int arg)
{
	/*
	 * If cpu's nodeid is valid, save and invalidate cpu's nodeid.
	 */

	switch (what) {
	case KADB_CPU_XCALL:
		if (cpuid >= 0 && cpuid < NCPU) {
			if (arg == 0)
				cpusave[cpuid].cpu_status = CPU_STATUS_INACTIVE;
			else
				cpusave[cpuid].cpu_status = CPU_STATUS_RUNNING;
		}
		break;
	default:
		break;
	}
}

/*
 * Called to notify kadb that the kernel has taken over the console and
 * the arg points to the polled input/output functions
 */
void
callb_set_polled_callbacks(cons_polledio_t *polled)
{
	extern int elf64mode;

	/* This is the 32 bit version of cons_polledio_t */
	typedef struct cons_polledio32 {
		uint32_t	cons_polledio_version;
		caddr32_t	cons_polledio_argument;
		caddr32_t	cons_polledio_putchar;
		caddr32_t	cons_polledio_getchar;
		caddr32_t	cons_polledio_ischar;
		caddr32_t	cons_polledio_enter;
		caddr32_t	cons_polledio_exit;
	} cons_polledio32_t;

	cons_polledio32_t polled32;

	/*
	 * If the argument is NULL, then just return
	 */
	if (polled == NULL)
		return;

	/*
	 * kadb runs in 64 bit mode.  If the kernel is also running in
	 * 64 bit, then copy the polled parameter and return.
	 */
	if (elf64mode) {
		bcopy(polled, &polled_io, sizeof (cons_polledio_t));
		return;
	}

	/*
	 * The kernel is running in 32 bit.  Copy the kernel's
	 * parameter into a 32 bit version of the polledio
	 * structure, and then copy the data field by field to
	 * polled_io
	 */

	bcopy(polled, &polled32, sizeof (cons_polledio32_t));

	polled_io.cons_polledio_argument =
		(struct cons_polledio_arg *)polled32.cons_polledio_argument;

	polled_io.cons_polledio_getchar =
		(int (*)(struct cons_polledio_arg *))
		polled32.cons_polledio_getchar;

	polled_io.cons_polledio_ischar =
		(boolean_t (*)(struct cons_polledio_arg *))
		polled32.cons_polledio_ischar;

	polled_io.cons_polledio_enter =
		(void (*)(struct cons_polledio_arg *))
		polled32.cons_polledio_enter;

	polled_io.cons_polledio_exit =
		(void (*)(struct cons_polledio_arg *))
		polled32.cons_polledio_exit;
}

/*
 * Borrowed from autoconf.c in the kernel - check to see if the nodeid's
 * status is valid.
 */
int
status_okay(int id, char *buf, int buflen)
{
	char status_buf[OBP_MAXPROPNAME];
	char *bufp = buf;
	int len = buflen;
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	int fail_len = (int)strlen(fail);

	/*
	 * Get the proplen ... if it's smaller than "fail",
	 * or doesn't exist ... then we don't care, since
	 * the value can't begin with the char string "fail".
	 *
	 * NB: proplen, if it's a string, includes the NULL in the
	 * the size of the property, and fail_len does not.
	 */
	proplen = prom_getproplen((dnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)	/* nonexistant or uninteresting len */
		return (1);

	/*
	 * if a buffer was provided, use it
	 */
	if ((buf == (char *)NULL) || (buflen <= 0)) {
		bufp = status_buf;
		len = sizeof (status_buf);
	}
	*bufp = (char)0;

	/*
	 * Get the property into the buffer, to the extent of the buffer,
	 * and in case the buffer is smaller than the property size,
	 * NULL terminate the buffer. (This handles the case where
	 * a buffer was passed in and the caller wants to print the
	 * value, but the buffer was too small).
	 */
	(void) prom_bounded_getprop((dnode_t)id, (caddr_t)status,
	    (caddr_t)bufp, len);
	*(bufp + len - 1) = (char)0;

	/*
	 * If the value begins with the char string "fail",
	 * then it means the node is failed. We don't care
	 * about any other values. We assume the node is ok
	 * although it might be 'disabled'.
	 */
	if (strncmp(bufp, fail, fail_len) == 0)
		return (0);

	return (1);
}



/*
 * Perform MP initialization. Walk the device tree and save the node IDs
 * of all CPUs in the system.
 */
void
mp_init()
{
	dnode_t nodeid;
	dnode_t sp[OBP_STACKDEPTH];
	pstack_t *stk;
	int upa_id, cpuid, i;
	int clock_freq;


	for (i = 0; i < NCPU; i++)
		cpusave[i].cpu_status = CPU_STATUS_INACTIVE;

	stk = prom_stack_init(sp, sizeof (sp));
	for (nodeid = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
	    nodeid != OBP_NONODE; nodeid = prom_nextnode(nodeid),
	    nodeid = prom_findnode_bydevtype(nodeid, "cpu", stk)) {
		if (prom_getprop(nodeid, "upa-portid",
		    (caddr_t)&upa_id) == -1) {
			prom_printf("cpu node %x without upa-portid prop\n",
			    nodeid);
			continue;
		}

		/*
		 * XXX - the following assumes the values are the same for
		 * all CPUs.
		 */
		if (prom_getprop(nodeid, "dcache-size",
		    (caddr_t)&vac_size) == -1) {
			prom_printf("can't get dcache-size for cpu 0x%x\n",
			    nodeid);
			vac_size = 2 * MMU_PAGESIZE;
		}
		if (prom_getprop(nodeid, "clock-frequency",
		    (caddr_t)&clock_freq) == -1) {
			prom_printf("can't get clock-freq for cpu 0x%x\n",
			    nodeid);
			clock_freq = 167;
		}
		Cpudelay = ((clock_freq + 500000) / 1000000) - 3;
		cpuid = UPAID_TO_CPUID(upa_id);
		if (!status_okay(nodeid, (char *)NULL, 0)) {
			prom_printf("kadb: bad status for cpu node %x\n",
			    cpuid);
			continue;
		}

		cpusave[cpuid].cpu_status = CPU_STATUS_RUNNING;
	}
	prom_stack_fini(stk);
}


/*
 * Construct an informative error message
 */
static
void
regerr(int reg, char *op)
{
	static char rw_invalid[ 60 ];

	if (reg < 0 || reg > NREGISTERS) {
		sprintf(rw_invalid, "Can't %s register #%d", op, reg);
	} else {
		sprintf(rw_invalid, "Can't %s register %s (%d)",
						op, regnames[reg], reg);
	}
	errflg = rw_invalid;
}


/*
 * readsavedreg -- retrieve value of register reg from a saved call
 *    frame.  The register must be one of those saved by a "save"
 *    instruction, i.e. Reg_L0 <= reg <= Reg_I7
 */
long
readsavedreg(struct stackpos *pos, int reg)
{
	static char reginvalid[60];
	struct allregs_v9 *regs = &cpusave[cur_cpuid].cpu_regs;
	unsigned cwp;
	unsigned long val;


	if (reg < Reg_L0 || reg > Reg_I7) {
	    sprintf(reginvalid, "Invalid window register number %d", reg);
	    errflg = reginvalid;
	    return (0);
	}

	/*
	 * For kadb, some stack windows are saved in cpusave, and
	 * others are on the stack.
	 */
	if ((pos->k_flags & K_ONSTACK) ||
	    pos->k_level > regs->r_canrestore) {
		return (readstackreg(pos, reg));
	}

	cwp = (regs->r_cwp - pos->k_level + nwindows) % nwindows;

	if (reg >= Reg_I0)
		val = (regs->r_window[cwp].rw_in[reg - Reg_I0]);
	else
		val = (regs->r_window[cwp].rw_local[reg - Reg_L0]);


	db_printf(1, "readsavedreg:  fp=%J cwp=%u reg=%d",
			pos->k_fp, cwp, reg);


	if (IS_V9STACK(pos->k_fp))
		return (val);
	else
		return ((uint32_t)val);
}


/*
 * readreg -- retrieve value of register reg from saved state.
 */
long
readreg(int reg)
{
	struct allregs_v9 *regs = &cpusave[cur_cpuid].cpu_regs;
	v9_fpregset_t *fregs = &cpusave[cur_cpuid].cpu_fpregs;

	db_printf(1, "readreg:  reg %d", reg);

	switch (reg) {
	case Reg_G0:
		return (0);

	case Reg_G1:
	case Reg_G2:
	case Reg_G3:
	case Reg_G4:
	case Reg_G5:
	case Reg_G6:
	case Reg_G7:
		return (regs->r_globals[reg - Reg_G1]);

	case Reg_O0:
	case Reg_O1:
	case Reg_O2:
	case Reg_O3:
	case Reg_O4:
	case Reg_O5:
	case Reg_O6:
	case Reg_O7: {
		unsigned cwp = (regs->r_cwp + 1) % nwindows;
		return (regs->r_window[cwp].rw_in[reg - Reg_O0]);
	}

	case Reg_L0:
	case Reg_L1:
	case Reg_L2:
	case Reg_L3:
	case Reg_L4:
	case Reg_L5:
	case Reg_L6:
	case Reg_L7: {
		unsigned cwp = regs->r_cwp;
		return (regs->r_window[cwp].rw_local[reg - Reg_L0]);
	}

	case Reg_I0:
	case Reg_I1:
	case Reg_I2:
	case Reg_I3:
	case Reg_I4:
	case Reg_I5:
	case Reg_I6:
	case Reg_I7: {
		unsigned cwp = regs->r_cwp;
		return (regs->r_window[cwp].rw_in[reg - Reg_I0]);
	}

	case Reg_Y:
		return (regs->r_y);

	case Reg_TSTATE:
		return (regs->r_tstate);

	case Reg_TBA:
		return (regs->r_tba);

	case Reg_PC:
		return (regs->r_pc);
	case Reg_NPC:
		return (regs->r_npc);

	case Reg_CWP:
		return (regs->r_cwp);
	case Reg_OTHERWIN:
		return (regs->r_otherwin);
	case Reg_CLEANWIN:
		return (regs->r_cleanwin);
	case Reg_CANSAVE:
		return (regs->r_cansave);
	case Reg_CANRESTORE:
		return (regs->r_canrestore);
	case Reg_WSTATE:
		return (regs->r_wstate);
	case Reg_TT:
		return (regs->r_tt);
	case Reg_PIL:
		return (regs->r_pil);

	case Reg_FSR:
		if (!(fregs->fpu_fprs & FPRS_FEF)) {
			regerr(reg, "read");
			return (0);
		}
		return (fregs->fpu_fsr);

	case Reg_FQ:
		/* no such beastie in Ultra-Sparc */
		regerr(reg, "read");
		return (0);

	default:
		if (reg < Reg_F0 || reg > Reg_F63 ||
			    !(fregs->fpu_fprs & FPRS_FEF)) {
			regerr(reg, "read");
			return (0);
		}
		return (fregs->fpu_fr.fpu_regs[reg - Reg_F0]);
	}
}


/*
 * writereg -- store value of register reg in saved state.
 */
int
writereg(int reg, long val)
{
	struct allregs_v9 *regs = &cpusave[cur_cpuid].cpu_regs;
	v9_fpregset_t *fregs = &cpusave[cur_cpuid].cpu_fpregs;

	db_printf(1, "writereg:  reg %d", reg);

	switch (reg) {
	case Reg_G0:
		regerr(reg, "write");
		return (0);

	case Reg_G1:
	case Reg_G2:
	case Reg_G3:
	case Reg_G4:
	case Reg_G5:
	case Reg_G6:
	case Reg_G7:
		regs->r_globals[reg - Reg_G1] = val;
		break;

	case Reg_O0:
	case Reg_O1:
	case Reg_O2:
	case Reg_O3:
	case Reg_O4:
	case Reg_O5:
	case Reg_O6:
	case Reg_O7: {
		unsigned cwp = (regs->r_cwp + 1) % nwindows;
		regs->r_window[cwp].rw_in[reg - Reg_O0] = val;
		break;
	}

	case Reg_L0:
	case Reg_L1:
	case Reg_L2:
	case Reg_L3:
	case Reg_L4:
	case Reg_L5:
	case Reg_L6:
	case Reg_L7: {
		unsigned cwp = regs->r_cwp;
		regs->r_window[cwp].rw_local[reg - Reg_L0] = val;
		break;
	}

	case Reg_I0:
	case Reg_I1:
	case Reg_I2:
	case Reg_I3:
	case Reg_I4:
	case Reg_I5:
	case Reg_I6:
	case Reg_I7: {
		unsigned cwp = regs->r_cwp;
		regs->r_window[cwp].rw_in[reg - Reg_I0] = val;
		break;
	}

	case Reg_Y:
		regs->r_y = val;
		break;

	case Reg_TSTATE:
		regs->r_tstate = val;
		break;

	case Reg_TBA:
		regs->r_tba = val;
		break;

	case Reg_PC:
		userpc = regs->r_pc = val;
		break;
	case Reg_NPC:
		regs->r_npc = val;
		break;

	case Reg_CWP:
		regs->r_cwp = val;
		break;
	case Reg_OTHERWIN:
		regs->r_otherwin = val;
		break;
	case Reg_CLEANWIN:
		regs->r_cleanwin = val;
		break;
	case Reg_CANSAVE:
		regs->r_cansave = val;
		break;
	case Reg_CANRESTORE:
		regs->r_canrestore = val;
		break;
	case Reg_WSTATE:
		regs->r_wstate = val;
		break;

	case Reg_FSR:
		if (!(fregs->fpu_fprs & FPRS_FEF)) {
			regerr(reg, "write");
			return (0);
		}
		fregs->fpu_fsr = val;
		break;

	case Reg_FQ:
		/* no such beastie in Ultra-Sparc */
		regerr(reg, "write");
		return (0);

	default:
		if (reg < Reg_F0 || reg > Reg_F63 ||
			    !(fregs->fpu_fprs & FPRS_FEF)) {
			regerr(reg, "write");
			return (0);
		}
		fregs->fpu_fr.fpu_regs[reg - Reg_F0] = val;
		break;
	}

	return (sizeof (int));
}

/*
 *  setreg -- pass through to writereg()
 *
 *  We need this function because it's called in getpcb().
 *  getpcb() needs setreg() to be different from writereg(),
 *  or "adb -k" gets broken.  Why this should be, I don't know.
 *  "Ours is not to reason why..."
 */
void
setreg(int reg, long val)
{
	(void) writereg(reg, val);
}
