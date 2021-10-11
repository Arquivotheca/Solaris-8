/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cyclic.c	1.2	99/11/19 SMI"

#include "cyclic.h"

#define	CYCLIC_TRACE

#include <mdb/mdb_modapi.h>
#include <sys/timer.h>
#include <sys/cyclic_impl.h>
#include <sys/sysmacros.h>
#include <stdio.h>

int
cyccpu_walk_init(mdb_walk_state_t *wsp)
{
	if (mdb_layered_walk("cpu", wsp) == -1) {
		mdb_warn("couldn't walk 'cpu'");
		return (WALK_ERR);
	}

	return (WALK_NEXT);
}

int
cyccpu_walk_step(mdb_walk_state_t *wsp)
{
	uintptr_t addr = (uintptr_t)((cpu_t *)wsp->walk_layer)->cpu_cyclic;
	cyc_cpu_t cpu;

	if (mdb_vread(&cpu, sizeof (cpu), addr) == -1) {
		mdb_warn("couldn't read cyc_cpu at %p", addr);
		return (WALK_ERR);
	}

	return (wsp->walk_callback(addr, &cpu, wsp->walk_cbdata));
}

void
cyclic_dump_node(cyc_cpu_t *cpu, cyc_index_t *heap, char **c,
    int ndx, int l, int r, int depth)
{
	int heap_left, heap_right;
	int me;
	int i, x = l + (r - l) / 2;

	heap_left = CYC_HEAP_LEFT(ndx);
	heap_right = CYC_HEAP_RIGHT(ndx);
	me = heap[ndx];

	if (ndx >= cpu->cyp_nelems)
		return;

	if (me < 10)
		sprintf(&c[depth][x - 1], " %d", me);
	else if (me >= 100)
		sprintf(&c[depth][x - 1], "%3d", me);
	else
		sprintf(&c[depth][x - 1], "%s%2d%s",
		    CYC_HEAP_LEFT(CYC_HEAP_PARENT(ndx)) == ndx ? " " : "", me,
		    CYC_HEAP_LEFT(CYC_HEAP_PARENT(ndx)) == ndx ? "" : " ");

	if (r - l > 5) {
		c[++depth][x] = '|';
		depth++;

		for (i = l + (r - l) / 4; i < r - (r - l) / 4; i++)
			c[depth][i] = '-';
		c[depth][l + (r - l) / 4] = '+';
		c[depth][r - (r - l) / 4 - 1] = '+';
		c[depth][x] = '+';
	} else {

		if (heap_left >= cpu->cyp_nelems)
			return;

		sprintf(&c[++depth][x - 1], "L%d", heap[heap_left]);

		if (heap_right >= cpu->cyp_nelems)
			return;

		sprintf(&c[++depth][x - 1], "R%d", heap[heap_right]);
		return;
	}

	if (heap_left < cpu->cyp_nelems)
		cyclic_dump_node(cpu, heap, c, heap_left, l, x, depth + 1);

	if (heap_right < cpu->cyp_nelems)
		cyclic_dump_node(cpu, heap, c, heap_right, x, r, depth + 1);
}

#define	LINES_PER_LEVEL 3

void
cyclic_pretty_dump(cyc_cpu_t *cpu)
{
	char **c;
	int i, j;
	int width = 80;
	int depth;
	cyc_index_t *heap;
	size_t hsize = sizeof (cyc_index_t) * cpu->cyp_size;

	heap = mdb_alloc(hsize, UM_SLEEP | UM_GC);

	if (mdb_vread(heap, hsize, (uintptr_t)cpu->cyp_heap) == -1) {
		mdb_warn("couldn't read heap at %p", (uintptr_t)cpu->cyp_heap);
		return;
	}

	for (depth = 0; (1 << depth) < cpu->cyp_nelems; depth++)
		continue;
	depth++;
	depth = (depth + 1) * LINES_PER_LEVEL;

	c = mdb_zalloc(sizeof (char *) * depth, UM_SLEEP|UM_GC);

	for (i = 0; i < depth; i++)
		c[i] = mdb_zalloc(width, UM_SLEEP|UM_GC);

	cyclic_dump_node(cpu, heap, c, 0, 1, width - 2, 0);

	for (i = 0; i < depth; i++) {
		int dump = 0;
		for (j = 0; j < width - 1; j++) {
			if (c[i][j] == '\0')
				c[i][j] = ' ';
			else
				dump = 1;
		}
		c[i][width - 2] = '\n';

		if (dump)
			mdb_printf(c[i]);
	}
}

/*
 * Yes, this is very weak.  Full 16-column-wide 64-bit addresses screw up
 * ::cycinfo's very carefully planned 80-column layout.  We set the column
 * width for addresses to be 11 (instead of 16), knowing that the kernel
 * heap (from which these data structures are allocated) starts at
 * 0x0000030000000000, and isn't likely to extend to 0x0000100000000000.
 */
#ifdef _LP64
#define	CYC_ADDR_WIDTH	11
#else
#define	CYC_ADDR_WIDTH	8
#endif

int
cycinfo(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	cyc_cpu_t cpu;
	cpu_t c;
	cyc_index_t root, i, *heap;
	size_t hsize;
	cyclic_t *cyc;
	uintptr_t caddr;
	uint_t verbose = FALSE, Verbose = FALSE;
	int header = 0;
	cyc_level_t lev;

	if (!(flags & DCMD_ADDRSPEC)) {
		if (mdb_walk_dcmd("cyccpu", "cycinfo", argc, argv) == -1) {
			mdb_warn("can't walk 'cyccpu'");
			return (DCMD_ERR);
		}
		return (DCMD_OK);
	}

	if (mdb_getopts(argc, argv,
	    'v', MDB_OPT_SETBITS, TRUE, &verbose,
	    'V', MDB_OPT_SETBITS, TRUE, &Verbose, NULL) != argc)
		return (DCMD_USAGE);

	if (!DCMD_HDRSPEC(flags) && (verbose || Verbose))
		mdb_printf("\n\n");

	if (DCMD_HDRSPEC(flags) || verbose || Verbose)
		mdb_printf("%3s %*s %7s %6s %*s %15s %s\n", "CPU",
		    CYC_ADDR_WIDTH, "CYC_CPU", "STATE", "NELEMS",
		    CYC_ADDR_WIDTH, "ROOT", "FIRE", "HANDLER");

	if (mdb_vread(&cpu, sizeof (cpu), addr) == -1) {
		mdb_warn("couldn't read cyc_cpu at %p", addr);
		return (DCMD_ERR);
	}

	if (mdb_vread(&c, sizeof (c), (uintptr_t)cpu.cyp_cpu) == -1) {
		mdb_warn("couldn't read cpu at %p", cpu.cyp_cpu);
		return (DCMD_ERR);
	}

	cyc = mdb_alloc(sizeof (cyclic_t) * cpu.cyp_size, UM_SLEEP | UM_GC);
	caddr = (uintptr_t)cpu.cyp_cyclics;

	if (mdb_vread(cyc, sizeof (cyclic_t) * cpu.cyp_size, caddr) == -1) {
		mdb_warn("couldn't read cyclic at %p", caddr);
		return (DCMD_ERR);
	}

	hsize = sizeof (cyc_index_t) * cpu.cyp_size;
	heap = mdb_alloc(hsize, UM_SLEEP | UM_GC);

	if (mdb_vread(heap, hsize, (uintptr_t)cpu.cyp_heap) == -1) {
		mdb_warn("couldn't read heap at %p", cpu.cyp_heap);
		return (DCMD_ERR);
	}

	root = heap[0];

	mdb_printf("%3d %0*p %7s %6d ", c.cpu_id, CYC_ADDR_WIDTH, addr,
	    cpu.cyp_state == CYS_ONLINE ? "online" :
	    cpu.cyp_state == CYS_OFFLINE ? "offline" :
	    cpu.cyp_state == CYS_EXPANDING ? "expand" :
	    cpu.cyp_state == CYS_REMOVING ? "remove" :
	    cpu.cyp_state == CYS_SUSPENDED ? "suspend" : "????",
	    cpu.cyp_nelems);

	if (cpu.cyp_nelems > 0)
		mdb_printf("%0*p %15llx %a\n", CYC_ADDR_WIDTH,
		    caddr, cyc[root].cy_expire, cyc[root].cy_handler);
	else
		mdb_printf("%*s %15s %s\n", CYC_ADDR_WIDTH, "-", "-", "-");

	if (!verbose && !Verbose)
		return (DCMD_OK);

	mdb_printf("\n");

	cyclic_pretty_dump(&cpu);

	mdb_inc_indent(2);

	for (i = 0; i < cpu.cyp_size; i++) {
		int j;

		for (j = 0; j < cpu.cyp_size; j++) {
			if (heap[j] == i)
				break;
		}

		if (!Verbose && j >= cpu.cyp_nelems)
			continue;

		if (!header) {
			header = 1;
			mdb_printf("\n%*s %3s %4s %4s %5s %15s %7s %s\n",
			    CYC_ADDR_WIDTH, "ADDR", "NDX", "HEAP", "LEVL",
			    "PEND", "FIRE", "USECINT", "HANDLER");
		}

		mdb_printf("%0*p %3d ", CYC_ADDR_WIDTH,
		    caddr + i * sizeof (cyclic_t), i);

		mdb_printf("%4d ", j);

		if (j >= cpu.cyp_nelems) {
			mdb_printf("%4s %5s %15s %7s %s\n", "-", "-",
			    "-", "-", "-");
			continue;
		}

		mdb_printf("%4s %5d %15llx ",
		    cyc[i].cy_level == CY_HIGH_LEVEL ? "high" :
		    cyc[i].cy_level == CY_LOCK_LEVEL ? "lock" :
		    cyc[i].cy_level == CY_LOW_LEVEL ? "low" : "????",
		    cyc[i].cy_pend, cyc[i].cy_expire);

		if (cyc[i].cy_interval + cyc[i].cy_expire != INT64_MAX)
			mdb_printf("%7lld ", cyc[i].cy_interval /
			    (uint64_t)(NANOSEC / MICROSEC));
		else
			mdb_printf("%7s ", "-");

		mdb_printf("%a\n", cyc[i].cy_handler);
	}


	if (!Verbose)
		goto out;

	for (lev = CY_LOW_LEVEL; lev < CY_LOW_LEVEL + CY_SOFT_LEVELS; lev++) {
		cyc_softbuf_t *softbuf = &cpu.cyp_softbuf[lev];
		char which = softbuf->cys_hard, shared = 1;
		cyc_pcbuffer_t *pc;
		size_t bufsiz;
		cyc_index_t *buf;

		if (softbuf->cys_hard != softbuf->cys_soft)
			shared = 0;

again:
		pc = &softbuf->cys_buf[which];
		bufsiz = (pc->cypc_sizemask + 1) * sizeof (cyc_index_t);
		buf = mdb_alloc(bufsiz, UM_SLEEP | UM_GC);

		if (mdb_vread(buf, bufsiz, (uintptr_t)pc->cypc_buf) == -1) {
			mdb_warn("couldn't read cypc_buf at %p", pc->cypc_buf);
			continue;
		}

		mdb_printf("\n%3s %4s %4s %4s %*s %4s %*s\n", "CPU",
		    "LEVL", "USER", "NDX", CYC_ADDR_WIDTH, "ADDR", "CYC",
		    CYC_ADDR_WIDTH, "CYC_ADDR", "PEND");

		for (i = 0; i <= pc->cypc_sizemask &&
		    i <= pc->cypc_prodndx; i++) {
			uintptr_t cyc_addr = caddr + buf[i] * sizeof (cyclic_t);

			mdb_printf("%3d %4s %4s ", c.cpu_id,
			    lev == CY_HIGH_LEVEL ? "high" :
			    lev == CY_LOCK_LEVEL ? "lock" :
			    lev == CY_LOW_LEVEL ? "low" : "????",
			    shared ? "shrd" : which == softbuf->cys_hard ?
			    "hard" : "soft");

			mdb_printf("%4d %0*p ", i, CYC_ADDR_WIDTH,
			    (uintptr_t)&buf[i] - (uintptr_t)&buf[0] +
			    (uintptr_t)pc->cypc_buf, buf[i],
			    caddr + buf[i] * sizeof (cyclic_t));

			if (i >= pc->cypc_prodndx)
				mdb_printf("%4s %*s %5s  ",
				    "-", CYC_ADDR_WIDTH, "-", "-");
			else {
				cyclic_t c;

				if (mdb_vread(&c, sizeof (c), cyc_addr) == -1) {
					mdb_warn("\ncouldn't read cyclic at "
					    "%p", cyc_addr);
					continue;
				}

				mdb_printf("%4d %0*p %5d  ", buf[i],
				    CYC_ADDR_WIDTH, cyc_addr, c.cy_pend);
			}

			if (i == (pc->cypc_consndx & pc->cypc_sizemask)) {
				mdb_printf("<-- consndx");
				if (i == (pc->cypc_prodndx & pc->cypc_sizemask))
					mdb_printf(",prodndx");
				mdb_printf("\n");
				continue;
			}

			if (i == (pc->cypc_prodndx & pc->cypc_sizemask)) {
				mdb_printf("<-- prodndx\n");
				continue;
			}
			mdb_printf("\n");

			if (i >= pc->cypc_prodndx)
				break;
		}

		if (!shared && which == softbuf->cys_hard) {
			which = softbuf->cys_soft;
			goto again;
		}
	}

out:
	mdb_dec_indent(2);
	return (DCMD_OK);
}

int
cyctrace_walk_init(mdb_walk_state_t *wsp)
{
	cyc_cpu_t *cpu;
	int i;

	cpu = mdb_zalloc(sizeof (cyc_cpu_t), UM_SLEEP);

	if (wsp->walk_addr == NULL) {
		/*
		 * If an address isn't provided, we'll use the passive buffer.
		 */
		GElf_Sym sym;
		cyc_tracebuf_t *tr = &cpu->cyp_trace[0];
		uintptr_t addr;

		if (mdb_lookup_by_name("cyc_ptrace", &sym) == -1) {
			mdb_warn("couldn't find passive buffer");
			return (-1);
		}

		addr = (uintptr_t)sym.st_value;

		if (mdb_vread(tr, sizeof (cyc_tracebuf_t), addr) == -1) {
			mdb_warn("couldn't read passive buffer");
			return (-1);
		}

		wsp->walk_addr = addr - offsetof(cyc_cpu_t, cyp_trace[0]);
	} else {
		if (mdb_vread(cpu, sizeof (cyc_cpu_t), wsp->walk_addr) == -1) {
			mdb_warn("couldn't read cyc_cpu at %p", wsp->walk_addr);
			mdb_free(cpu, sizeof (cyc_cpu_t));
			return (-1);
		}
	}

	for (i = 0; i < CY_LEVELS; i++) {
		if (cpu->cyp_trace[i].cyt_ndx-- == 0)
			cpu->cyp_trace[i].cyt_ndx = CY_NTRACEREC - 1;
	}

	wsp->walk_data = cpu;

	return (0);
}

int
cyctrace_walk_step(mdb_walk_state_t *wsp)
{
	cyc_cpu_t *cpu = wsp->walk_data;
	cyc_tracebuf_t *buf = cpu->cyp_trace;
	hrtime_t latest = 0;
	int i, ndx, new_ndx, lev, rval;
	uintptr_t addr;

	for (i = 0; i < CY_LEVELS; i++) {
		if ((ndx = buf[i].cyt_ndx) == -1)
			continue;

		/*
		 * Account for NPT.
		 */
		buf[i].cyt_buf[ndx].cyt_tstamp <<= 1;
		buf[i].cyt_buf[ndx].cyt_tstamp >>= 1;

		if (buf[i].cyt_buf[ndx].cyt_tstamp > latest) {
			latest = buf[i].cyt_buf[ndx].cyt_tstamp;
			lev = i;
		}
	}

	/*
	 * If we didn't find one, we're done.
	 */
	if (latest == 0)
		return (-1);

	buf = &buf[lev];
	ndx = buf->cyt_ndx;
	addr = wsp->walk_addr +
	    (uintptr_t)&(buf->cyt_buf[ndx]) - (uintptr_t)cpu;

	rval = wsp->walk_callback(addr, &buf->cyt_buf[ndx], wsp->walk_cbdata);

	new_ndx = ndx == 0 ? CY_NTRACEREC - 1 : ndx - 1;

	if (buf->cyt_buf[new_ndx].cyt_tstamp != 0 &&
	    buf->cyt_buf[new_ndx].cyt_tstamp > buf->cyt_buf[ndx].cyt_tstamp)
		new_ndx = -1;

	buf->cyt_ndx = new_ndx;

	return (rval);
}

void
cyctrace_walk_fini(mdb_walk_state_t *wsp)
{
	cyc_cpu_t *cpu = wsp->walk_data;

	mdb_free(cpu, sizeof (cyc_cpu_t));
}

#define	WHYLEN	17

int
cyctrace_walk(uintptr_t addr, const cyc_tracerec_t *rec, cyc_cpu_t *cpu)
{
	int i;
	char c[WHYLEN];

	for (i = 0; cpu != NULL && i < CY_LEVELS; i++)
		if (addr < (uintptr_t)&cpu->cyp_trace[i + 1].cyt_buf[0])
			break;

	(void) mdb_readstr(c, WHYLEN, (uintptr_t)rec->cyt_why);

	mdb_printf("%08p %4s %15llx %-*s %15llx %15llx\n",
	    addr & UINT_MAX, cpu == NULL ? "pasv" :
	    i == CY_HIGH_LEVEL ? "high" : i == CY_LOCK_LEVEL ? "lock" :
	    i == CY_LOW_LEVEL ? "low" : "????", rec->cyt_tstamp, WHYLEN, c,
	    rec->cyt_arg0, rec->cyt_arg1);

	return (0);
}

/*ARGSUSED*/
int
cyctrace(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	if (!(flags & DCMD_ADDRSPEC))
		addr = NULL;

	if (mdb_pwalk("cyctrace", (mdb_walk_cb_t)cyctrace_walk,
	    (void *)addr, addr) == -1) {
		mdb_warn("couldn't walk cyctrace");
		return (DCMD_ERR);
	}

	return (DCMD_OK);
}

int
cyccover_comp(const void *l, const void *r)
{
	cyc_coverage_t *lhs = (cyc_coverage_t *)l;
	cyc_coverage_t *rhs = (cyc_coverage_t *)r;

	char ly[WHYLEN], ry[WHYLEN];

	if (rhs->cyv_why == lhs->cyv_why)
		return (0);

	if (rhs->cyv_why == NULL)
		return (-1);

	if (lhs->cyv_why == NULL)
		return (1);

	(void) mdb_readstr(ly, WHYLEN, (uintptr_t)lhs->cyv_why);
	(void) mdb_readstr(ry, WHYLEN, (uintptr_t)rhs->cyv_why);

	return (strcmp(ly, ry));
}

/*ARGSUSED*/
int
cyccover(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	cyc_coverage_t cv[CY_NCOVERAGE];
	char c[WHYLEN];
	GElf_Sym sym;
	int i;

	if (mdb_lookup_by_name("cyc_coverage", &sym) == -1) {
		mdb_warn("couldn't find coverage information");
		return (DCMD_ERR);
	}

	addr = (uintptr_t)sym.st_value;

	if (mdb_vread(cv, sizeof (cyc_coverage_t) * CY_NCOVERAGE, addr) == -1) {
		mdb_warn("couldn't read coverage array at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%-*s %8s %8s %8s %15s %15s\n",
	    WHYLEN, "POINT", "HIGH", "LOCK", "LOW/PASV", "ARG0", "ARG1");

	qsort(cv, CY_NCOVERAGE, sizeof (cyc_coverage_t), cyccover_comp);

	for (i = 0; i < CY_NCOVERAGE; i++) {
		if (cv[i].cyv_why != NULL) {
			(void) mdb_readstr(c, WHYLEN, (uintptr_t)cv[i].cyv_why);
			mdb_printf("%-*s %8d %8d %8d %15llx %15llx\n",
			    WHYLEN, c,
			    cv[i].cyv_count[CY_HIGH_LEVEL],
			    cv[i].cyv_count[CY_LOCK_LEVEL],
			    cv[i].cyv_passive_count != 0 ?
			    cv[i].cyv_passive_count :
			    cv[i].cyv_count[CY_LOW_LEVEL],
			    cv[i].cyv_arg0, cv[i].cyv_arg1);
		}
	}

	return (DCMD_OK);
}

/*ARGSUSED*/
int
cyclic(uintptr_t addr, uint_t flags, int argc, const mdb_arg_t *argv)
{
	cyclic_t cyc;

	if (!(flags & DCMD_ADDRSPEC))
		return (DCMD_USAGE);

	if (DCMD_HDRSPEC(flags))
		mdb_printf("%?s %4s %5s %5s %15s %7s %s\n", "ADDR", "LEVL",
		    "PEND", "FLAGS", "FIRE", "USECINT", "HANDLER");

	if (mdb_vread(&cyc, sizeof (cyclic_t), addr) == -1) {
		mdb_warn("couldn't read cyclic at %p", addr);
		return (DCMD_ERR);
	}

	mdb_printf("%0?p %4s %5d  %04x %15llx %7lld %a\n", addr,
	    cyc.cy_level == CY_HIGH_LEVEL ? "high" :
	    cyc.cy_level == CY_LOCK_LEVEL ? "lock" :
	    cyc.cy_level == CY_LOW_LEVEL ? "low" : "????",
	    cyc.cy_pend, cyc.cy_flags, cyc.cy_expire,
	    cyc.cy_interval / (uint64_t)(NANOSEC / MICROSEC),
	    cyc.cy_handler);

	return (DCMD_OK);
}
