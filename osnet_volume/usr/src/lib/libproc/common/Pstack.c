/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Pstack.c	1.3	99/09/06 SMI"

/*
 * Pstack.c - Code to iterate backward through the set of previous stack frames
 * associated with a register set and invoke an iterator function at each step.
 * This process is relatively straightforward, but is complicated slightly by
 * the need for special handling for gwindows information (on SPARC only), and
 * signal handler frames (on all architectures).
 */

#ifdef __sparcv9
#define	__sparcv9cpu
#endif

#include <sys/stack.h>
#include <sys/regset.h>
#include <sys/frame.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "libproc.h"
#include "Pcontrol.h"
#include "P32ton.h"

/*
 * Utility function to prevent stack loops from running on forever by
 * detecting when there is a stack loop (the %fp has been seen before).
 */
static int
stack_loop(prgreg_t fp, prgreg_t **prevfpp, int *nfpp, uint_t *pfpsizep)
{
	prgreg_t *prevfp = *prevfpp;
	uint_t pfpsize = *pfpsizep;
	int nfp = *nfpp;
	int i;

	for (i = 0; i < nfp; i++) {
		if (fp == prevfp[i])
			return (1); /* stack loop detected */
	}

	if (nfp == pfpsize) {
		pfpsize = pfpsize ? pfpsize * 2 : 16;
		prevfp = realloc(prevfp, pfpsize * sizeof (prgreg_t));
		/*
		 * Just assume there is no loop in the face of allocation
		 * failure; the caller still has the original prevfp pointer.
		 */
		if (prevfp == NULL)
			return (0);
	}

	prevfp[nfp++] = fp;
	*prevfpp = prevfp;
	*pfpsizep = pfpsize;
	*nfpp = nfp;

	return (0);
}

/*
 * Signal Frame Detection
 *
 * In order to facilitate detection and processing of signal handler frames
 * during a stack backtrace, we define a set of utility routines to operate on
 * a uclist (ucontext address list), and then use these routines in the various
 * implementations of Pstack_iter below.  Certain source-level debuggers and
 * virtual machines that shall remain nameless believe that in order to detect
 * signal handler frames, one must hard-code checks for symbol names defined
 * in libc and libthread and knowledge of their implementation.  We make no
 * such assumptions, allowing us to operate on programs that manipulate their
 * underlying kernel signal handlers (i.e. use __sigaction) and to not require
 * changes in the face of future library modifications.
 *
 * A signal handler frame is essentially a set of data pushed on to the user
 * stack by the kernel prior to returning to the user program in one of the
 * pre-defined signal handlers.  The signal handler itself receives the signal
 * number, an optional pointer to a siginfo_t, and a pointer to the interrupted
 * ucontext as arguments.  When performing a stack backtrace, we would like to
 * detect these frames so that we can correctly return the interrupted program
 * counter and frame pointer as a separate frame.  When a signal handler frame
 * is constructed on the stack by the kernel, the signalled LWP has its
 * lwp_oldcontext member (exported through /proc as lwpstatus.pr_oldcontext)
 * set to the user address at which the ucontext_t was placed on the LWP's
 * stack.  The ucontext_t's uc_link member is set to the previous value of
 * lwp_oldcontext.  Thus when signal handlers are active, pr_oldcontext will
 * point to the first element of a linked list of ucontext_t addresses.
 *
 * The stack layout for a signal handler frame is as follows:
 *
 * SPARC v7/v9:                           Intel IA32:
 * +--------------+ -        high         +--------------+ -
 * |  struct fq   | ^        addrs        |  ucontext_t  | mandatory
 * +--------------+ |          ^          +--------------+ -
 * |  gwindows_t  |            |          |  siginfo_t   | optional
 * +--------------+ optional              +--------------+ -
 * |  siginfo_t   |                       | ucontext_t * | ^
 * +--------------+ |          |          +--------------+ |
 * |  xregs data  | v          v          |  siginfo_t * |
 * +--------------+ -         low         +--------------+ mandatory
 * |  ucontext_t  | ^        addrs        |  int (signo) |
 * +--------------+ mandatory             +--------------+ |
 * | struct frame | v                     | struct frame | v
 * +--------------+ - <- %sp on resume    +--------------+ - <- %esp on resume
 *
 * The bottom-most struct frame is actually constructed by the kernel by
 * copying the previous stack frame, allowing naive backtrace code to simply
 * skip over the interrupted frame.  The copied frame is never really used,
 * since it is presumed the libc or libthread signal handler wrapper function
 * will explicitly setcontext(2) to the interrupted context if the user
 * program's handler returns.  If we detect a signal handler frame, we simply
 * read the interrupted context structure from the stack, use its embedded
 * gregs to construct the register set for the interrupted frame, and then
 * continue our backtrace.  Detecting the frame itself is easy according to
 * the diagram ("oldcontext" represents any element in the uc_link chain):
 *
 * On SPARC v7 or v9:
 * %fp + sizeof (struct frame) == oldcontext
 *
 * On Intel IA32:
 * %ebp + sizeof (struct frame) + (3 words) == oldcontext
 * %ebp + sizeof (struct frame) + (3 words) + sizeof (siginfo_t) == oldcontext
 *
 * A final complication is that we want libproc to support backtraces from
 * arbitrary addresses without the caller passing in an LWP id.  To do this,
 * we must first determine all the known oldcontexts by iterating over all
 * LWPs and following their pr_oldcontext pointers.  We optimize our search
 * by discarding NULL pointers and pointers whose value is less than that
 * of the initial stack pointer (since stacks grow down from high memory),
 * and then sort the resulting list by virtual address so we can binary search.
 */

typedef struct {
	struct ps_prochandle *uc_proc;	/* libproc handle */
	uintptr_t uc_start;		/* starting stack address */
	uintptr_t *uc_addrs;		/* array of stack addresses */
	uint_t uc_nelems;		/* number of valid elements */
	uint_t uc_size;			/* actual size of array */
} uclist_t;

static int
load_uclist(uclist_t *ucl, const lwpstatus_t *psp)
{
	struct ps_prochandle *P = ucl->uc_proc;
	uintptr_t addr = psp->pr_oldcontext;

	uintptr_t *new_addrs;
	uint_t new_size;
	ucontext_t uc;

	/*
	 * Follow the uc_link chain beginning at pr_oldcontext until we reach
	 * NULL or an address below uc_start (stacks grow down from high addrs).
	 */
	while (addr != NULL && addr > ucl->uc_start) {
		if (ucl->uc_nelems == ucl->uc_size) {
			new_size = ucl->uc_size ? ucl->uc_size * 2 : 16;
			new_addrs = realloc(ucl->uc_addrs,
			    new_size * sizeof (uintptr_t));

			if (new_addrs != NULL) {
				ucl->uc_addrs = new_addrs;
				ucl->uc_size = new_size;
			} else
				break; /* abort if allocation failure */
		}
#ifdef _LP64
		if (P->status.pr_dmodel == PR_MODEL_ILP32) {
			ucontext32_t u32;

			if (Pread(P, &u32, sizeof (u32), addr) != sizeof (u32))
				break; /* abort if we fail to read ucontext */
			uc.uc_link = (ucontext_t *)u32.uc_link;
		} else
#endif
		if (Pread(P, &uc, sizeof (uc), addr) != sizeof (uc))
			break; /* abort if we fail to read ucontext */

		dprintf("detected lwp %d signal context at %p\n",
		    (int)psp->pr_lwpid, (void *)addr);
		ucl->uc_addrs[ucl->uc_nelems++] = addr;
		addr = (uintptr_t)uc.uc_link;
	}

	return (0);
}

static int
sort_uclist(const void *lhp, const void *rhp)
{
	uintptr_t lhs = *((const uintptr_t *)lhp);
	uintptr_t rhs = *((const uintptr_t *)rhp);

	if (lhs < rhs)
		return (-1);
	if (lhs > rhs)
		return (+1);
	return (0);
}

static void
init_uclist(uclist_t *ucl, struct ps_prochandle *P, uintptr_t start)
{
	ucl->uc_proc = P;
	ucl->uc_start = start;
	ucl->uc_addrs = NULL;
	ucl->uc_nelems = 0;
	ucl->uc_size = 0;

	(void) Plwp_iter(P, (proc_lwp_f *)load_uclist, ucl);
	qsort(ucl->uc_addrs, ucl->uc_nelems, sizeof (uintptr_t), sort_uclist);
}

static void
free_uclist(uclist_t *ucl)
{
	if (ucl->uc_addrs != NULL)
		free(ucl->uc_addrs);
}

static int
find_uclink(uclist_t *ucl, uintptr_t addr)
{
	if (ucl->uc_nelems != 0) {
		return (bsearch(&addr, ucl->uc_addrs, ucl->uc_nelems,
		    sizeof (uintptr_t), sort_uclist) != NULL);
	}

	return (0);
}

#if defined(sparc) || defined(__sparc)
/*
 * For gwindows_t support, we define a structure to pass arguments to
 * a Plwp_iter() callback routine.
 */
typedef struct {
	struct ps_prochandle *gq_proc;	/* libproc handle */
	struct rwindow *gq_rwin;	/* rwindow destination buffer */
	uintptr_t gq_addr;		/* stack address to match */
} gwin_query_t;

static int
find_gwin(gwin_query_t *gqp, const lwpstatus_t *psp)
{
	gwindows_t gwin;
	struct stat64 st;
	char path[64];
	ssize_t n;
	int fd, i;
	int rv = 0; /* Return value for skip to next lwp */

	(void) snprintf(path, sizeof (path), "/proc/%d/lwp/%d/gwindows",
	    (int)gqp->gq_proc->pid, (int)psp->pr_lwpid);

	if (stat64(path, &st) == -1 || st.st_size == 0)
		return (0); /* Nothing doing; skip to next lwp */

	if ((fd = open64(path, O_RDONLY)) >= 0) {
		/*
		 * Zero out the gwindows_t because the gwindows file only has
		 * as much data as needed to represent the saved windows.
		 */
#ifdef _LP64
		if (gqp->gq_proc->status.pr_dmodel == PR_MODEL_ILP32) {
			gwindows32_t g32;

			(void) memset(&g32, 0, sizeof (g32));
			if ((n = read(fd, &g32, sizeof (g32))) > 0)
				gwindows_32_to_n(&g32, &gwin);

		} else {
#endif
			(void) memset(&gwin, 0, sizeof (gwin));
			n = read(fd, &gwin, sizeof (gwin));
#ifdef _LP64
		}
#endif
		if (n > 0) {
			/*
			 * If we actually found a non-zero gwindows file and
			 * were able to read it, iterate through the buffers
			 * looking for a stack pointer match; if one is found,
			 * copy out the corresponding register window.
			 */
			for (i = 0; i < gwin.wbcnt; i++) {
				if (gwin.spbuf[i] == (greg_t *)gqp->gq_addr) {
					(void) memcpy(gqp->gq_rwin,
					    &gwin.wbuf[i],
					    sizeof (struct rwindow));

					rv = 1; /* We're done */
					break;
				}
			}
		}
		(void) close(fd);
	}

	return (rv);
}

static int
read_gwin(struct ps_prochandle *P, struct rwindow *rwp, uintptr_t sp)
{
	gwin_query_t gq;

	if (P->state == PS_DEAD) {
		lwp_info_t *lwp = list_next(&P->core->core_lwp_head);
		uint_t n;
		int i;

		for (n = 0; n < P->core->core_nlwp; n++, lwp = list_next(lwp)) {
			gwindows_t *gwin = lwp->lwp_gwins;

			if (gwin == NULL)
				continue; /* No gwindows for this lwp */

			/*
			 * If this lwp has gwindows associated with it, iterate
			 * through the buffers looking for a stack pointer
			 * match; if one is found, copy out the register window.
			 */
			for (i = 0; i < gwin->wbcnt; i++) {
				if (gwin->spbuf[i] == (greg_t *)sp) {
					(void) memcpy(rwp, &gwin->wbuf[i],
					    sizeof (struct rwindow));
					return (0); /* We're done */
				}
			}
		}

		return (-1); /* No gwindows match found */

	}

	gq.gq_proc = P;
	gq.gq_rwin = rwp;
	gq.gq_addr = sp;

	return (Plwp_iter(P, (proc_lwp_f *)find_gwin, &gq) ? 0 : -1);
}

static void
ucontext_n_to_prgregs(const ucontext_t *src, prgregset_t dst)
{
	const greg_t *gregs = &src->uc_mcontext.gregs[0];

#ifdef __sparcv9
	dst[R_CCR] = gregs[REG_CCR];
	dst[R_ASI] = gregs[REG_ASI];
	dst[R_FPRS] = gregs[REG_FPRS];
#else
	dst[R_PSR] = gregs[REG_PSR];
#endif
	dst[R_PC] = gregs[REG_PC];
	dst[R_nPC] = gregs[REG_nPC];
	dst[R_Y] = gregs[REG_Y];

	dst[R_G1] = gregs[REG_G1];
	dst[R_G2] = gregs[REG_G2];
	dst[R_G3] = gregs[REG_G3];
	dst[R_G4] = gregs[REG_G4];
	dst[R_G5] = gregs[REG_G5];
	dst[R_G6] = gregs[REG_G6];
	dst[R_G7] = gregs[REG_G7];

	dst[R_O0] = gregs[REG_O0];
	dst[R_O1] = gregs[REG_O1];
	dst[R_O2] = gregs[REG_O2];
	dst[R_O3] = gregs[REG_O3];
	dst[R_O4] = gregs[REG_O4];
	dst[R_O5] = gregs[REG_O5];
	dst[R_O6] = gregs[REG_O6];
	dst[R_O7] = gregs[REG_O7];
}

#ifdef _LP64
static void
ucontext_32_to_prgregs(const ucontext32_t *src, prgregset_t dst)
{
	/*
	 * We need to be very careful here to cast the greg32_t's (signed) to
	 * unsigned and then explicitly promote them as unsigned values.
	 */
	const greg32_t *gregs = &src->uc_mcontext.gregs[0];

	dst[R_PSR] = (uint64_t)(uint32_t)gregs[REG_PSR];
	dst[R_PC] = (uint64_t)(uint32_t)gregs[REG_PC];
	dst[R_nPC] = (uint64_t)(uint32_t)gregs[REG_nPC];
	dst[R_Y] = (uint64_t)(uint32_t)gregs[REG_Y];

	dst[R_G1] = (uint64_t)(uint32_t)gregs[REG_G1];
	dst[R_G2] = (uint64_t)(uint32_t)gregs[REG_G2];
	dst[R_G3] = (uint64_t)(uint32_t)gregs[REG_G3];
	dst[R_G4] = (uint64_t)(uint32_t)gregs[REG_G4];
	dst[R_G5] = (uint64_t)(uint32_t)gregs[REG_G5];
	dst[R_G6] = (uint64_t)(uint32_t)gregs[REG_G6];
	dst[R_G7] = (uint64_t)(uint32_t)gregs[REG_G7];

	dst[R_O0] = (uint64_t)(uint32_t)gregs[REG_O0];
	dst[R_O1] = (uint64_t)(uint32_t)gregs[REG_O1];
	dst[R_O2] = (uint64_t)(uint32_t)gregs[REG_O2];
	dst[R_O3] = (uint64_t)(uint32_t)gregs[REG_O3];
	dst[R_O4] = (uint64_t)(uint32_t)gregs[REG_O4];
	dst[R_O5] = (uint64_t)(uint32_t)gregs[REG_O5];
	dst[R_O6] = (uint64_t)(uint32_t)gregs[REG_O6];
	dst[R_O7] = (uint64_t)(uint32_t)gregs[REG_O7];
}
#endif	/* _LP64 */

int
Pstack_iter(struct ps_prochandle *P, const prgregset_t regs,
	proc_stack_f *func, void *arg)
{
	prgreg_t *prevfp = NULL;
	uint_t pfpsize = 0;
	int nfp = 0;
	prgregset_t gregs;
	long args[6];
	prgreg_t fp;
	int i;
	int rv;
	uintptr_t sp;
	ssize_t n;
	uclist_t ucl;
	ucontext_t uc;

	init_uclist(&ucl, P, regs[R_FP]);
	(void) memcpy(gregs, regs, sizeof (gregs));

	for (;;) {
		fp = gregs[R_FP];
		if (stack_loop(fp, &prevfp, &nfp, &pfpsize))
			break;

		for (i = 0; i < 6; i++)
			args[i] = gregs[R_I0 + i];
		if ((rv = func(arg, gregs, 6, args)) != 0)
			break;

		gregs[R_PC] = gregs[R_I7];
		gregs[R_nPC] = gregs[R_PC] + 4;
		(void) memcpy(&gregs[R_O0], &gregs[R_I0], 8*sizeof (prgreg_t));
		if ((sp = gregs[R_FP]) == 0)
			break;

#ifdef _LP64
		if (P->status.pr_dmodel == PR_MODEL_ILP32) {
			struct rwindow32 rw32;
			ucontext32_t uc32;

			if (find_uclink(&ucl, sp +
			    SA32(sizeof (struct frame32))) &&
			    Pread(P, &uc32, sizeof (uc32), sp +
			    SA32(sizeof (struct frame32))) == sizeof (uc32)) {
				ucontext_32_to_prgregs(&uc32, gregs);
				sp = gregs[R_SP];
			}

			n = Pread(P, &rw32, sizeof (struct rwindow32), sp);

			if (n == sizeof (struct rwindow32)) {
				rwindow_32_to_n(&rw32,
				    (struct rwindow *)&gregs[R_L0]);
				continue;
			}

		} else {
#endif	/* _LP64 */
			sp += STACK_BIAS;

			if (find_uclink(&ucl, sp + SA(sizeof (struct frame))) &&
			    Pread(P, &uc, sizeof (uc), sp +
			    SA(sizeof (struct frame))) == sizeof (uc)) {
				ucontext_n_to_prgregs(&uc, gregs);
				sp = gregs[R_SP] + STACK_BIAS;
			}

			n = Pread(P, &gregs[R_L0], sizeof (struct rwindow), sp);

			if (n == sizeof (struct rwindow))
				continue;
#ifdef _LP64
		}
#endif	/* _LP64 */

		/*
		 * If we get here, then our Pread of the register window
		 * failed.  If this is because the address was not mapped,
		 * then we attempt to read this window via any gwindows
		 * information we have.  If that too fails, abort our loop.
		 */
		if (n > 0)
			break;	/* Failed for reason other than not mapped */

		if (read_gwin(P, (struct rwindow *)&gregs[R_L0], sp) == -1)
			break;	/* No gwindows match either */
	}

	if (prevfp)
		free(prevfp);

	free_uclist(&ucl);
	return (rv);
}
#endif	/* sparc */

#if defined(i386) || defined(__i386)

/*
 * Given the return PC, return the number of arguments.
 * (A bit of disassembly of the instruction is required here.)
 */
static ulong_t
argcount(struct ps_prochandle *P, long pc, ssize_t sz)
{
	uchar_t instr[6];
	ulong_t count;

	/*
	 * Read the instruction at the return location.
	 */
	if (Pread(P, instr, sizeof (instr), pc) != sizeof (instr) ||
	    instr[1] != 0xc4)
		return (0);

	switch (instr[0]) {
	case 0x81:	/* count is a longword */
		count = instr[2]+(instr[3]<<8)+(instr[4]<<16)+(instr[5]<<24);
		break;
	case 0x83:	/* count is a byte */
		count = instr[2];
		break;
	default:
		count = 0;
		break;
	}

	if (count > sz)
		count = sz;
	return (count / sizeof (long));
}

static void
ucontext_n_to_prgregs(const ucontext_t *src, prgregset_t dst)
{
	(void) memcpy(dst, src->uc_mcontext.gregs, sizeof (gregset_t));
}

int
Pstack_iter(struct ps_prochandle *P, const prgregset_t regs,
	proc_stack_f *func, void *arg)
{
	prgreg_t *prevfp = NULL;
	uint_t pfpsize = 0;
	int nfp = 0;
	struct {
		long	fp;
		long	pc;
		long	args[32];
	} frame;
	uint_t argc;
	ssize_t sz;
	prgregset_t gregs;
	prgreg_t fp, pfp;
	prgreg_t pc;
	int rv;

	/*
	 * Type definition for a structure corresponding to an IA32
	 * signal frame.  Refer to the comments above for more info.
	 */
	typedef struct {
		long fp;
		long pc;
		int signo;
		siginfo_t *sip;
		ucontext_t *ucp;
	} sf_t;

	uclist_t ucl;
	ucontext_t uc;
	uintptr_t uc_addr;

	init_uclist(&ucl, P, regs[R_FP]);
	(void) memcpy(gregs, regs, sizeof (gregs));

	fp = regs[R_FP];
	pc = regs[R_PC];

	while (fp != 0 || pc != 0) {
		if (stack_loop(fp, &prevfp, &nfp, &pfpsize))
			break;

		if (fp != 0 &&
		    (sz = Pread(P, &frame, sizeof (frame), (uintptr_t)fp)
		    >= (ssize_t)(2* sizeof (long)))) {
			/*
			 * One more trick for signal frames: the kernel sets
			 * the return pc of the signal frame to 0xffffffff on
			 * Intel IA32, so argcount won't work.
			 */
			if (frame.pc != -1L) {
				sz -= 2* sizeof (long);
				argc = argcount(P, (long)frame.pc, sz);
			} else
				argc = 3; /* sighandler(signo, sip, ucp) */
		} else {
			(void) memset(&frame, 0, sizeof (frame));
			argc = 0;
		}

		gregs[R_FP] = fp;
		gregs[R_PC] = pc;

		if ((rv = func(arg, gregs, argc, frame.args)) != 0)
			break;

		pfp = fp;
		fp = frame.fp;
		pc = frame.pc;

		/*
		 * We have to check at two possible locations for a signal
		 * handler's saved ucontext_t.  See comments above.
		 */
		if (find_uclink(&ucl, pfp + sizeof (sf_t) + sizeof (siginfo_t)))
			uc_addr = pfp + sizeof (sf_t) + sizeof (siginfo_t);
		else if (find_uclink(&ucl, pfp + sizeof (sf_t)))
			uc_addr = pfp + sizeof (sf_t);
		else
			uc_addr = NULL;

		if (uc_addr != NULL &&
		    Pread(P, &uc, sizeof (uc), uc_addr) == sizeof (uc)) {
			ucontext_n_to_prgregs(&uc, gregs);
			fp = gregs[R_FP];
			pc = gregs[R_PC];
		}
	}

	if (prevfp)
		free(prevfp);

	free_uclist(&ucl);
	return (rv);
}
#endif	/* i386 */
