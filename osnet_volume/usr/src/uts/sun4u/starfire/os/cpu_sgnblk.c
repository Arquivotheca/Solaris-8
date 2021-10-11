/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpu_sgnblk.c	1.7	99/04/14 SMI"

/*
 *	Following is STARFIRE specific code
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/vmem.h>
#include <sys/mman.h>
#include <sys/vm.h>

#include <sys/cmn_err.h>
#include <sys/cpu_sgnblk_defs.h>
#include <sys/starfire.h>

#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kp.h>
#include <sys/vtrace.h>

/*
 * SIGBCPU represents the cpu maintaining the primary
 * sigblock (bbsram).  This bbsram is used for CVC
 * and maintains the post2obp structure.  It starts
 * out as the bootproc (cpu0).
 */
struct cpu	*SIGBCPU = &cpu0;

cpu_sgnblk_t *cpu_sgnblkp[NCPU];

/*
 * Mapin the the cpu's signature block.
 */
void
cpu_sgn_mapin(int cpuid)
{
	uint64_t bbsram_physaddr;
	uint64_t cpu_sgnblk_physaddr;
	uint32_t cpu_sgnblk_offset;
	caddr_t	cvaddr;
	u_int	num_pages;
	pfn_t	pfn;

	ASSERT(cpu_sgnblkp[cpuid] == NULL);

	/*
	 * Construct the physical base address of the bbsram
	 * in PSI space associated with this cpu in question.
	 */
	cpu_sgnblk_physaddr = bbsram_physaddr =
				STARFIRE_UPAID2UPS(cpuid) | STARFIRE_PSI_BASE;

	/*
	 * The cpu_sgnblk pointer offsets are stored in the
	 * undefined hardware trap slot 0x7f which is located
	 * at offset 0xfe0. There are 2 of them since the
	 * bbsram is shared among the 2 cpus residing on the
	 * a PC. We need to determine the CPU in question whether
	 * it is in port 0 or 1. CPU on port 0 has its
	 * signature blkptr stored in 0xfe0 while the cpu_sgnblk
	 * ptr of local port 1's CPU is in offset 0xfe8.
	 */
	if (cpuid & 0x1) {
		/* CPU is in local port 1 */
		bbsram_physaddr |= 0xfe8ULL;
	} else {
		/* CPU is in local port 0 */
		bbsram_physaddr |= 0xfe0ULL;
	}

	/*
	 * Read in the cpu_sgnblk pointer offset. Add it to the bbsram
	 * base address to get the base address of the cpu_sgnblk.
	 */
	cpu_sgnblk_offset = ldphysio(bbsram_physaddr);
	cpu_sgnblk_physaddr += cpu_sgnblk_offset;

	pfn = (u_int) (cpu_sgnblk_physaddr >> MMU_PAGESHIFT);

	num_pages = mmu_btopr(((u_int)(cpu_sgnblk_physaddr &
				MMU_PAGEOFFSET) + sizeof (cpu_sgnblk_t)));

	/*
	 * Map in the cpu_sgnblk
	 */
	cvaddr = vmem_alloc(heap_arena, ptob(num_pages), VM_SLEEP);

	hat_devload(kas.a_hat, cvaddr, ptob(num_pages),
	    pfn, PROT_READ | PROT_WRITE, HAT_LOAD_LOCK);

	cpu_sgnblkp[cpuid] = ((cpu_sgnblk_t *)(cvaddr +
	    (uint32_t)(cpu_sgnblk_offset & MMU_PAGEOFFSET)));
}

void
cpu_sgn_mapout(int cpuid)
{
	u_long 	cvaddr, num_pages;
	uint32_t cpu_sgnblk_offset;
	uint64_t cpu_sgnblk_physaddr;
	uint64_t bbsram_physaddr;

	if ((cvaddr = (u_long)cpu_sgnblkp[cpuid]) == NULL) {
		cmn_err(CE_WARN, "cpu_sgn_mapout: ERROR: "
			"cpu_sgnblkp[%d] = NULL\n", cpuid);
	} else {
		cvaddr &= ~MMU_PAGEOFFSET;

		/*
		 * Construct the physical base address of the bbsram
		 * in PSI space associated with this cpu in question.
		 */
		cpu_sgnblk_physaddr = bbsram_physaddr =	\
					STARFIRE_UPAID2UPS(cpuid)	\
					| STARFIRE_PSI_BASE;

		/*
		 * The cpu_sgnblk pointer offsets are stored in the
		 * undefined hardware trap slot 0x7f which is located
		 * at offset 0xfe0. There are 2 of them since the
		 * bbsram is shared among the 2 cpus residing on the
		 * a PC. We need to determine the CPU in question whether
		 * it is in port 0 or 1. CPU on port 0 has its
		 * signature blkptr stored in 0xfe0 while the cpu_sgnblk
		 * ptr of local port 1's CPU is in offset 0xfe8.
		 */
		if (cpuid & 0x1) {
			/* CPU is in local port 1 */
			bbsram_physaddr |= 0xfe8ULL;
		} else {
			/* CPU is in local port 0 */
			bbsram_physaddr |= 0xfe0ULL;
		}

		/*
		 * Read in the cpu_sgnblk pointer offset. Add it to the bbsram
		 * base address to get the base address of the cpu_sgnblk.
		 */
		cpu_sgnblk_offset = ldphysio(bbsram_physaddr);
		cpu_sgnblk_physaddr += cpu_sgnblk_offset;

		num_pages = mmu_btopr(((u_int)(cpu_sgnblk_physaddr &
				MMU_PAGEOFFSET) + sizeof (cpu_sgnblk_t)));

		hat_unload(kas.a_hat, (caddr_t)cvaddr, ptob(num_pages),
		    HAT_UNLOAD_UNLOCK);
		vmem_free(heap_arena, (caddr_t)cvaddr, ptob(num_pages));

		cpu_sgnblkp[cpuid] = NULL;
	}
}

/*
 * Update signature block and the signature ring buffer of a given cpu_id.
 */
void
cpu_sgn_update(int cpuid, u_short sgn, u_char state, u_char sub_state)
{
	u_char idx;
	cpu_sgnblk_t *cpu_sgnblkptr;

	if (cpu_sgnblkp[cpuid] == NULL)
		return;

	cpu_sgnblkptr = cpu_sgnblkp[cpuid];

	cpu_sgnblkptr->sigb_signature.state_t.sig = sgn;
	cpu_sgnblkptr->sigb_signature.state_t.state = state;
	cpu_sgnblkptr->sigb_signature.state_t.sub_state = sub_state;

	/* Update the ring buffer */
	idx = cpu_sgnblkptr->sigb_ringbuf.wr_ptr;
	cpu_sgnblkptr->sigb_ringbuf.ringbuf[idx].state_t.sig = sgn;
	cpu_sgnblkptr->sigb_ringbuf.ringbuf[idx].state_t.state = state;
	cpu_sgnblkptr->sigb_ringbuf.ringbuf[idx].state_t.sub_state = 	\
								sub_state;
	cpu_sgnblkptr->sigb_ringbuf.wr_ptr += 1;
	cpu_sgnblkptr->sigb_ringbuf.wr_ptr &= RB_IDX_MASK;
}

/*
 * Update signature block and the signature ring buffer of all CPUs.
 */
void
sgn_update_all_cpus(u_short sgn, u_char state, u_char sub_state)
{
	int i = 0;
	u_char cpu_state;
	u_char cpu_sub_state;

	for (i = 0; i < NCPU; i++) {
		cpu_sgnblk_t *sblkp;
		sblkp = cpu_sgnblkp[i];
		cpu_sub_state = sub_state;
		if ((sblkp != NULL) && 	\
			(cpu[i] != NULL && (cpu[i]->cpu_flags &	\
					(CPU_EXISTS|CPU_QUIESCED)))) {

			if (sub_state == EXIT_REBOOT) {
				cpu_sub_state = \
				sblkp->sigb_signature.state_t.sub_state;

				if ((cpu_sub_state == EXIT_PANIC1) ||	\
					(cpu_sub_state == EXIT_PANIC2))
					cpu_sub_state = EXIT_PANIC_REBOOT;
				else
					cpu_sub_state = EXIT_REBOOT;
			}

			/*
			 * If we get here from an OBP sync after watchdog,
			 * we need to retain the watchdog sync state so that
			 * hostmon knows what's going on.  So if we're in
			 * watchdog we don't update the state.
			 */

			cpu_state = 	\
			sblkp->sigb_signature.state_t.state;
			if (cpu_state == SIGBST_WATCHDOG_SYNC)
				cpu_sgn_update(i, sgn,
					SIGBST_WATCHDOG_SYNC, cpu_sub_state);
			else if (cpu_state == SIGBST_REDMODE_SYNC)
				cpu_sgn_update(i, sgn,
					SIGBST_REDMODE_SYNC, cpu_sub_state);
			else
				cpu_sgn_update(i, sgn, state, cpu_sub_state);
		}
	}
}

int
cpu_sgn_exists(int cpuid)
{
	return (cpu_sgnblkp[cpuid] != NULL);
}

u_short
get_cpu_sgn(int cpuid)
{
	if (cpu_sgnblkp[cpuid] == NULL)
		return ((u_short)-1);

	return (cpu_sgnblkp[cpuid]->sigb_signature.state_t.sig);
}

u_char
get_cpu_sgn_state(int cpuid)
{
	if (cpu_sgnblkp[cpuid] == NULL)
		return ((u_char)-1);

	return (cpu_sgnblkp[cpuid]->sigb_signature.state_t.state);
}
