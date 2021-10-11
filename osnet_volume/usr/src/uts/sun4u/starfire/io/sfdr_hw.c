/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sfdr_hw.c	1.16	98/09/30 SMI"

/*
 * Starfire Memory Controller specific routines.
 */

#include <sys/debug.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/dditypes.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/machsystm.h>
#include <sys/starfire.h>

#include <sys/dr.h>
#include <sys/sfdr.h>
#include <sys/pda.h>

/*
 * portctrl
 */
extern int		pc_madr_add(int lboard, int rboard, int proc,
						uint_t madr);
/*
 * memctrl
 */
extern uint64_t		mc_get_idle_addr(dnode_t nodeid);
extern int		mc_read_asr(dnode_t nodeid, uint_t *mcregp);
extern int		mc_write_asr(dnode_t nodeid, uint_t mcreg);
extern uint64_t		mc_asr_to_pa(uint_t mcreg);

typedef enum {
	DO_IDLE,
	DO_UNIDLE,
	DO_PAUSE,
	DO_UNPAUSE
} pc_op_t;

static int		sfhw_program_interconnect(dnode_t nodeid,
						int boardnum, uint_t mcreg);
static void		sfhw_iopc_op(pda_handle_t ph, pc_op_t op);

/*
 * Get the base physical address to which the given
 * memory controller node responds.
 */
/*ARGSUSED*/
int
sfhw_get_base_physaddr(dnode_t nodeid, uint64_t *basepa)
{
	uint_t		mcreg;
	static fn_t	f = "sfhw_get_base_physaddr";

	if (mc_read_asr(nodeid, &mcreg) < 0) {
		PR_HW("%s: failed to read asr for nodeid (0x%x)\n",
			f, (uint_t)nodeid);
		return (-1);
	}

	*basepa = mc_asr_to_pa(mcreg);

	return (0);
}

/*ARGSUSED*/
int
sfhw_program_memctrl(dnode_t nodeid, int boardnum)
{
	uint_t	mcreg;

	if (mc_read_asr(nodeid, &mcreg) < 0)
		return (-1);

	mcreg |= STARFIRE_MC_MEM_PRESENT_MASK;

	if (mc_write_asr(nodeid, mcreg) < 0)
		return (-1);

	if (sfhw_program_interconnect(nodeid, boardnum, mcreg) < 0)
		return (-1);

	return (0);
}

/*ARGSUSED*/
int
sfhw_deprogram_memctrl(dnode_t nodeid, int boardnum)
{
	uint_t	mcreg;

	if (mc_read_asr(nodeid, &mcreg) < 0)
		return (-1);

	ASSERT(mcreg & STARFIRE_MC_MEM_PRESENT_MASK);
	/*
	 * Turn off presence bit.
	 */
	mcreg &= ~STARFIRE_MC_MEM_PRESENT_MASK;

	if (sfhw_program_interconnect(nodeid, boardnum, mcreg) < 0)
		return (-1);

	if (mc_write_asr(nodeid, mcreg) < 0)
		return (-1);

	return (0);
}

/*ARGSUSED*/
static int
sfhw_program_interconnect(dnode_t nodeid, int boardnum, uint_t mcreg)
{
	int		rv = 0;
	register int	b;
	pda_handle_t	ph;
	static fn_t	f = "sfhw_program_interconnect";

	if ((ph = pda_open()) == NULL) {
		cmn_err(CE_WARN, "sfdr:%s: failed to open pda", f);
		return (-1);
	}

	for (b = 0; b < MAX_BOARDS; b++) {
		int		p;
		ushort_t	bda_proc, bda_ioc;
		board_desc_t	*bdesc;

		if (pda_board_present(ph, b) == 0)
			continue;

		bdesc = (board_desc_t *)pda_get_board_info(ph, b);
		/*
		 * Update PCs for CPUs.
		 */
		bda_proc = bdesc->bda_proc;
		for (p = 0; p < MAX_PROCMODS; p++) {
			if (BDA_NBL(bda_proc, p) != BDAN_GOOD)
				continue;

			if (pc_madr_add(b, boardnum, p, mcreg))
				break;
		}
		if (p < MAX_PROCMODS) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to program cpu port-"
				"controllers (0x%x, %d, 0x%x)",
				f, (uint_t)nodeid, boardnum, mcreg);
			rv = -1;
			break;
		}
		/*
		 * Update PCs for IOCs.
		 */
		bda_ioc = bdesc->bda_ioc;
		for (p = 0; p < MAX_IOCS; p++) {
			if (BDA_NBL(bda_ioc, p) != BDAN_GOOD)
				continue;

			if (pc_madr_add(b, boardnum, p + 4, mcreg))
				break;
		}
		if (p < MAX_IOCS) {
			cmn_err(CE_WARN,
				"sfdr:%s: failed to program ioc port-"
				"controllers (0x%x, %d, 0x%x)",
				f, (uint_t)nodeid, boardnum, mcreg);
			rv = -1;
			break;
		}
	}

	pda_close(ph);

	return (rv);
}

int
sfhw_cpu_pc_idle(int cpuid)
{
	int		board, port, p;
	u_longlong_t	pc_addr;
	uchar_t		rvalue;
	static fn_t	f = "sfhw_cpu_pc_idle";

	board = cpuid / MAX_CPU_UNITS_PER_BOARD;
	port = cpuid % MAX_CPU_UNITS_PER_BOARD;
	p = cpuid & 1;

	pc_addr = STARFIRE_BB_PC_ADDR(board, port, 0);

	PR_HW("%s: PC idle cpu %d (addr = 0x%llx, port = %d, p = %d)\n",
		f, cpuid, pc_addr, port, p);

	rvalue = ldbphysio(pc_addr);
	rvalue |= STARFIRE_BB_PC_IDLE(p);
	stbphysio(pc_addr, rvalue);
	DELAY(50000);

	return (0);
}

int
sfhw_cpu_reset_on(int cpuid)
{
	int		board, port, p;
	u_longlong_t	reset_addr;
	uchar_t		rvalue;
	static fn_t	f = "sfhw_cpu_reset_on";

	board = cpuid / MAX_CPU_UNITS_PER_BOARD;
	port = cpuid % MAX_CPU_UNITS_PER_BOARD;
	p = cpuid & 1;

	reset_addr = STARFIRE_BB_RESET_ADDR(board, port);

	PR_HW("%s: reseting cpu %d (addr = 0x%llx, port = %d, p = %d)\n",
		f, cpuid, reset_addr, port, p);

	rvalue = ldbphysio(reset_addr);
	rvalue |= STARFIRE_BB_SYSRESET(p);
	stbphysio(reset_addr, rvalue);
	DELAY(50000);

	return (0);
}

int
sfhw_cpu_reset_off(int cpuid)
{
	int		board, port, p;
	u_longlong_t	reset_addr;
	uchar_t		rvalue;

	board = cpuid / MAX_CPU_UNITS_PER_BOARD;
	port = cpuid % MAX_CPU_UNITS_PER_BOARD;
	p = cpuid & 1;

	reset_addr = STARFIRE_BB_RESET_ADDR(board, port);

	/*
	 * Protocol here is borrowed from ssp_release_processor()
	 * in libSspLoader:ssp_bbc.c.
	 * 1. Clear RESET
	 */
	rvalue = ldbphysio(reset_addr);
	rvalue &= ~STARFIRE_BB_SYSRESET(p);
	stbphysio(reset_addr, rvalue);
	DELAY(50000);

	return (0);
}

static void
sfhw_iopc_op(pda_handle_t ph, pc_op_t op)
{
	register int	b;
	static fn_t	f = "sfhw_iopc_op";

	for (b = 0; b < MAX_BOARDS; b++) {
		int		p;
		ushort_t	bda_ioc;
		board_desc_t	*bdesc;

		if (pda_board_present(ph, b) == 0)
			continue;

		bdesc = (board_desc_t *)pda_get_board_info(ph, b);
		/*
		 * Update PCs for IOCs.
		 */
		bda_ioc = bdesc->bda_ioc;
		for (p = 0; p < MAX_IOCS; p++) {
			u_longlong_t	idle_addr;
			uchar_t		value;

			if (BDA_NBL(bda_ioc, p) != BDAN_GOOD)
				continue;

			idle_addr = STARFIRE_BB_PC_ADDR(b, p, 1);

			switch (op) {
			case DO_PAUSE:
				value = STARFIRE_BB_PC_PAUSE(p);
				break;

			case DO_IDLE:
				value = STARFIRE_BB_PC_IDLE(p);
				break;

			case DO_UNPAUSE:
				value = ldbphysio(idle_addr);
				value &= ~STARFIRE_BB_PC_PAUSE(p);
				break;

			case DO_UNIDLE:
				value = ldbphysio(idle_addr);
				value &= ~STARFIRE_BB_PC_IDLE(p);
				break;

			default:
				cmn_err(CE_PANIC,
					"sfdr:%s: unknown op (%d)",
					f, (int)op);
				/*NOTREACHED*/
			}
			stbphysio(idle_addr, value);
		}
	}
}

/*
 * UPA IDLE
 * Protocol = PAUSE -> IDLE -> UNPAUSE
 * In reality since we only "idle" the IOPCs it's sufficient
 * to just issue the IDLE operation since (in theory) all IOPCs
 * in the field are PC6.  However, we'll be robust and do the
 * proper workaround protocol so that we never have to worry!
 */
void
sfhw_idle_interconnect(void *cookie)
{
	pda_handle_t	ph = (pda_handle_t)cookie;

	ASSERT(ph != NULL);

	sfhw_iopc_op(ph, DO_PAUSE);
	sfhw_iopc_op(ph, DO_IDLE);
	DELAY(100);
	sfhw_iopc_op(ph, DO_UNPAUSE);
	DELAY(100);
}

/*
 * UPA UNIDLE
 * Protocol = UNIDLE
 */
void
sfhw_resume_interconnect(void *cookie)
{
	pda_handle_t	ph = (pda_handle_t)cookie;

	ASSERT(ph != NULL);

	sfhw_iopc_op(ph, DO_UNIDLE);
	DELAY(100);
}

void
sfhw_dump_pdainfo(dr_handle_t *hp)
{
	int		i, board;
	pda_handle_t	ph;
	board_desc_t	*bdesc;
	static fn_t	f = "sfhw_dump_pdainfo";

	if ((ph = pda_open()) == NULL) {
		cmn_err(CE_WARN, "%s: unable open pda", f);
		return;
	}

	board = GETSLOT(hp->h_dev);

	if (pda_board_present(ph, board) == 0) {
		cmn_err(CE_CONT,
			"%s: board %d is MISSING\n", f, board);
		pda_close(ph);
		return;
	}

	cmn_err(CE_CONT, "%s: board %d is PRESENT\n", f, board);

	bdesc = (board_desc_t *)pda_get_board_info(ph, board);
	if (bdesc == NULL) {
		cmn_err(CE_CONT,
			"%s: no board descriptor found for board %d\n",
			f, board);
		pda_close(ph);
		return;
	}

	for (i = 0; i < MAX_PROCMODS; i++) {
		if (BDA_NBL(bdesc->bda_proc, i) == BDAN_GOOD)
			cmn_err(CE_CONT,
				"%s: proc %d.%d PRESENT\n",
				f, board, i);
		else
			cmn_err(CE_CONT,
				"%s: proc %d.%d MISSING\n",
				f, board, i);
	}

	for (i = 0; i < MAX_MGROUPS; i++) {
		if (BDA_NBL(bdesc->bda_mgroup, i) == BDAN_GOOD)
			cmn_err(CE_CONT,
				"%s: mgroup %d.%d PRESENT\n",
				f, board, i);
		else
			cmn_err(CE_CONT,
				"%s: mgroup %d.%d MISSING\n",
				f, board, i);
	}

	for (i = 0; i < MAX_IOCS; i++) {
		int	s;

		if (BDA_NBL(bdesc->bda_ioc, i) == BDAN_GOOD) {
			cmn_err(CE_CONT,
				"%s: ioc %d.%d PRESENT\n",
				f, board, i);
			for (s = 0; s < MAX_SLOTS_PER_IOC; s++) {
				if (BDA_NBL(bdesc->bda_ios[i], s) != BDAN_GOOD)
					continue;
				cmn_err(CE_CONT,
					"%s: ..scard %d.%d.%d PRESENT\n",
					f, board, i, s);
			}
		} else {
			cmn_err(CE_CONT,
				"%s: ioc %d.%d MISSING\n",
				f, board, i);
		}
	}

	cmn_err(CE_CONT,
		"%s: board %d memsize = %d pages\n",
		f, board, pda_get_mem_size(ph, board));

	pda_close(ph);
}
