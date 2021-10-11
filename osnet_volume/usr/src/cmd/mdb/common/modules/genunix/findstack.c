/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)findstack.c	1.1	99/08/11 SMI"

#include <mdb/mdb_modapi.h>

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/stack.h>
#include <sys/thread.h>

#include "findstack.h"

#ifndef STACK_BIAS
#define	STACK_BIAS	0
#endif

#define	fs_dprintf(x)					\
	if (findstack_debug_on) {			\
		mdb_printf("findstack debug: ");	\
		/*CSTYLED*/				\
		mdb_printf x ;				\
	}

static int findstack_debug_on = 0;

#ifdef __i386
struct rwindow {
	uintptr_t rw_fp;
	uintptr_t rw_pc;
};
#endif

#define	TOO_BIG_FOR_A_STACK (1024 * 1024)

#define	KTOU(p) ((p) - kbase + ubase)
#define	UTOK(p) ((p) - ubase + kbase)

#ifdef __i386
static GElf_Sym thread_exit_sym;
#endif

#define	CRAWL_FOUNDALL	(-1)

/*
 * Given a stack pointer, try to crawl down it to the bottom.
 * "frame" is a VA in MDB's address space.
 *
 * Returns the number of frames successfully crawled down, or
 * CRAWL_FOUNDALL if it got to the bottom of the stack.
 */
static int
crawl(uintptr_t frame, uintptr_t kbase, uintptr_t ktop, uintptr_t ubase,
    int kill_fp)
{
	int levels = 0;

	fs_dprintf(("<0> frame = %p, kbase = %p, ktop = %p, ubase = %p\n",
	    frame, kbase, ktop, ubase));
	for (;;) {
		uintptr_t fp;
		long *fpp = (long *)&((struct rwindow *)frame)->rw_fp;

		fs_dprintf(("<1> fpp = %p, frame = %p\n", fpp, frame));

		if ((frame & (STACK_ALIGN - 1)) != 0)
			break;

		fp = ((struct rwindow *)frame)->rw_fp + STACK_BIAS;
		fs_dprintf(("<2> fp = %p\n", fp));

		if (fp == ktop)
			return (CRAWL_FOUNDALL);
		fs_dprintf(("<3> not at base\n"));

#ifdef __i386
		/*
		 * Terribly B-team: I haven't investigated why this works,
		 * but it makes a huge difference.
		 */
		if ((ktop - fp == 0x10) && ((*(ulong_t *)KTOU(fp + 4)) ==
		    thread_exit_sym.st_value)) {
			fs_dprintf(("<4> found thread_exit\n"));
			return (CRAWL_FOUNDALL);
		}
#endif

		fs_dprintf(("<5> fp = %p, kbase = %p, ktop - size = %p\n",
		    fp, kbase, ktop - sizeof (struct rwindow)));

		if (fp < kbase || fp >= (ktop - sizeof (struct rwindow)))
			break;

		frame = KTOU(fp);
		fs_dprintf(("<6> frame = %p\n", frame));

		/*
		 * NULL out the old %fp so we don't go down this stack
		 * more than once.
		 */
		if (kill_fp) {
			fs_dprintf(("<7> fpp = %p\n", fpp));
			*fpp = NULL;
		}

		fs_dprintf(("<8> levels = %d\n", levels));
		levels++;
	}

	return (levels);
}

/*
 * "sp" is a kernel VA.
 */
static void
print_stack(uintptr_t sp, uintptr_t pc, uintptr_t addr,
    int argc, const mdb_arg_t *argv)
{
	mdb_printf("stack pointer for thread %p: %p\n", addr, sp);
	if (pc != 0 && argc == 0)
		mdb_printf("[ %0?lr %a() ]\n", sp, pc);

	mdb_inc_indent(2);
	mdb_set_dot(sp);
	if (argc != 0)
		mdb_eval(argv->a_un.a_str);
	else
		mdb_eval("<.$C0");
	mdb_dec_indent(2);
}

/*ARGSUSED*/
int
findstack(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	kthread_t thr;
	size_t stksz;
	uintptr_t ubase, utop;
	uintptr_t kbase, ktop;
	uintptr_t win, sp;

	if (!(flags & DCMD_ADDRSPEC) || argc > 1)
		return (DCMD_USAGE);

	if (argc != 0 && argv->a_type != MDB_TYPE_STRING)
		return (DCMD_USAGE);

	if (mdb_vread(&thr, sizeof (thr), addr) == -1) {
		mdb_warn("couldn't read thread at %p\n", addr);
		return (DCMD_ERR);
	}

	if ((thr.t_schedflag & TS_LOAD) == 0) {
		mdb_warn("thread %p isn't in memory\n", addr);
		return (DCMD_ERR);
	}

	if (thr.t_stk < thr.t_stkbase) {
		mdb_warn("stack base or stack top corrupt for thread %p\n",
		    addr);
		return (DCMD_ERR);
	}

	kbase = (uintptr_t)thr.t_stkbase;
	ktop = (uintptr_t)thr.t_stk;
	stksz = ktop - kbase;

	/*
	 * If the stack size is larger than a meg, assume that it's bogus.
	 */
	if (stksz > TOO_BIG_FOR_A_STACK) {
		mdb_warn("stack size for thread %p is too big to be "
		    "reasonable\n", addr);
		return (DCMD_ERR);
	}

	ubase = (uintptr_t)mdb_alloc(stksz, UM_SLEEP | UM_GC);
	utop = ubase + stksz;
	if (mdb_vread((caddr_t)ubase, stksz, kbase) != stksz) {
		mdb_warn("couldn't read entire stack for thread %p\n", addr);
		return (DCMD_ERR);
	}

	/*
	 * Try the saved %sp first, if it looks reasonable.
	 */
	sp = KTOU((uintptr_t)thr.t_sp + STACK_BIAS);
	if (sp >= ubase && sp <= utop) {
		if (crawl(sp, kbase, ktop, ubase, 0) == CRAWL_FOUNDALL) {
			print_stack((uintptr_t)thr.t_sp, thr.t_pc, addr,
			    argc, argv);
			return (DCMD_OK);	/* found it */
		}
	}

#ifdef __i386
	/*
	 * See $SRC/cmd/mdb/intel/mdb/kvm_ia32dep.c:kt_stack_iter().
	 */
	sp = KTOU((uintptr_t)thr.t_sp + 0xc);
	if (sp >= ubase && sp <= utop) {
		if (crawl(sp, kbase, ktop, ubase, 0) == CRAWL_FOUNDALL) {
			print_stack((uintptr_t)thr.t_sp + 0xc, thr.t_pc, addr,
			    argc, argv);
			return (DCMD_OK);	/* found it */
		}
	}
#endif

	/*
	 * Now walk through the whole stack, starting at the base,
	 * trying every possible "window".
	 */
	for (win = ubase;
	    win + sizeof (struct rwindow) <= utop;
	    win += sizeof (struct rwindow *)) {
		if (crawl(win, kbase, ktop, ubase, 1) == CRAWL_FOUNDALL) {
			print_stack(UTOK(win) - STACK_BIAS, 0, addr,
			    argc, argv);
			return (DCMD_OK);	/* found it */
		}
	}

	/*
	 * We didn't conclusively find the stack.  So we'll take another lap,
	 * and print out anything that looks possible.
	 */
	mdb_printf("Possible stack pointers for thread %p:\n", addr);
	(void) mdb_vread((caddr_t)ubase, stksz, kbase);

	for (win = ubase;
	    win + sizeof (struct rwindow) <= utop;
	    win += sizeof (struct rwindow *)) {
		uintptr_t fp = ((struct rwindow *)win)->rw_fp;
		int levels;

		if ((levels = crawl(win, kbase, ktop, ubase, 1)) > 1) {
			mdb_printf("  %p (%d)\n", fp, levels);
		} else if (levels == CRAWL_FOUNDALL) {
			/*
			 * If this is a live system, the stack could change
			 * between the two mdb_vread(ubase, utop, kbase)'s,
			 * and we could have a fully valid stack here.
			 */
			print_stack(UTOK(win) - STACK_BIAS, 0, addr,
			    argc, argv);
			return (DCMD_OK);	/* found it */
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
findstack_debug(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *av)
{
	findstack_debug_on ^= 1;

	mdb_printf("findstack: debugging is now %s\n",
	    findstack_debug_on ? "on" : "off");

	return (DCMD_OK);
}

int
findstack_init(void)
{
#ifdef __i386
	if (mdb_lookup_by_name("thread_exit", &thread_exit_sym) == -1) {
		mdb_warn("couldn't find 'thread_exit' symbol");
		return (DCMD_ABORT);
	}
#endif

	return (DCMD_OK);
}
