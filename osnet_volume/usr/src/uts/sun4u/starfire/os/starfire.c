/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)starfire.c	1.21	99/10/22 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/machparam.h>
#include <sys/kobj.h>
#include <sys/mem_cage.h>
#include <sys/starfire.h>

#include <sys/platform_module.h>
#include <sys/errno.h>
#include <vm/page.h>
#include <vm/hat_sfmmu.h>
#include <sys/memnode.h>
#include <vm/vm_machdep.h>

/*
 * By default the DR Cage is disabled for maximum
 * OS performance.  Users wishing to utilize DR must
 * specifically turn on this variable via /etc/system.
 */
int	kernel_cage_enable;

/* Preallocation of spare tsb's for DR - none for now */
int starfire_tsb_spares = STARFIRE_MAX_BOARDS << 1;

/* Set the maximum number of boards... for DR */
int starfire_boards = STARFIRE_MAX_BOARDS;

/* Maximum number of cpus per board... for DR */
int starfire_cpu_per_board = 4;

/* Maximum number of mem-units per board... for DR */
int starfire_mem_per_board = 1;

/* Maximum number of io-units (buses) per board... for DR */
int starfire_io_per_board = 2;

/* Preferred minimum cage size (expressed in pages)... for DR */
pgcnt_t starfire_startup_cage_size = 0;

int
set_platform_max_ncpus(void)
{
	starfire_boards = MIN(starfire_boards, STARFIRE_MAX_BOARDS);

	if (starfire_boards < 1)
		starfire_boards = 1;

	return (starfire_boards * starfire_cpu_per_board);
}

int
set_platform_tsb_spares()
{
	return (MIN(starfire_tsb_spares, MAX_UPA));
}

void
set_platform_defaults(void)
{
	extern int ce_verbose;
	extern char *tod_module_name;
	extern int ts_dispatch_extended;

	uint32_t	revlevel;
	char		buf[20];

	ce_verbose = 1;		/* verbose ecc error correction */

	/*
	 * Check to see if we have the right firmware
	 * We simply do a prom_test to see if
	 * "SUNW,UE10000-prom-version" interface exist.
	 */
	if (prom_test("SUNW,UE10000-prom-version") != 0) {
		halt("Firmware upgrade is required to boot this OS!");
	} else {
		/*
		 * Versions 5 to 50 and 150 or above  can support this OS
		 */
		sprintf(buf, "cpu-prom-version swap l!");
		prom_interpret(buf, (uint32_t)&revlevel, 0, 0, 0, 0);
		if ((revlevel < 5) || ((revlevel > 50) && (revlevel < 150)))
			halt("Firmware upgrade is required to boot this OS!");
	}

	/* Set appropriate tod module for starfire */
	ASSERT(tod_module_name == NULL);
	tod_module_name = "todstarfire";

	/*
	 * Use the alternate TS dispatch table, which is better
	 * tuned for large servers.
	 */
	if (ts_dispatch_extended == -1) /* use platform default */
		ts_dispatch_extended = 1;
}

#ifdef DEBUG
pgcnt_t starfire_cage_size_limit;
#endif

void
set_platform_cage_params(void)
{
	extern pgcnt_t total_pages;
	extern struct memlist *phys_avail;
	int ret;

	if (kernel_cage_enable) {
		pgcnt_t preferred_cage_size;

		preferred_cage_size =
			MAX(starfire_startup_cage_size, total_pages / 256);

#ifdef DEBUG
		if (starfire_cage_size_limit)
			preferred_cage_size = starfire_cage_size_limit;
#endif
		kcage_range_lock();
		/*
		 * Note: we are assuming that post has load the
		 * whole show in to the high end of memory. Having
		 * taken this leap, we copy the whole of phys_avail
		 * the glist and arrange for the cage to grow
		 * downward (descending pfns).
		 */
		ret = kcage_range_init(phys_avail, 1);
		if (ret == 0)
			kcage_init(preferred_cage_size);
		kcage_range_unlock();
	}

	if (kcage_on)
		cmn_err(CE_NOTE, "!DR Kernel Cage is ENABLED");
	else
		cmn_err(CE_NOTE, "!DR Kernel Cage is DISABLED");
}

void
load_platform_drivers(void)
{
}

/*
 * Starfire does not support power control of CPUs from the OS.
 */
/*ARGSUSED*/
int
plat_cpu_poweron(struct cpu *cp)
{
	int (*starfire_cpu_poweron)(struct cpu *) = NULL;

	starfire_cpu_poweron =
		(int (*)(struct cpu *))kobj_getsymvalue("sfdr_cpu_poweron", 0);

	if (starfire_cpu_poweron == NULL)
		return (ENOTSUP);
	else
		return ((starfire_cpu_poweron)(cp));
}

/*ARGSUSED*/
int
plat_cpu_poweroff(struct cpu *cp)
{
	int (*starfire_cpu_poweroff)(struct cpu *) = NULL;

	starfire_cpu_poweroff =
		(int (*)(struct cpu *))kobj_getsymvalue("sfdr_cpu_poweroff", 0);

	if (starfire_cpu_poweroff == NULL)
		return (ENOTSUP);
	else
		return ((starfire_cpu_poweroff)(cp));
}

void
plat_dmv_params(uint_t *hwint, uint_t *swint)
{
	*hwint = STARFIRE_DMV_HWINT;
	*swint = 0;
}

/*
 * The following our currently private to Starfire DR
 */
int
plat_max_boards()
{
	return (starfire_boards);
}

int
plat_max_cpu_units_per_board()
{
	return (starfire_cpu_per_board);
}

int
plat_max_mem_units_per_board()
{
	return (starfire_mem_per_board);
}

int
plat_max_io_units_per_board()
{
	return (starfire_io_per_board);
}


/*
 * This function returns an index modulo the number of memory boards.
 * This index is used to associate a given pfn to a place on the freelist.
 * This results in dispersing pfn assignment over all the boards in the
 * system.
 */
static uint_t random_idx(int ubound);

#define	PFN_2_LBN(pfn)	(((pfn) >> (STARFIRE_MC_MEMBOARD_SHIFT - PAGESHIFT)) % \
			STARFIRE_MAX_BOARDS)

void
plat_freelist_process(int mnode)
{
	page_t		*page, **freelist;
	page_t		*bdlist[STARFIRE_MAX_BOARDS];
	page_t		 **sortlist[STARFIRE_MAX_BOARDS];
	uint32_t	idx, idy, size, color, max_color, lbn;
	uint32_t	bd_flags, bd_cnt, result, bds;
	kmutex_t	*pcm;
	int 		mtype;

	/* for each page size */
	for (mtype = 0; mtype < MAX_MEM_TYPES; mtype++) {
		for (size = 0; size < MMU_PAGE_SIZES; size++) {

			/*
			 * Compute the maximum # of phys colors based on
			 * page size.
			 */
			max_color = page_get_pagecolors(size);

			/* for each color */
			for (color = 0; color < max_color; color++) {

				bd_cnt = 0;
				bd_flags = 0;
				for (idx = 0; idx < STARFIRE_MAX_BOARDS;
						idx++) {
					bdlist[idx] = NULL;
					sortlist[idx] = NULL;
				}

				/* find freelist */
				freelist = &PAGE_FREELISTS(mnode, size,
				    color, mtype);

				if (*freelist == NULL)
					continue;

				/* acquire locks */
				pcm = PC_BIN_MUTEX(mnode, color, PG_FREE_LIST);
				mutex_enter(pcm);

				/*
				 * read freelist & sort pages by logical
				 * board number
				 */
				/* grab pages till last one. */
				while (*freelist) {
					page = *freelist;
					result = page_trylock(page, SE_EXCL);

					ASSERT(result);

					/* Delete from freelist */
					if (size != 0) {
						page_vpsub(freelist, page);
					} else {
						mach_page_sub(freelist, page);
					}

					/* detect the lbn */
					lbn = PFN_2_LBN((
					    (machpage_t *)page)->p_pagenum);

					/* add to bdlist[lbn] */
					if (size != 0) {
						page_vpadd(&bdlist[lbn], page);
					} else {
						mach_page_add(&bdlist[lbn],
						    page);
					}

					/* if lbn new */
					if ((bd_flags & (1 << lbn)) == 0) {
						bd_flags |= (1 << lbn);
						bd_cnt++;
					}
					page_unlock(page);
				}

				/*
				 * Make the sortlist so
				 * bd_cnt choices show up
				 */
				bds = 0;
				for (idx = 0; idx < STARFIRE_MAX_BOARDS;
						idx++) {
					if (bdlist[idx])
						sortlist[bds++] = &bdlist[idx];
				}

				/*
				 * now rebuild the freelist by shuffling
				 * pages from bd lists
				 */
				while (bd_cnt) {

					/*
					 * get "random" index between 0 &
					 * bd_cnt
					 */

					ASSERT(bd_cnt &&
					    (bd_cnt < STARFIRE_MAX_BOARDS+1));

					idx = random_idx(bd_cnt);

					page = *sortlist[idx];
					result = page_trylock(page, SE_EXCL);

					ASSERT(result);

					/* Delete from sort_list */
					/*  & Append to freelist */
					/* Big pages use vp_add - 8k don't */
					if (size != 0) {
						page_vpsub(sortlist[idx], page);
						page_vpadd(freelist, page);
					} else {
						mach_page_sub(sortlist[idx],
						    page);
						mach_page_add(freelist, page);
					}

					/* needed for indexing tmp lists */
					lbn = PFN_2_LBN((
						(machpage_t *)page)->p_pagenum);

					/*
					 * if this was the last page on this
					 * list?
					 */
					if (*sortlist[idx] == NULL) {

						/* have to find brd list */

						/* idx is lbn? -- No! */
						/* sortlist, brdlist */
						/*  have diff indexs */
						bd_flags &= ~(1 << lbn);
						--bd_cnt;

						/*
						 * redo the sortlist so only
						 * bd_cnt choices show up
						 */
						bds = 0;
						for (idy = 0;
						    idy < STARFIRE_MAX_BOARDS;
						    idy++) {
							if (bdlist[idy]) {
								sortlist[bds++]
								= &bdlist[idy];
							}
						}
					}
					page_unlock(page);
				}
				mutex_exit(pcm);
			}
		}
	}
}

/* These will return an int between 0 & ubound */
static uint_t
random_idx(int ubound)
{
	static int idx = 0;

	return (((idx++)%ubound));
}

/*
 * No platform pm drivers on this platform
 */
char *platform_pm_module_list[] = {
	(char *)0
};

/*ARGSUSED*/
void
plat_tod_fault(enum tod_fault_type tod_bad)
{
}
