/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ident	"@(#)hwbcopy.c	1.52	97/04/18 SMI"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/pte.h>
#include <sys/kmem.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <sys/sysmacros.h>
#include <sys/iommu.h>
#include <sys/physaddr.h>	/* PA_IS_IO_SPACE() */
#include <sys/debug.h>
#include <sys/spl.h>

#include <sys/cpuvar.h>

/* #define	BCSTAT */

#include <sys/bcopy_if.h>

extern longlong_t	xdb_cc_ssar_get(void);
extern void		xdb_cc_ssar_set(longlong_t src);
extern longlong_t	xdb_cc_sdar_get(void);
extern void		xdb_cc_sdar_set(longlong_t src);
extern void		xdb_cc_sdr_set(longlong_t *data);
extern u_short		xdb_cc_ipr_get(void);
extern u_int		disable_traps(void);
extern void		enable_traps(u_int psr_value);
extern void 		debug_enter(char *msg);

extern void 		bcopy_asm(const void *from, void *to, size_t count);
extern void 		bcopy_asm_toio(const void *from, void *to, \
			    size_t count);
extern void 		bzero_asm(void *addr, size_t resid);

#ifdef IOC_DW_BUG
extern int	need_ioc_workaround(void);
int		ioc_dw_bug = 1;
#endif	/* IOC_DW_BUG */

#define	SIGN_CHECK(card64)	((u_int)(card64 >> 32) & (1U << 31))

/*
 * (physical) block scan, fill, & copy primitives
 * these are a bit sensitive because they operate with traps disabled
 */
void
hwbc_scan(u_int blks, pa_t src)
{
	/* if memory, set the cacheable bit */
	if (!(PA_IS_IO_SPACE(src)))
		src |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	while (blks != 0) {
		u_int psr = disable_traps();
		u_int ipr_min = (1 << (spltoipl(psr) + 1));

		while (blks != 0) {
			xdb_cc_ssar_set(src);
			src += BLOCK_SIZE;
			blks--;
			if (xdb_cc_ipr_get() >= ipr_min) {
				break;
			}
		}

		while (SIGN_CHECK(xdb_cc_ssar_get()) == 0) { }
		enable_traps(psr);
	}
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
void
hwbc_fill(u_int blks, pa_t dest, pa_t pattern)
{
	/* if memory, set the cacheable bit */
	if (!(PA_IS_IO_SPACE(dest)))
		dest |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	while (blks != 0) {
		u_int psr = disable_traps();
		u_int ipr_min = (1 << (spltoipl(psr) + 1));

		xdb_cc_ssar_set(pattern);	/* stateful */

		while (blks != 0) {
			xdb_cc_sdar_set(dest);
			dest += BLOCK_SIZE;
			blks--;
			if (xdb_cc_ipr_get() >= ipr_min) {
				break;
			}
		}

		while (SIGN_CHECK(xdb_cc_sdar_get()) == 0) { }

		enable_traps(psr);
	}
}

static int hwbc_zero_buf[(NCPU + 1) * 16] = { 0, };
static int hwbc_zero_buf_cpu[NCPU];
static pa_t hwbc_zero_pa[NCPU];

static void
init_bzbuf()
{
	u_int a = (u_int) hwbc_zero_buf;
	u_int b, off, i;
	pa_t c;

	/* align to 64 bytes boundary. */
	b = (a + BLOCK_SIZE) & ~(BLOCK_SIZE - 1);

	for (i = 0; i < NCPU; i++) {
		hwbc_zero_buf_cpu[i] = b;
		b +=  BLOCK_SIZE;
	}

	for (i = 0; i < NCPU; i++) {
		off = hwbc_zero_buf_cpu[i] & (0xfff);
		b = mmu_probe((caddr_t)hwbc_zero_buf_cpu[i], NULL);
		if (b == 0) {
			cmn_err(CE_PANIC, "no pte??");
		}

		/* b is a pte now */
		c = (pa_t)b;
		c = ((c & 0xffffff00) << 4) + off;
		hwbc_zero_pa[i] = c | ((u_longlong_t)1 << BC_CACHE_SHIFT);
	}
}

static void
hwbc_zero(u_int blks, pa_t dest)
{
	int cpu_id = curthread->t_cpu->cpu_id;
	pa_t pattern;

	/* if memory, set the cacheable bit */
	if (!(PA_IS_IO_SPACE(dest)))
		dest |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	pattern = hwbc_zero_pa[cpu_id];

	while (blks != 0) {
		u_int psr = disable_traps();
		u_int ipr_min = (1 << (spltoipl(psr) + 1));

		xdb_cc_ssar_set(pattern);	/* stateful */

		while (blks != 0) {
			xdb_cc_sdar_set(dest);
			dest += BLOCK_SIZE;
			blks--;
			if (xdb_cc_ipr_get() >= ipr_min) {
				break;
			}
		}

		while (SIGN_CHECK(xdb_cc_sdar_get()) == 0) { }

		enable_traps(psr);
	}
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
/*
 * this loop runs a hair short of the theoretical maximum bandwidth
 * for the MXCC block copy engine.
 *
 * Note that the instructions between
 * xdb_cc_ssar_set(src) and xdb_cc_ssar_set(dest)
 * are "free" -- as writing the dest register will
 * not return until the source read has completed.
 * Note also that the xdb_cc_sdar_set() completes quickly.
 */
void
hwbc_copy(u_int blks, pa_t src, pa_t dest)
{
	/* if memory, set the cacheable bit */
	if (!(PA_IS_IO_SPACE(src)))
		src |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	if (!(PA_IS_IO_SPACE(dest)))
		dest |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	while (blks != 0) {
		u_int psr = disable_traps();
		u_int ipr_min = (1 << (spltoipl(psr) + 1));

		while (blks != 0) {
			u_int bail = 0;

			xdb_cc_ssar_set(src);

			if (xdb_cc_ipr_get() >= ipr_min)
				bail = 1;

			src += BLOCK_SIZE;
			blks--;

			xdb_cc_sdar_set(dest);
			dest += BLOCK_SIZE;

			if (bail)
				break;
		}

		while (SIGN_CHECK(xdb_cc_sdar_get()) == 0) { }
		enable_traps(psr);
	}
}

/*
 * physical page/block - scan, fill, zero, & copy primitives
 */
void
hwpage_scan(u_int pfn)
{
	u_int blks = BLOCKS_PER_PAGE;
	pa_t src = PFN_ENLARGE(pfn);

	hwbc_scan(blks, src);
}

void
hwpage_zero(u_int pfn)
{
	u_int blks = BLOCKS_PER_PAGE;
	pa_t dest = PFN_ENLARGE(pfn);

	hwbc_zero(blks, dest);
}

void
hwpage_fill(u_int pfn, pa_t pattern)
{
	u_int blks = BLOCKS_PER_PAGE;
	pa_t dest = PFN_ENLARGE(pfn);

	hwbc_fill(blks, dest, pattern);
}

void
hwpage_copy(u_int spfn, u_int dpfn)
{
	u_int blks = BLOCKS_PER_PAGE;
	pa_t src = PFN_ENLARGE(spfn);
	pa_t dest = PFN_ENLARGE(dpfn);

	hwbc_copy(blks, src, dest);
}

/*
 * The Presto driver relies on this routine and it's currently implemented
 * interface. Any change to this interface could break the Presto driver.
 */
void
hwblk_zero(u_int blks, pa_t dest)
{
	hwbc_zero(blks, dest);
}

/*
 * Optional DEBUG if you're paranoid...
 */

/* #define	HWBCOPY_DEBUG */


/*
 * initialize 'hwbc_zeroes' to a hardware block address for 'hwbc_zero_buf',
 * has to deal with alignment issues; also one-time check some invariants.
 */
void
hwbc_init(void)
{
#ifndef lint

	/* Constant expression checkers confuse lint. */
	ASSERT(MMU_STD_FIRSTSHIFT == 12);
	ASSERT(MMU_STD_FIRSTSHIFT == PAGESHIFT);
	ASSERT(MMU_STD_FIRSTMASK == ((1 << MMU_STD_FIRSTSHIFT) - 1));
	ASSERT(sizeof (pa_t) == 8);

#endif /* lint */

	init_bzbuf();

#ifdef USE_HW_BCOPY
	{
		extern int hwbcopy, defhwbcopy;
#ifdef BCSTAT
		mutex_init(&bcstat.lock, NULL, MUTEX_DEFAULT, NULL);
#endif
		ASSERT(hwbcopy == 0);	/* We shouldn't be using hwbcopy */
					/* until now */
		hwbcopy = defhwbcopy;	/* now turn it on */
	}

#ifdef	HWBCOPY_DEBUG
	{
		static void hwbc_selftest(void);
		hwbc_selftest();
	}
#endif	/* HWBCOPY_DEBUG */
#endif	/* USE_HW_BCOPY */

#ifdef IOC_DW_BUG
	/*
	 * we boot assuming we have broken IOCs
	 * if (none of them are really broken)
	 * we disable the workaround here.
	 */
	if (!need_ioc_workaround())
		ioc_dw_bug = 0;
#endif IOC_DW_BUG

}

/* ---------------------------------------------------------------------- */

#ifdef USE_HW_BCOPY

#ifdef IOC_DW_BUG

#define	IOC_DW_BUG_MASK		0x7400000FF0F80000
#define	IOC_DW_BUG_PAT		0x4000000FE0200000
#define	CC_BASE_LOCAL			0x01f00000

void
hwbc_copy_toio(u_int blks, pa_t src, pa_t dest)
{
	/* if memory, set the cacheable bit */
	if (!(PA_IS_IO_SPACE(src)))
		src |= ((u_longlong_t)1 << BC_CACHE_SHIFT);

	while (blks != 0) {
		u_int psr = disable_traps();
		u_int ipr_min = (1 << (spltoipl(psr) + 1));

		while (blks != 0) {
			u_int i;
			u_longlong_t *sdrp = (u_longlong_t *)CC_BASE_LOCAL;

			xdb_cc_ssar_set(src);
			xdb_cc_sdar_set(dest);

			/*
			 * this poll has never been seen to loop more
			 * than once, but is necessary to guarantee
			 * the data we read from the Stream Data Register
			 * is valid when we access it.
			 */
			while (SIGN_CHECK(xdb_cc_ssar_get()) == 0) {}

			/*
			 * Read 8 double words from MXCC.BCOPY_STREAM_DATA_REG
			 * and if pattern matches IOC bug, re-write that
			 * double word as two words
			 */
			for (i = 0; i < 64; i += 8, sdrp++) {
				u_longlong_t ddata;

				ddata =	ldda_02(sdrp);

				if ((ddata & IOC_DW_BUG_MASK) ==
				    IOC_DW_BUG_PAT) {
					u_int hibits;

					/*
					 * re-write double as 2 words
					 * note big endian -- MSW is in
					 * %o0 (dest >> 32), and goes
					 * to dest, LSW to (dest + 4).
					 *
					 * need to switch b/c different ASI
					 * for each pa[35:32].
					 */

					hibits = (u_int)(dest >> 32) & 0xf;

					switch (hibits) {
					case 0x8:
						sta_28((u_int)(ddata >> 32),
						    (u_int *)(dest + i));
						sta_28((u_int)ddata,
						    (u_int *)(dest + i + 4));
						break;
					case 0x9:
						sta_29((u_int)(ddata >> 32),
						    (u_int *)(dest + i));
						sta_29((u_int)ddata,
						    (u_int *)(dest + i + 4));
						break;
					case 0xa:
						sta_2a((u_int)(ddata >> 32),
						    (u_int *)(dest + i));
						sta_2a((u_int)ddata,
						    (u_int *)(dest + i + 4));
						break;
					case 0xb:
						sta_2b((u_int)(ddata >> 32),
						    (u_int *)(dest + i));
						sta_2b((u_int)ddata,
						    (u_int *)(dest + i + 4));
						break;
					default:
						/*
						 * non-Sbus xfer
						 * do nothing.
						 */
						break;
					}

				}
			}

			src += BLOCK_SIZE;
			dest += BLOCK_SIZE;
			blks--;

			if (xdb_cc_ipr_get() >= ipr_min) {
				break;
			}
		}

		while (SIGN_CHECK(xdb_cc_sdar_get()) == 0) { }
		enable_traps(psr);
	}
}

#endif	/* IOC_DW_BUG */

/*
 * Tunable parameters:
 *
 * hwbcopy -- size threshold a transfer must meet before we consider
 * using the MXCC block copy hardware.  units are BLOCK_SIZE (64 bytes)
 *
 * hwbcopy is initialized to defhwbcopy at boot time in hwbc_init().
 * hwbcopy must be zero before that point,
 * and defhwbcopy is unused after that point.
 *
 * to disable hardware bcopy:
 * in /etc/system:
 * set defhwbcopy=0
 *
 * or on a running system, you can set hwbcopy to 0.
 *
 * For copies to or from I/O space, we use any non zero hwbcopy value
 * to indicate that we can use the hardware for count >= BLOCK_SIZE.
 */
int	defhwbcopy =   	16;	/* (16 * 64) = 4096 */
int	hwbcopy =	0;

#ifdef BCSTAT
bcopy_stat_t bcstat;
int bcstaton = 0x0;
#endif

/*
 * ----------------------------------------------------------------------
 *
 * bcopy() divides the area to copy into 3 logical address sections:
 *
 * 1. "from" up to beginning of a BLOCK_SIZE boundary.
 *    The size of this region is >= 0 && < BLOCK_SIZE.
 * 2. 1 or more BLOCK_SIZE blocks - blks;
 * 3. Remaining bytes up to "from" + "count".  Again, the size of this region
 *    is >= 0 && < BLOCK_SIZE.
 *
 * "from" side:					"to" side:
 * ---- from					---- to
 * .						.
 * .						.
 * ----		first BLOCK_SIZE byte boundary	----
 * .						.
 * .						.
 * .						.
 * .						.
 * .						.
 * ----		end of last BLOCK_SIZE byte boundary	----
 * .						.
 * .						.
 * ---- end					---- end
 *
 *
 */

#define	ISIO(upte)	(!(upte.pte.Cacheable))

static int
get_hwbc_burst(union ptes pte)
{
	int sbus_id, slot_id;
	extern struct sbus_private sbus_pri_data[];

	/* Sbus id is at PA<33:30> */
	sbus_id = (pte.pte.PhysicalPageNumber >> 18) & 0xf;
	ASSERT((sbus_id >= 0) && (sbus_id < MX_SBUS));

	/* slot id is at PA<29:28> */
	slot_id = (pte.pte.PhysicalPageNumber >> 16) & 0x3;

	return (sbus_pri_data[sbus_id].burst_size[slot_id] & HWBC_BURST_MASK);
}

#ifdef DEBUG
static u_int xxnoburst = 0;
static u_int xxto_io = 0;
static u_int xxfrom_io = 0;
static u_int xxno_pte = 0;
static u_int bz_l1, bz_l2,  bz_l3;
static u_int bc_sl1, bc_sl2, bc_sl3;
static u_int bc_dl1, bc_dl2, bc_dl3;
static u_int sv_dptefind, sv_sptefind;
#endif DEBUG

void
bcopy(const void *from_arg, void *to_arg, size_t count)
{
	register int resid;
	union ptes to_pte, from_pte;
	u_int to_io;
	u_int from_io;
#ifdef IOC_DW_BUG
	u_int do_ioc_bugfix = 0;
#endif /* IOC_DW_BUG */
	u_int get_pte_fast();
	u_int spgsize, spgoffmask;
	u_int dpgsize, dpgoffmask;
	int slevel, dlevel;
	caddr_t snxp, dnxp;
	caddr_t from = (caddr_t)from_arg;
	caddr_t to = to_arg;

	if (count == 0) 	/* Yes, many call bcopy() with count==0 */
		return;

	if ((to_pte.pte_int = get_pte_fast(to)) == 0) {
		/*
		 * MMU probe failed even after we touched it. someone stole
		 * our page table. unlikely, but ...
		 */
#ifdef DEBUG
		xxno_pte++;
#endif DEBUG
		goto swcopy;
	}

	to_io = ISIO(to_pte);

#ifdef DEBUG
	if (to_io)
		xxto_io++;
#endif DEBUG

#ifdef IOC_DW_BUG
	/*
	 * if (ioc_dw_bug) probe if destination is to IO space.
	 * must set do_ioc_bugfix before we goto swcopy
	 */
	if (ioc_dw_bug)
		do_ioc_bugfix = to_io;
#endif	/* IOC_DW_BUG */

	BC_STAT_SMALLCALLER(count);

	/*
	 * common cases first
	 */
	if (count < BLOCK_SIZE)
		goto swcopy;

	/*
	 * "from" and "to" must be aligned on BLOCK_SIZE byte boundaries
	 */
	if (((u_int)from ^ (u_int)to) & BLOCK_MASK)
		goto swcopy;

	/*
	 * hwbcopy == 0 only during boot
	 */
	if (hwbcopy <= 0)
		goto swcopy;

	if ((from_pte.pte_int = get_pte_fast(from)) == 0) {
#ifdef DEBUG
		xxno_pte++;
#endif DEBUG
		/*
		 * MMU probe failed even after we touched it. someone stole
		 * our page table. unlikely, but ...
		 */
		goto swcopy;
	}

	from_io = ISIO(from_pte);

#ifdef DEBUG
	if (from_io)
		xxfrom_io++;
#endif DEBUG

	/*
	 * if this is a memory to memory copy.
	 * (ie. not to or from IO space) and
	 * && (count too small)
	 * then bcopy_asm().
	 */
	if (!to_io && (count < (hwbcopy << BLOCK_SIZE_SHIFT)) && !from_io) {
			bcopy_asm(from, to, count);
			return;
	}

	/*
	 * Verify that neither the source nor the destination
	 * is a dumb card that does not support burst i/o.
	 */
	if ((to_io && !get_hwbc_burst(to_pte)) ||
	    (from_io && !get_hwbc_burst(from_pte))) {

#ifdef DEBUG
		xxnoburst++;
#endif DEBUG

		goto swcopy;
	}

	/*
	 * Copy part 1
	 */
	resid = (u_int)from & BLOCK_MASK;
	if (resid) {
		resid = BLOCK_SIZE - resid;
		count -= resid;
#ifdef IOC_DW_BUG
		if (do_ioc_bugfix)
			bcopy_asm_toio(from, to, resid);
		else
#endif /* IOC_DW_BUG */
			bcopy_asm(from, to, resid);

		from += resid;
		to += resid;
	}

	/*
	 * Copy section 2
	 */
	resid = count & ~BLOCK_MASK;	/* bytes to copy in part 2 */
	count &= BLOCK_MASK;		/* calculate remainder for part 3 */

	snxp = dnxp = 0;

	while (resid > 0) {
		register int bcnt;
		pa_t phys_saddr, phys_daddr;
		union ptpe ptpe;

		if (from >= snxp) {
			if (!srmmu_xlate(-1, from, &phys_saddr, &ptpe,
			    &slevel)) {
				/*
				 * Oh well, we're not mapped.  Sorry.
				 */
				count += resid;
				goto swcopy;
			}

			switch (slevel) {
			case 3:
#ifdef DEBUG
				bc_sl3++;
#endif
				spgsize = MMU_L3_SIZE;
				spgoffmask = MMU_L3_SIZE - 1;
				snxp = BC_ROUNDUP2(from, MMU_L3_SIZE);
				break;

			case 2:
#ifdef DEBUG
				bc_sl2++;
#endif
				spgsize = MMU_L2_SIZE;
				spgoffmask = MMU_L2_SIZE - 1;
				snxp = BC_ROUNDUP2(from, MMU_L2_SIZE);
				break;

			case 1:
#ifdef DEBUG
				bc_sl1++;
#endif
				spgsize = MMU_L1_SIZE;
				spgoffmask = MMU_L1_SIZE - 1;
				snxp = BC_ROUNDUP2(from, MMU_L1_SIZE);
				break;

			default:
				cmn_err(CE_PANIC,
				    "bcopy: bad source pte level %d", slevel);

			}

			phys_saddr |= ((pa_t)ptpe.pte.Cacheable <<
				BC_CACHE_SHIFT);
		} else {
#ifdef DEBUG
			sv_sptefind++;
#endif
			/* LINTED lint confusion: used before set: bcnt */
			phys_saddr += ((pa_t)bcnt);
		}


		if (to >= dnxp) {
			if (!srmmu_xlate(-1, to, &phys_daddr, &ptpe, &dlevel)) {
				count += resid;
				goto swcopy;
			}

			switch (dlevel) {
			case 3:
#ifdef DEBUG
				bc_dl3++;
#endif
				dpgsize = MMU_L3_SIZE;
				dpgoffmask = MMU_L3_SIZE - 1;
				dnxp = BC_ROUNDUP2(to, MMU_L3_SIZE);
				break;

			case 2:
#ifdef DEBUG
				bc_dl2++;
#endif
				dpgsize = MMU_L2_SIZE;
				dpgoffmask = MMU_L2_SIZE - 1;
				dnxp = BC_ROUNDUP2(to, MMU_L1_SIZE);
				break;

			case 1:
#ifdef DEBUG
				bc_dl1++;
#endif
				dpgsize = MMU_L1_SIZE;
				dpgoffmask = MMU_L1_SIZE - 1;
				dnxp = BC_ROUNDUP2(to, MMU_L1_SIZE);
				break;

			default:
				cmn_err(CE_PANIC,
				    "bcopy: bad dest pte level %d", dlevel);

			}
			phys_daddr |= ((pa_t)ptpe.pte.Cacheable <<
				BC_CACHE_SHIFT);
		} else {
#ifdef DEBUG
			sv_dptefind++;
#endif
			phys_daddr += ((pa_t)bcnt);
		}

		bcnt = MIN((spgsize - ((u_int) from & spgoffmask)),
				(dpgsize - ((u_int) to & dpgoffmask)));
		if (resid < bcnt)
			bcnt = resid;

		/*
		 * if to IO, then we don't have to mark
		 * the dest page with R/M bits.
		 */
		if (to_io) {
#ifdef IOC_DW_BUG
			if (do_ioc_bugfix)
				hwbc_copy_toio((bcnt >> BLOCK_SIZE_SHIFT),
					phys_saddr, phys_daddr);
			else
#endif IOC_DW_BUG
				hwbc_copy((bcnt >> BLOCK_SIZE_SHIFT),
					phys_saddr, phys_daddr);
		} else {
			/*
			 * touch dest before & after to set R/M bits
			 * xxx is before necessary?
			 */
			*to = *from;
			hwbc_copy((bcnt >> BLOCK_SIZE_SHIFT),
				phys_saddr, phys_daddr);
			*to = *from;
		}

		from += bcnt;
		to += bcnt;
		resid -= bcnt;
	}

	if (count)	/* copy part 3 */
	{
swcopy:
#ifdef IOC_DW_BUG
		if (do_ioc_bugfix)
			bcopy_asm_toio(from, to, count);
		else
#endif IOC_DW_BUG
			bcopy_asm(from, to, count);
	}
}

void
bzero(void *addr_arg, size_t count)
{
	register int resid;
	union ptes to_pte;
	u_int get_pte_fast();
	caddr_t addr = addr_arg;

	if (hwbcopy <= 0)
		goto swzero;

	BC_STAT_SMALLCALLER(count);

	if (count < (hwbcopy << BLOCK_SIZE_SHIFT))
		goto swzero;

	if ((to_pte.pte_int = get_pte_fast(addr)) == 0) {
		/*
		 * MMU probe failed even after we touched it. someone stole
		 * our page table. unlikely, but ...
		 */
		goto swzero;
	}

	/*
	 * Verify that the destination is a dump card that
	 * does not support burst i/o.
	 */
	if (ISIO(to_pte) && !get_hwbc_burst(to_pte)) {
		goto swzero;
	}

	/*
	 * zero part 1
	*/

	resid = (u_int)addr & BLOCK_MASK;
	if (resid) {
		resid = BLOCK_SIZE - resid;
		count -= resid;
		bzero_asm(addr, resid);
		addr += resid;
	}

	resid = count & ~BLOCK_MASK;	/* bytes to zero in part 2 */
	count &= BLOCK_MASK;		/* calculate remainder for part 3 */

	/*
	 * Section 2
	 */
	while (resid > 0) {
		register u_int bcnt;
		register caddr_t nxp;
		pa_t phys_addr;
		union ptpe ptpe;
		int pgsize, level;

		if (!srmmu_xlate(-1, addr, &phys_addr, &ptpe, &level)) {
			count += resid;
			goto swzero;
		}
		phys_addr |= ((pa_t)ptpe.pte.Cacheable << BC_CACHE_SHIFT);

		switch (level) {
		case 3:
#ifdef DEBUG
			bz_l3++;
#endif
			pgsize = MMU_L3_SIZE;
			break;

		case 2:
#ifdef DEBUG
			bz_l2++;
#endif
			pgsize = MMU_L2_SIZE;
			break;

		case 1:
#ifdef DEBUG
			bz_l1++;
#endif
			pgsize = MMU_L1_SIZE;
			break;

		default:
			cmn_err(CE_PANIC, "bzero: bad pte level %d", level);

		}

		nxp = BC_ROUNDUP2(addr, pgsize);
		bcnt = MIN(resid, (nxp - addr));
		/*
		 * Mark this page as Referenced and Modified and call
		 * the HW routine;
		 */
		*addr = 0;
		hwbc_zero((bcnt >> BLOCK_SIZE_SHIFT), phys_addr);
		*addr = 0;

		addr += bcnt;
		resid -= bcnt;
	}

	if (count)
swzero:
		bzero_asm(addr, count);
}

#ifdef BCSTAT
void
insertcaller(callerl_t *clist, caddr_t callerp)
{
	u_int ndx = (u_int) callerp % CALLERLSIZE;

	if (clist[ndx].caller == NULL)
		clist[ndx].caller = callerp;
	else if (clist[ndx].caller != callerp) {
		for (ndx = 0; ndx < CALLERLSIZE; ndx++) {
			if (clist[ndx].caller == NULL) {
				clist[ndx].caller = callerp;
				break;
			}
			if (clist[ndx].caller == callerp)
				break;
		}
	}
	if (ndx == CALLERLSIZE)
		return;			/* no room for this entry */
	clist[ndx].count++;
}



void
bc_statsize(histo_t *histop, int count)
{
	int ndx = count / PAGESIZE;
	if (ndx > 3)
		ndx = 3;
	mutex_enter(&bcstat.lock);
	histop->bucket[ndx]++;
	if (count < PAGESIZE) {
		ndx = count / BLOCK_SIZE;
		if (ndx > 15)
			ndx = 15;
		histop->sbucket[ndx]++;
	}
	mutex_exit(&bcstat.lock);
}

void
bc_stattype(caddr_t src, caddr_t dest, int count, int destonly)
{
	union ptpe ptpe;

	if (srmmu_xlate(-1, dest, NULL, &ptpe, NULL))
	    bc_statsize(ptpe.pte.Cacheable ? &bcstat.dest_mem : &bcstat.dest_io,
		count);

	if (destonly)	/* for bzero */
		return;
	if (srmmu_xlate(-1, src, NULL, &ptpe, NULL))
	    bc_statsize(ptpe.pte.Cacheable ? &bcstat.src_mem : &bcstat.src_io,
		count);
}

#endif /* BCSTAT */


#ifdef HWBCOPY_DEBUG

int ldebug = 0;
#define	dprint if (ldebug) prom_printf

#include <sys/kmem.h>

static u_int
hwbc_mem_pfn(caddr_t p)
{
	u_int cacheable = (1 << (36 - MMU_STD_FIRSTSHIFT));
	u_int pfn = cacheable | hat_getkpfnum(p);
	/* assume the page is locked down! */
	return (pfn);
}

static void
hwbc_selftest(void)
{
	u_int one_page = BLOCKS_PER_PAGE * BLOCK_SIZE;
	caddr_t chunk = kmem_alloc(3 * one_page, KM_NOSLEEP);
	caddr_t from_buf = (caddr_t)
		(((u_int) chunk + one_page) & ~MMU_STD_FIRSTMASK);
	caddr_t to_buf = from_buf + one_page;
	u_int spfn = hwbc_mem_pfn(from_buf);
	u_int dpfn = hwbc_mem_pfn(to_buf);

	int i;

	for (i = 0; i < one_page; i++) {
		from_buf[i] = (i & 0xff);
	}

	cmn_err(CE_CONT, "?page_copy self-test: ");
	hwpage_copy(spfn, dpfn);
	cmn_err(CE_CONT, "?verifying... ");

	for (i = 0; i < one_page; i++) {
		u_char byte = to_buf[i];
		if (byte != (i & 0xff)) {
			debug_enter("page_copy bug?");
		}
	}

	cmn_err(CE_CONT, "?done\n");

	cmn_err(CE_CONT, "?page_zero self-test: ");
	hwpage_zero(dpfn);
	cmn_err(CE_CONT, "?verifying... ");

	for (i = 0; i < one_page; i++) {
		u_char byte = to_buf[i];
		if (byte != 0) {
			debug_enter("page_zero bug?");
		}
	}

	cmn_err(CE_CONT, "?done\n");

	cmn_err(CE_CONT, "?page_scan self-test: ");
	hwpage_scan(spfn);
	cmn_err(CE_CONT, "?done\n");

	kmem_free(chunk, 3 * one_page);
}


check_copy(caddr_t from, caddr_t to, int count, int lineno)
{
	caddr_t f, t;
	int c;
	int err = 0;

	for (f = from, t = to, c = count; c > 0; f++, t++, c--) {
		if (*f != *t) {
			if (++err < 20)
				dprint(
			"bcopy(%x, %x, %d): failure at %x -> %x diff %d\n",
				from, to, count, f, t, f - from);
		}
	}
	if (err)
		dprint("bcopy: %d total errors at line %d\n", err, lineno);
	if (err && ldebug > 1)
		debug_enter("stopping");
	return (err);
}

check_zero(caddr_t addr, int count, int lineno)
{
	caddr_t a;
	int c;
	int err = 0;

	for (a = addr, c = count; c > 0; a++, c--) {
		if (*a != 0) {
			if (++err < 20)
				dprint("bzero(%x, %d): failure at %x diff %d\n",
				addr, count, a, a - addr);
		}
	}
	if (err)
		dprint("bzero: %d total errors at line %d\n", err, lineno);
	if (err && ldebug > 1)
		debug_enter("bzero");
	return (err);
}
#endif	/* HWBCOPY_DEBUG */

#endif	/* USE_HW_BCOPY */
