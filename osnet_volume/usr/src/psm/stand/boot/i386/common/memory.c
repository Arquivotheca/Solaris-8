/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)memory.c	1.28	99/10/07 SMI"

/*
 *	i86pc memory routines
 *
 *	This file contains memory management routines to provide
 *	functionality found in proms on Sparc machines
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/promif.h>
#include <sys/bootconf.h>
#include <sys/bootdef.h>
#include <sys/booti386.h>
#include <sys/bootlink.h>
#include <sys/sysenvmt.h>
#include <sys/bootinfo.h>
#include <sys/mmu.h>
#include <sys/saio.h>
#include <sys/machine.h>
#include <sys/salib.h>
#include <sys/tss.h>

#define	ptalign(p)	((int *)((uint_t)(p) & ~(MMU_PAGESIZE-1)))

#ifdef BOOT_DEBUG
char *mem_types[] = 	{
			"installed",
			"available",
			"virtual",
			"shadow"
			};
#endif

caddr_t	top_realmem, bot_realmem, low_realmem;
struct	bootmem mem64avail[B_MAXMEM64];
int	mem64availcnt = 0;
int	p0_mapped = 0;


static struct freenode {
    struct freenode *next;
    size_t size;
} *free_realmem;

/*  These are the various memory lists in boot.c */
extern struct memlist 	*pfreelistp,	/* physmem available */
			*vfreelistp,	/* virtmem available */
			*pinstalledp;   /* physmem installed */

struct memlist  bvirtlist, vinstalled;	/* Some static memlists */

struct memlist	*bphyslistp,			/* phys memory used by boot */
		*vinstalledp = &vinstalled,	/* virtual memory installed */
		*bvirtlistp = &bvirtlist;	/* virt memory used by boot */


int setup_memlist(int type, struct bootmem *bmem, int cnt);
void page_on(void);

extern caddr_t caller();
extern struct sysenvmt sysenvmt, *sep;
extern struct bootinfo bootinfo, *bip;
extern struct bootenv bootenv, *btep;
extern void *memcpy(void *s1, void *s2, size_t n);
extern void *memset(void *s, int c, size_t n);
extern caddr_t resalloc(enum RESOURCES type, size_t bytes, caddr_t virthint,
    int align);

extern caddr_t memlistpage;
extern caddr_t magic_phys;
uint_t top_virtaddr;
extern struct int_pb ic;

extern void dbfault();
extern paddr_t map_mem(uint_t, uint_t, int);

#ifndef ASSERT
#define	ASSERT(x)
#endif

#define	ALIGN(x, a)	((a) == 0 ? (intptr_t)(x) : \
	(((intptr_t)(x) + (intptr_t)(a) - 1l) & ~((intptr_t)(a) - 1l)))

/*
 * These 2 TSS structures are part of double fault handling.  They
 * are defined here because the double fault TSS CR3 field is filled
 * in by init_paging() in this file.
 */
struct tss386 tss_normal;
struct tss386 tss_dblflt = {
	0,			/* t_link */
	SBOOT_INTSTACKLOC, B_GDT, /* t_esp0 and t_ss0 */
	0, 0, 0, 0,		/* t_esp1 through t_ss2 */
	0,			/* t_cr3 (will be filled in init_paging) */
	(unsigned long)dbfault,	/* t_eip */
	0,			/* t_eflags */
	0, 0, 0, 0,		/* t_eax through t_ebx */
	SBOOT_INTSTACKLOC,	/* t_esp */
	0, 0, 0,		/* t_ebp through t_edi */
	B_GDT, C_GDT, B_GDT,	/* t_es, t_cs, t_ss */
	B_GDT, B_GDT, B_GDT,	/* t_ds, t_fs, t_gs */
	0, 0			/* t_ldt and t_bitmapbase */
};

void
init_paging(void)
{
	extern void start_paging();
	ptbl_t	*ptp, *pdp;
	extern uint_t	bpt_loc, bpd_loc;
	uint_t	v_ref;
	uint_t	i;
	uint_t	pgs;
	uint_t	bpte;

	/* allocate room for boot page table directory */
	pdp = (ptbl_t *)resalloc(RES_BOOTSCRATCH, MMU_PAGESIZE, 0, 0);
	if (pdp == (ptbl_t *)0) {
		printf("No memory for boot page table directory\n");
		prom_panic("init_paging dir");
	} /* save it */
	tss_dblflt.t_cr3 = bpd_loc = (uint_t)pdp;

	/* allocate room for boot page table */
	ptp = (ptbl_t *)resalloc(RES_BOOTSCRATCH, MMU_PAGESIZE, 0, 0);
	if (ptp == (ptbl_t *)0) {
		printf("No memory for boot page table \n");
		prom_panic("init_paging table");
	} /* save it */
	bpt_loc = (uint_t)ptp;

	/* initialize the page table to zeroes */
	(void) memset(ptp, '\0', MMU_PAGESIZE);

	/* begins at 0 */
	v_ref = (uint_t)ptalign(0);

	/* each page table entry for this area is r/w */
	bpte = (0 & MMU_STD_PAGEMASK) | (MMU_PFEC_P| MMU_PFEC_WRITE);

	/*
	 * "pgs" gives number of pte's in boot memory area.  This must be
	 *  more than 1MB worth (total realmode memory) but can't be more
	 *  than 4MB since we've only allocated a single page directory.
	 */
	pgs = (uint_t)magic_phys/MMU_PAGESIZE;
	ASSERT((pgs >= 256) && (pgs <= 1024));

	/* skip the first page */
	bpte  += MMU_PAGESIZE;
	v_ref += MMU_PAGESIZE;

	/* for each page in the segment */
	for (i = 1; i < pgs; i++) {
		ptp->page[MMU_L2_INDEX(v_ref)] = bpte;
		bpte  += MMU_PAGESIZE;
		v_ref += MMU_PAGESIZE;
	}

	/* initialize the page directory to zeroes */
	(void) memset(pdp, '\0', MMU_PAGESIZE);

	/* First page directory entry holds address of the one page table */
	pdp->page[0] = ((uint_t)ptp) | (MMU_PFEC_P| MMU_PFEC_WRITE);

	start_paging();
	page_on();
}

uint_t
p0_setpte(uint_t bpte)
{
	/*
	 * bpt_loc is the address of the boot page table.  If it is
	 * null, assume that paging has not been set up yet and
	 * access is safe.
	 */
	uint_t answer = 0;
	extern uint_t bpt_loc;
	ptbl_t	*ptp;
	extern void invalidate(char *);

	if (bpt_loc && p0_mapped == 0) {
		ptp = (ptbl_t *)bpt_loc;
		answer = ptp->page[0];
		ptp->page[0] = bpte;
		invalidate(0);
	}
	return (answer);
}

/*
 * boot.bin normally runs with page 0 unmapped so that null-pointer
 * references fail.  p0_setpte would be very complex if we tried to
 * find the correct page table entries once the kernel has taken over.
 * So p0_mapin should be called before transferring control to the
 * kernel to map in page 0.  If the kernel uses PAE mode it will then
 * generate page tables with page 0 accessible.  After p0_mapin has
 * been called, the page will stay mapped in, even if peeks and pokes
 * are called.
 */
void
p0_mapin(void)
{
	(void) p0_setpte(MMU_PFEC_P | MMU_PFEC_WRITE);
	p0_mapped = 1;
}

unsigned short
peeks(unsigned short *addr)
{
	/*
	 * We want to look at a low address (in zero-page).  So we
	 * temporarily make the page read-only so that we can
	 * access the page without generating a page fault.
	 *
	 * Restore the original p0 settings rather than assuming
	 * that p0 was disabled.  This allows peeks to be called
	 * after calling p0_mapin() with no special test.
	 */
	ushort_t rv;
	uint_t bpte;

	bpte = p0_setpte(MMU_PFEC_P);

	rv = *addr;

	(void) p0_setpte(bpte);

	return (rv);
}

void
pokes(unsigned short *addr, unsigned short val)
{
	/*
	 * We want to set something at a low address (in zero-page).
	 * So we temporarily make the page writable so that we can
	 * access the page without generating a page fault.
	 *
	 * Restore the original p0 settings rather than assuming
	 * that p0 was disabled.  This allows pokes to be called
	 * after calling p0_mapin() with no special test.
	 */
	uint_t bpte;

	bpte = p0_setpte(MMU_PFEC_P | MMU_PFEC_WRITE);

	*addr = val;

	(void) p0_setpte(bpte);
}

unsigned char
peek8(unsigned char *addr)
{
	/* 8-bit version of peeks (see above) */
	unsigned char rv;
	uint_t bpte;

	bpte = p0_setpte(MMU_PFEC_P);

	rv = *addr;

	(void) p0_setpte(bpte);

	return (rv);
}

void
poke8(unsigned char *addr, unsigned char val)
{
	/* 8-bit version of pokes (see above) */
	uint_t bpte;

	bpte = p0_setpte(MMU_PFEC_P | MMU_PFEC_WRITE);

	*addr = val;

	(void) p0_setpte(bpte);
}

void
page_on(void)
{
	extern unsigned int cr0mask;
	cr0mask |= CR0_PG;
}


/*
 * The following routines handle the realmode memory heap.
 * Lots of debugging stuff can be compiled in by defining "RM_DEBUG".
 */

/* #define	RM_DEBUG	if defined, compile in debugging */
#ifdef RM_DEBUG
#define	RM_DBG(x) x;
#else
#define	RM_DBG(x)
#endif

void
rm_check()
{
	/*
	 * sanity test to make sure .bef drivers have
	 * not dropped into our realmem arena. This can be called
	 * at anytime such as handle21() or other SLB entries.
	 * Too useful to compile it out.
	 */
	static int rm_check_warn = 0;	/* XXX turn off for production! */
	static int rm_check_min = 10000; /* defines pending doom */
	static int cur_space;
#ifdef	RM_DEBUG
	static int rm_check_verbose = 1; /* shout when arena(s) grow */
	static caddr_t last_bot;
	static caddr_t last_top;
#endif
	extern short DOSsnarf_flag;
	short tmp_snarf;
	struct int_pb tmp_pb;

	tmp_snarf = DOSsnarf_flag;
	tmp_pb = ic;

	top_realmem = (caddr_t)(peeks((ushort_t *)MEMBASE_ADDR) *
							BOOTMEMBLKSIZE);
	cur_space = top_realmem - bot_realmem;

	if (cur_space < 0) {
		/*
		 * bad things, man
		 */
		printf(
	"\nrm_check: top_realmem = 0x%x bot_realmem = 0x%x overlap = %d\n",
			top_realmem, bot_realmem, -cur_space);
		prom_panic("memory soon to be hosed");
	}

	if (rm_check_warn && cur_space < rm_check_min) {
		printf("\nrm_check: WARNING! realmem down to %d\n", cur_space);
		rm_check_min = cur_space;
	}

#ifdef	RM_DEBUG
	if (!last_top) {
		/* first time through */
		last_top = top_realmem;
		last_bot = bot_realmem;
	}
	if (rm_check_verbose &&
		(bot_realmem != last_bot || top_realmem != last_top)) {
		printf(
"\nrm_check:                                  BOT 0x%x TOP 0x%x SPACE 0x%x\n",
		bot_realmem, top_realmem, cur_space);
	}
	last_bot = bot_realmem;
	last_top = top_realmem;
#endif
	ic = tmp_pb;
	DOSsnarf_flag = tmp_snarf;
}


#ifdef	RM_DEBUG
static int bverbose = 1;
static int bwait = 0;

static void
free_request(caddr_t who, caddr_t addr, size_t size)
{
	/*
	 * debug routine reporting requests to rm_free
	 */
	extern short DOSsnarf_flag;
	short tmp_snarf;
	struct int_pb tmp_pb;

	if (!bverbose)
		return;
	tmp_snarf = DOSsnarf_flag;
	tmp_pb = ic;
	printf("0x%x free( 0x%x, %d ) ", who, addr, size);
	ic = tmp_pb;
	DOSsnarf_flag = tmp_snarf;
}

static void
free_dbg(char *info, int wait)
{
	/*
	 * debug routine reporting results from rm_free
	 */
	extern short DOSsnarf_flag;
	short tmp_snarf;
	struct int_pb tmp_pb;

	if (!bverbose)
		return;
	tmp_snarf = DOSsnarf_flag;
	tmp_pb = ic;
	if (!wait) {
		printf(" %s ", info);
	} else  {
		printf("--more--");
		if (bwait)
			(void) getchar();
		printf("\n");
	}
	ic = tmp_pb;
	DOSsnarf_flag = tmp_snarf;
}

void
alloc_request(caddr_t who, size_t size, unsigned align, caddr_t addr)
{
	/*
	 * debug routine reporting requests to rm_alloc
	 */
	extern short DOSsnarf_flag;
	short tmp_snarf;
	struct int_pb tmp_pb;

	if (!bverbose)
		return;
	tmp_snarf = DOSsnarf_flag;
	tmp_pb = ic;
	printf("\n0x%x alloc( %d, %x, 0x%x ) ", who, size, align, addr);
	ic = tmp_pb;
	DOSsnarf_flag = tmp_snarf;
}
void
alloc_dbg(caddr_t vad, char *where)
{
	/*
	 * debug routine reporting results from rm_alloc
	 */
	extern short DOSsnarf_flag;
	short tmp_snarf;
	struct int_pb tmp_pb;

	if (!bverbose)
		return;
	tmp_snarf = DOSsnarf_flag;
	tmp_pb = ic;
	printf("--> 0x%x %s --more--", vad, where);
	if (bwait)
		(void) getchar();
	printf("\n");
	ic = tmp_pb;
	DOSsnarf_flag = tmp_snarf;
}
#endif	/* RM_DEBUG */

void
rm_free(caddr_t virt, size_t size)
{
	/*
	 *  Free Realmode Memory:
	 *
	 *  This routine frees up "size" bytes of low-core memory beginning
	 *  at address "virt".  This is done by initializing a "freenode"
	 *  struct at the front of the block to be freed and chaining it
	 *  into the list of free realmode memory headed at "free_realmem",
	 *  collapsing any existing blocks that may be immediately adjacent
	 *  to it.
	 */

	rm_check();
	RM_DBG(free_request(caller(), virt, size))
	if (virt < low_realmem || virt+size > bot_realmem) {
		printf("\nrm_free: out of range addr 0x%x size %d from 0x%x\n",
			virt, size, caller());
		return;
	}

	size = roundup(size, PARASIZE);
	if (size >= PARASIZE) {
		struct freenode *fp = (struct freenode *)&free_realmem;

		while (fp->next && (virt > (caddr_t)fp->next)) {
			/*
			 *  Find new free block's position in the free_realmem
			 *  list.  We keep this list in physical address order.
			 */

			fp = fp->next;
			if (((caddr_t)fp + fp->size) > virt) {
				printf("rm_free: invalid block 1\n");
				return;
			}

			if (((caddr_t)fp + fp->size) == virt) {
				/*
				 *  Newly freed block fits immediately behind
				 *  an existing free block.  Bump existing
				 *  block's size to include this block.
				 */

				if (fp->next &&
				    (((caddr_t)fp+fp->size+size) >
					(caddr_t)fp->next)) {
					printf("rm_free: invalid block 2\n");
					return;
				}
				RM_DBG(free_dbg("APPEND", 0))
				fp->size += size;
				size = 0;
				break;
			}
		}

		if ((((struct freenode *)virt)->size = size) != 0) {
			/*
			 *  Insert new free block behind the node at "fp".
			 *  Then make this the target node for possible col-
			 *  lapsing.
			 */

			RM_DBG(free_dbg("INSERT", 0))
			((struct freenode *)virt)->next = fp->next;
			fp->next = (struct freenode *)virt;
			fp = (struct freenode *)virt;
		}

		if (fp->next && ((caddr_t)fp+fp->size) == (caddr_t)fp->next) {
			/*
			 *  An existing free block lies immediately beyond the
			 *  end of the new free block.  Collapse these blocks
			 *  into one.
			 */

			RM_DBG(free_dbg("PREPEND", 0))
			fp->size += fp->next->size;
			fp->next = fp->next->next;
		}
	}
	RM_DBG(free_dbg(0, 1))
}


caddr_t
rm_malloc(size_t size, uint_t align, caddr_t virt)
{
	/* BEGIN CSTYLED */
	/*
	 *  Allocate Realmode Memory:
	 *
	 *     This routine allocates available memory below the magic 1MB
	 *     boundary.  This is a rather precious resource on ix86 machines
	 *     as the processor can only access the first megabyte when running
	 *     in "real mode" (I guess Intel doesn't consider machines with
	 *     more than 1MB of memory to be real!).
	 *
	 *     Arguments are more-or-less standard, except for "virt".  Whereas
	 *     other allocators interpret this as VIRTUAL address to which a
	 *     dynamically allocated chunk of memory is to be mapped, rm_malloc
	 *     interprets it as the PHYSICAL address we want to allocate.  This
	 *     allows us to load realmode programs (e.g, DOS device drivers) at
	 *     specific locations.
	 *
	 *     We maintain two realmode memory pointers:
	 *
	 *         bot_realmem:  Points to the first available byte of realmode
	 *                       memory.  This starts just beyond the end of the
	 *                       bootblock itself (i.e, at "_end").
	 *
	 *         top_realmem:  Points just beyond the last byte of available
	 *                       realmode memory.  This starts at 1MB.
	 *
	 *     Whenever possible, we allocate from the list of previously freed
	 *     memory.  This saves the higher locations for use by the ".bef"
	 *     modules.
	 */
	/* END CSTYLED */

	caddr_t vad;
	struct freenode *fp, *hp;
	int big_enough, growable;

	ASSERT(!(align && (align & (align-1)))); /* Must be a power of 2! */
	ASSERT(memlistpage != (caddr_t)0);	/* Must be initialized!	 */
	ASSERT(virt < (caddr_t)USER_START);	/* Must be in low MByte	 */

	rm_check();
	RM_DBG(alloc_request(caller(), size, align, virt))
	if (!low_realmem)
		low_realmem = bot_realmem;	/* lowest ever address */

	if (align < PARASIZE)
		align = PARASIZE;	/* the chunks we deal with */

	if (!size) {
		RM_DBG(alloc_dbg(0, "FAIL"))
		return (0);
	}
	size = roundup(size, PARASIZE);
	top_realmem = (caddr_t)(peeks((ushort_t *)MEMBASE_ADDR) *
							BOOTMEMBLKSIZE);

	for (hp = (struct freenode *)&free_realmem;
	    (fp = hp->next) != 0; hp = fp) {
		/*
		 *  Search the free_realmem list looking for a free block
		 *  that can be used to satisfy the caller's request.  This
		 *  list is maintained in physical address sequence.
		 */

		/*
		 * Calculate the address to look for.
		 */
		if ((vad = virt) != 0) {
			/*
			 * User asked for a particular address and we
			 * just passed it w/o finding a block.
			 */
			if (virt < (caddr_t)fp)
				return (0);
		} else {
			/*
			 * Caller doesn't need a particular address,
			 * assume this block is okay for now.
			 */
			vad = (caddr_t)roundup((paddr_t)fp, align);
		}

		/*
		 * Now see if can use the current free block. We can
		 * use it if it is large enough or if it is adjacent
		 * to the top of the arena and we can grow it.
		 */

		big_enough = ((vad >= (caddr_t)fp) &&
				((vad+size) <= ((caddr_t)fp + fp->size)));
		growable = ((fp->next == 0) &&
				((caddr_t)fp+fp->size == bot_realmem) &&
				((vad+size) <= top_realmem));

		if (big_enough || growable) {

			/*
			 *  Caller's request can be satisfied from the freelist
			 *  node at "*fp".  Adjust this node's size and replace
			 *  it with another node(s) if neccessary.
			 */

			size_t resid;

			if (!big_enough) {
				/* grow the arena and this free block */
				bot_realmem = vad+size;
				fp->size = bot_realmem - (caddr_t)fp;
			}

			resid = fp->size;

			if (vad != (caddr_t)fp) {
				/*
				 *  We skipped some padding bytes at the front
				 *  of the freelist node.  Adjust node's size
				 *  to match this padding.
				 */

				fp->size = (vad - (caddr_t)fp);
				resid -= fp->size;
				hp = fp;

			} else {
				/*
				 *  Remove this node from the freelist chain.
				 */

				hp->next = fp->next;
			}

			if ((resid -= size) > 0) {
				/*
				 *  There's some unused memory at the back end
				 *  of the freelist node.  Add it to the
				 *  realmode free list.
				 */

				fp = (struct freenode *)(vad+size);
				fp->next = hp->next;
				fp->size = resid;
				hp->next = fp;
			}

			(void) memset(vad, '\0', size);
			RM_DBG(alloc_dbg(vad, big_enough ?
				"FREELIST" : "EXTEND LAST CHUNK"))
			return ((caddr_t)vad);
		}
	}

	/*
	 * Nothing suitable on the freelist, try to expand the arena.
	 */
	vad = (caddr_t)roundup((paddr_t)(virt ? virt : bot_realmem), align);

	if ((vad >= bot_realmem) && ((vad+size) <= top_realmem)) {
		/*
		 *  We can satisfy this request with previously unallocated
		 *  realmode memory.  Figure out which end of the realmode
		 *  arena we're working on.
		 */

		if ((vad+size) == top_realmem) {
			/*
			 *  Caller is allocating from the 1MB boundary down;
			 *  reset the top indicator.  Also reset the bios
			 *  high memory indicator for use by .BEF files.
			 */
			uint_t v = (uint_t)vad;

			top_realmem = (caddr_t)(v & ~(BOOTMEMBLKSIZE-1));
			pokes((ushort_t *)MEMBASE_ADDR,
				(ushort_t)(v/BOOTMEMBLKSIZE));
			rm_free(top_realmem, (vad - top_realmem));

		} else {
			/*
			 *  Caller is allocating from the bootblock up; reset
			 *  the bottom pointer and "rm_free" any padding that
			 *  we may have inserted.
			 */
			caddr_t tbot;

			tbot = bot_realmem;
			bot_realmem = vad + size;
			rm_free(tbot, (vad - tbot));
		}

		(void) memset(vad, '\0', size);
		RM_DBG(alloc_dbg(vad, "NEW CHUNK"))
		return ((caddr_t)vad);
	}

	return (0);
}

int
rm_resize(caddr_t vad, size_t oldsz, size_t newsz)
{
	/*
	 *  Pseudo realloc function:
	 *
	 *  This routine may be used to resize a block of memory allocated by
	 *  rm_malloc.  If the old size is greater than the new size, memory
	 *  at the end of the block is returned to the free pool.  If the new
	 *  size is greater than the old, fresh memory is appended to the end
	 *  of the block and cleared to zeros.
	 *
	 *  Returns 0 if it works, -1 if there's no room behind the current
	 *  block to allow extension.
	 */

	int dif;
	caddr_t oad, nad;
	struct freenode *fp, *hp = (struct freenode *)&free_realmem;

	oad = vad + (oldsz = roundup(oldsz, PARASIZE));
	nad = vad + (newsz = roundup(newsz, PARASIZE));

	if ((dif = newsz - oldsz) <= 0) {
		/*
		 *  Caller is shrinking the block.  Check to see if the size
		 *  difference is enough to be worth worrying about!
		 */

		if ((dif = -dif) >= PARASIZE) {
			/*
			 *  New area is at least 16 bytes smaller than the old
			 *  one.  Free up memory at end of this block.
			 */

			rm_free(nad, dif);
		}

		return (0);

	} else if (oad == bot_realmem) {
		/*
		 *  The block to be resized is at the lower limit of un-
		 *  allocated memory.  Find top of unallocated memory and
		 *  see if there's enough space between bottom & top to
		 *  satisfy resize request.
		 */

		top_realmem = (caddr_t)(peeks((ushort_t *)MEMBASE_ADDR) *
		    BOOTMEMBLKSIZE);

		if (nad <= top_realmem) {
			/*
			 *  There's room enough here.  Fix real mem bottom
			 *  marker to point to new lower limit.
			 */

			(void) memset(oad, 0, dif);
			bot_realmem = nad;
			return (0);
		}

	} else while (((fp = hp->next) != 0) && (oad >= (caddr_t)fp)) {
		/*
		 *  We're forced to search the free list!  Look for the entry
		 *  that sits just beyound the current end of the block we're
		 *  trying to resize.
		 */

		if ((oad == (caddr_t)fp) && (fp->size >= dif)) {
			/*
			 *  Well, what do you know!  There's actually a free
			 *  block immediately behind the caller's block and
			 *  it's big enough to allow the requested extension.
			 */

			if ((fp->size -= dif) >= PARASIZE) {
				/*
				 *  There will even be some space left over in
				 *  the adjacent free block after we've re-
				 *  sized the current block!
				 */

				hp->next = (struct freenode *)((caddr_t)fp+dif);
				hp->next->next = fp->next;
				hp->next->size = fp->size;

			} else {
				/*
				 *  Resizing the current block will consume
				 *  all of the free space behind it.
				 */

				hp->next = fp->next;
			}

			(void) memset(fp, 0, dif);
			return (0);
		}

		hp = fp;	/* Check next free pool entry		*/
	}

	return (-1);
}

/*
 * Find largest available low core block
 */
long
rm_maxblock()
{
	struct freenode *fp, *hp;
	long maxblock = 0;
	caddr_t top_availmem;

	hp = (struct freenode *)0;
	/*
	 * Check real memory free block list
	 */
	for (fp = free_realmem; fp != 0; fp = fp->next) {
		if (fp->size > maxblock)
			maxblock = fp->size;
		hp = fp;
	}
	if (top_realmem - bot_realmem > 0x1000)
		top_availmem = top_realmem - 0x1000;
	else
		top_availmem = bot_realmem;
	/*
	 * Check against free real memory size too.
	 * Leave 4k free for emergencies though.
	 */
	if (hp && ((caddr_t)hp + hp->size) == bot_realmem) {
		if ((top_availmem - (caddr_t)hp) > maxblock)
			maxblock = top_availmem - (caddr_t)hp;
	} else {
		if ((top_availmem - bot_realmem) > maxblock)
			maxblock = top_availmem - bot_realmem;
	}
	return (maxblock);
}

struct claimed {		/* Records low-core "claimed" by boot	*/

	struct claimed *next;	/* .. Next "claimed" record		*/
	caddr_t addr;		/* .. Address of this block		*/
	int len;		/* .. Length of this block		*/
};

static struct claimed *claim_list = 0;

static int
get_mem_args(int argc, char **argv, caddr_t *ap, int *lp, char *cmd)
{
	/*
	 *  Parse memory command argument:
	 *
	 *  The boot interpreter's P1275-like memory commands, "claim" and
	 *  "release" each take two arguments:  A physical address and a byte
	 *  count.  This routine extracts the arguments from the "argv" list,
	 *  verifies them, and returns the respective values in the "ap" and
	 *  "lp" locations.  It returns zero (after printing an error message)
	 *  if there's an error.
	 */

	char *cp;
	int len = 0;
	extern long strtol(char *, char **, int);
	*ap = (caddr_t)strtol(argv[1], &cp, 0);

	if (*cp) {
		/*
		 *  "strtol" was unable to parse the address argument.  Print
		 *  error message and return.
		 */

		printf("%s: invalid address\n", cmd);
		return (0);
	}

	if (argc == 3) {
		/*
		 *  Don't bother with the length argument if it's not there!
		 */

		if (((len = *lp = (int)strtol(argv[2], &cp, 0)) < 0) || *cp) {
			/*
			 *  Couldn't parse the length argument.  Print error
			 *  message and return.
			 */

			printf("%s: invalid length\n", cmd);
			return (0);
		}
	}

	if ((*ap + len) > (caddr_t)USER_START) {
		/*
		 *  The "claim" and "release" commands only work on memory
		 *  residing below the magic 1MB boundary.
		 */

		printf("%s: invalid address range\n", cmd);
		return (0);
	}

	return (1);
}

void
claim_cmd(int argc, char **argv)
{
	/*
	 *  Interpreter command used to reserve memory (to support brain-dead
	 *  DOS code that makes unwarrented assumptions about how low memory
	 *  is used).
	 */

	struct claimed *cp;	/* Ptr to claim buffer */
	caddr_t addr;		/* Address we're claiming */
	int len;		/* Length of memory to claim */

	if (argc != 3) {
		/*
		 *  The "claim" command takes two arguments.  Make sure caller
		 *  provides them.
		 */

		printf("usage: claim address length\n");

	} else if (get_mem_args(argc, argv, &addr, &len, "claim")) {
		/*
		 *  The arguments look good, see if we can allocate the memory.
		 */

		if (!rm_malloc(len, 0, (caddr_t)addr)) {
			/*
			 *  Rm_malloc failed, which means the memory is already
			 *  allocated.
			 */

			printf("claim: memory already allocated\n");
			return;
		}

		if (!(cp = (struct claimed *)
				    bkmem_alloc(sizeof (struct claimed)))) {
			/*
			 *  This is odd:  We can allocate low-memory but are
			 *  unable to allocate from the much larger arena above
			 *  the 1MB boundary!
			 */

			printf("claim: warning, memory cannot be released\n");

		} else {
			/*
			 *  Successful memory reservation.  Record the memory
			 *  we just claimed in the claim list.
			 */

			cp->next = claim_list;
			cp->addr = addr;
			cp->len = len;

			claim_list = cp;
		}
	}
}

void
release_cmd(int argc, char **argv)
{
	/*
	 *  Release "claim"ed memory:
	 *
	 *  This is the inverse of the "claim_cmd".  It rleases memory that was
	 *  allocated via a "rm_malloc" call.
	 */

	caddr_t addr;	/* Address we're claiming			*/

	if (argc != 2) {
		/*
		 *  The "release command takes a single argument (which the
		 *  user has neglected to supply!
		 */

		printf("usage: release address\n");

	} else if (get_mem_args(argc, argv, &addr, 0, "release")) {
		/*
		 *  Arguments look good, see if we can free the memory.  This
		 *  means searching the list of previously claimed buffers.
		 */

		struct claimed *cp, *xp;

		for (xp = (struct claimed *)&claim_list; (cp = xp->next) != 0;
								    xp = cp) {
			/*
			 *  Check all buffers in the "claimed" list.  We assume
			 *  there aren't that many of them and use a linear
			 *  search.  If the claim/release commands become more
			 *  frequently used, this may need to be changed.
			 */

			if (addr == cp->addr) {
				/*
				 *  This is the buffer we want.  Release the
				 *  claimed memory, the free the claim buffer
				 *  itself.
				 */

				rm_free(addr, cp->len);
				xp->next = cp->next;
				bkmem_free((caddr_t)cp, sizeof (*cp));
				return;
			}
		}

		printf("release: memory not claimed\n");
	}
}

paddr_t
find_mem(size, align)
unsigned int size;
int align;
{
	paddr_t	loadaddr;
	register int	availseg;

#ifdef BOOT_DEBUG
	printf("find_mem: size 0x%x align 0x%x\n", size, align);
#endif /* BOOT_DEBUG */

	/* find adequate physical start addr available */
	loadaddr = (paddr_t)-1;

	for (availseg = 0; availseg < bip->memavailcnt; availseg++) {
		if (bip->memavail[availseg].flags & B_MEM_BOOTSTRAP)
			continue;

		if (size > bip->memavail[availseg].extent)
			continue;

		if (align &&
			(size + (align - 1) > bip->memavail[availseg].extent))
			continue;

		loadaddr = ALIGN(bip->memavail[availseg].base, align);

		/*
		 * if we are returning memory that has a special alignment
		 * constraint and that alignment has caused us to bump
		 * up the return addr above bip->memavail[availseg].base
		 * we have to split the current segment into two segments
		 * so we don't have a hole in the middle.
		 *
		 * the new segment list looks like this:
		 * memavail[availseg].base -> loadaddr
		 * new segment is
		 * loadaddr + size is the base.
		 */
		if (loadaddr != bip->memavail[availseg].base) {
			int segs = bip->memavailcnt;

			bip->memavail[segs].base = loadaddr + size;
			bip->memavail[segs].extent =
				(bip->memavail[availseg].base +
				bip->memavail[availseg].extent) -
					(loadaddr + size);
			bip->memavail[segs].flags =
				bip->memavail[availseg].flags;

			bip->memavail[availseg].extent = loadaddr -
				bip->memavail[availseg].base;

			bip->memavailcnt++;
		} else {
			bip->memavail[availseg].base += size;
			bip->memavail[availseg].extent -= size;
		}

		break;
	}

	/* and room for the section */
	if (loadaddr == (paddr_t)-1) {
		printf("find_mem fail size 0x%x align %d\n", size, align);
		return ((paddr_t)-1);
	}
	return (loadaddr);
}

int
setup_memlist(int type, struct bootmem *bmem, int cnt)
{

	int i, again;
	struct memlist *head, *this, *prev;

	head = (struct memlist *)0;
	prev = (struct memlist *)0;
	again = 1;

append:
	/* Note that this section will be skipped for VIRTUAL */
	for (i = 0; i < cnt; i++, bmem++) {
		this = (struct memlist *)rm_malloc(sizeof (struct memlist),
								    0, 0);
		if (this == (struct memlist *)0) {
			return (-1);
		}
		if (!head) {
			head = this;
		}
		this->address = bmem->base;
		this->size = bmem->extent;
		this->prev = prev;
		if (prev)
			prev->next = this;
		this->next = (struct memlist *)0;
		prev = this;
	}

	/* append >4G memory ranges to the end of the memlist */
	if (mem64availcnt > 0 && (type == MEM_AVAIL || type == MEM_INSTALLED)) {
		if (again) {
			bmem = &mem64avail[0];
			cnt = mem64availcnt;
			again = 0;
			goto append;
		}
	}

	switch (type) {
	case MEM_AVAIL:
		pfreelistp = head;
		break;

	case MEM_VIRTUAL:
		this = (struct memlist *)rm_malloc(
			sizeof (struct memlist), 0, 0);
		if (this == (struct memlist *)0) {
			return (-1);
		}
		vfreelistp = this;
		this->address = VIRTUAL_BASE;
		this->size = (unsigned)VIRTUAL_SIZE;
		this->next = (struct memlist *)0;

		/*
		 *  Make a second copy to serve as installed
		 *  virtual memory.
		 */
		(void) memcpy(vinstalledp, this, sizeof (struct memlist));
		break;

	case MEM_INSTALLED:
		pinstalledp = head;
		break;

	default:
		break;
	}

#ifdef BOOT_DEBUG
	dump_memlists();
#endif
	return (0);

}

#ifdef BOOT_DEBUG

int
dump_memlists()
{
	printf("Dumping %s memory list\n", mem_types[MEM_AVAIL]);
	print_memlist(pfreelistp);

	printf("Dumping %s memory list\n", mem_types[MEM_INSTALLED]);
	print_memlist(pinstalledp);

	printf("Dumping %s memory list\n", mem_types[MEM_VIRTUAL]);
	print_memlist(vfreelistp);

	(void) goany();
}
#endif


void
setup_memlists(void)
{
	struct memlist *mlp, *mxp;

	/* Show early memory available count - interpret as installed */
	(void) setup_memlist(MEM_INSTALLED, bip->memavail, bip->memavailcnt);

	/* Setup available list */
	(void) setup_memlist(MEM_AVAIL, bip->memavail, bip->memavailcnt);

	/* and virtual list(s) */
	(void) setup_memlist(MEM_VIRTUAL, 0, 0);
	bvirtlist.address = VIRTUAL_BASE;
	bvirtlist.size = (uint_t)magic_phys;
	bvirtlist.next = 0;

	/* The first used memory segment is the boot. */

	bip->memused[0].base = 0L;
	bip->memused[0].extent = bootenv.bootsize;
	bip->memused[0].flags = B_MEM_BOOTSTRAP;

	mxp = (struct memlist *)((char *)&bphyslistp
				    - (int)&((struct memlist *)0)->next);

	for (mlp = pinstalledp; mlp && mlp->address < (uint_t)magic_phys;
							    mlp = mlp->next) {
		/*
		 *  Build the "used by boot" memory list.  This consists of all
		 *  physical memory that appears between 0 and "magic_phys"
		 *  (which is already recorded in the "pinstalled" list).
		 */
		struct memlist *this;

		if (this =
		    (struct memlist *)bkmem_alloc(sizeof (struct memlist))) {
			/*
			 *  Make a copy of the next physical memlist entry.
			 *  This will be used in the "reserved to boot" list.
			 */
			if ((this->size = (uint_t)magic_phys - mlp->address) >
			    mlp->size) {
				/*
				 *  If this component doesn't span all the way
				 *  up to the "magic_phys" limit, use it's size
				 *  in place of the residual we just calculated.
				 */
				this->size = mlp->size;
			}

			this->address = mlp->address;
			mxp->next = this;
			this->next = 0;
			mxp = this;
		}
	}
}

/*
 * Allocate and map a page in user space
 */
caddr_t
malloc(caddr_t vaddr, unsigned size, int align)
{
	return ((caddr_t)map_mem((uint_t)vaddr, size, align));
}

/*
 * Let user set top virtual address or just find out what it is
 */
uint_t
vlimit(unsigned utop)
{
	return (utop ? (top_virtaddr = utop) : top_virtaddr);
}
