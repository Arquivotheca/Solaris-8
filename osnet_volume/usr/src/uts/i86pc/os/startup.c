/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)startup.c	1.133	99/10/25 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/conf.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>

#include <sys/disp.h>
#include <sys/class.h>
#include <sys/bitmap.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kstat.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_emul.h>	/* for create_prom_prop */
#include <sys/modctl.h>		/* for "procfs" hack */

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/tss.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/fp.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat.h>
#include <vm/hat_i86.h>
#include <sys/instance.h>
#include <sys/smp_impldefs.h>
#include <sys/x86_archext.h>
#include <sys/segment.h>
#include <sys/clconf.h>
#include <sys/kobj.h>
#include <sys/prom_emul.h>

extern void debug_enter(char *);
extern void lomem_init(void);

/*
 * XXX make declaration below "static" when drivers no longer use this
 * interface.
 */
extern caddr_t p0_va;	/* Virtual address for accessing physical page 0 */

static void kvm_init(void);

/*
 * Declare these as initialized data so we can patch them.
 */
pgcnt_t physmem = 0;	/* memory size in pages, patch if you want less */
int kernprot = 1;	/* write protect kernel text */

/* Global variables for MP support. Used in mp_startup */
caddr_t	rm_platter_va;
paddr_t	rm_platter_pa;


/*
 * Configuration parameters set at boot time.
 */

caddr_t econtig;		/* end of first block of contiguous kernel */
caddr_t eecontig;		/* end of segkp, which is after econtig */

struct bootops *bootops = 0;	/* passed in from boot */
extern struct bootops **bootopsp;

char bootblock_fstype[16];

/*
* new memory fragmentations are possible in startup() due to BOP_ALLOCs. this
* depends on number of BOP_ALLOC calls made and requested size, memory size `
*  combination.
*/
#define	POSS_NEW_FRAGMENTS	10

/*
 * VM data structures
 */
long page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
struct	page **page_hash;	/* Page hash table */
struct	seg *segkmap;		/* Kernel generic mapping segment */
struct	seg ktextseg;		/* Segment used for kernel executable image */
struct	seg kvalloc;		/* Segment used for "valloc" mapping */
struct seg *segkp;		/* Segment for pageable kernel virt. memory */
struct memseg *memseg_base;
struct	vnode unused_pages_vp;

#define	FOURGB	0x100000000LL
#define	PFN_32GB 0x7fffff

struct memlist *memlist;

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */
caddr_t modtext;	/* start of loadable module text reserved */
caddr_t e_modtext;	/* end of loadable module text reserved */
caddr_t extra_et;	/* MMU_PAGESIZE aligned after e_modtext */
uint64_t extra_etpa;	/* extra_et (phys addr) */
caddr_t moddata;	/* start of loadable module data reserved */
caddr_t e_moddata;	/* end of loadable module data reserved */

struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
struct memlist *shadow_ram;	/* Non-dma-able memory XXX needs work */
uint64_t pmeminstall;		/* total physical memory installed */

static void memlist_add(uint64_t, uint64_t, struct memlist **,
	struct memlist **);
static void kphysm_init(machpage_t *, struct memseg *, pgcnt_t);

#define	IO_PROP_SIZE	64	/* device property size */

/*
 * Monitor pages may not be where this says they are.
 * and the debugger may not be there either.
 *
 *		  Physical memory layout
 *		(not necessarily contiguous)
 *
 *    availmem -+-----------------------+
 *		|			|
 *		|	page pool	|
 *		|			|
 *		|-----------------------|
 *		|   configured tables	|
 *		|	buffers		|
 *   firstaddr -|-----------------------|
 *		|   hat data structures |
 *		|-----------------------|
 *		|    kernel data, bss	|
 *		|-----------------------|
 *		|    interrupt stack	| ?????
 *		|-----------------------|
 *		|	kernel text	|
 *		+-----------------------+
 *
 *
 *		  Virtual memory layout.
 *		+-----------------------+
 *		|	psm 1-1 map	|
 *		|	exec args area	|
 * 0xFFC00000  -|-----------------------|- ARGSBASE
 *		|	debugger	|
 * 0xFF800000  -|-----------------------|- DEBUGSTART
 *		|      Kernel Data	|
 * 0xFEC00000  -|-----------------------|
 *              |      Kernel Text	|
 * 0xFE800000  -|-----------------------|
 * 		|      Segkmap		|
 * 0xFD800000  -|-----------------------|- SEGKMAP_START
 * 		|			|
 *		|	segkp		|
 *		|			|
 * 0xED800000  -|-----------------------|- ekernelheap (floating)
 *		|			|
 *		|	kvseg		|
 * 0xE4000000  -|-----------------------|- kernelheap (floating)
 *		|	pp structures	|
 * 0xE0000000  -|-----------------------|- KERNELBASE (floating)
 *		|	Red Zone	|
 * 0xDFFFF000  -|-----------------------|- User shared objects 	||
 *		|			|			||
 *		|	Shared objects	|			\/
 *		|			|
 *		:			:
 *		|	user data	|
 *		|-----------------------|
 *		|	user text	|
 * 0x00010000  -|-----------------------|
 *		|	invalid		|
 * 0x00000000	+-----------------------+
 */

void init_intr_threads(struct cpu *);

/* real-time-clock initialization parameters */
long gmt_lag;		/* offset in seconds of gmt to local time */
extern long process_rtc_config_file(void);

int	insert_into_pmemlist(struct memlist **, struct memlist *);
uintptr_t	kernelbase, valloc_base, eprom_kernelbase;
size_t		valloc_sz;
/*
 * The following three variables are initialized with default
 * values in common/conf/param.c. They are updated in startup() with their
 * values, computed at boot time.
 */
#if	!defined(lint)
extern uintptr_t _kernelbase, _userlimit, _userlimit32;
extern unsigned long _dsize_limit;
extern rlim64_t rlim_infinity_map[];
#endif

/*
 * Space for low memory (below 16 meg), contiguous, memory
 * for DMA use (allocated by ddi_iopb_alloc()).  This default,
 * changeable in /etc/system, allows 2 64K buffers, plus space for
 * a few more small buffers for (e.g.) SCSI command blocks.
 */
long lomempages = (2*64*1024/PAGESIZE + 4);
kmutex_t hat_page_lock;

/*
 * List of bootstrap pages.  We mark them as allocated in startup;
 * kern_setup2 calls release_bootstrap() to free them when we're
 * completely done with the bootstrap.
 */
static page_t *lowpages_pp;

struct mmuinfo mmuinfo;
struct system_hardware system_hardware;

#ifdef  DEBUGGING_MEM
/*
 * Enable some debugging messages concerning memory usage...
 */
static int debugging_mem;
static void
printmemlist(char *title, struct memlist *mp)
{
	if (debugging_mem) {
		prom_printf("%s:\n", title);
		while (mp != 0)  {
			prom_printf("\tAddress 0x%x, size 0x%x\n",
			    mp->address, mp->size);
			mp = mp->next;
		}
	}
}
#endif	DEBUGGING_MEM

#define	BOOT_END	(3*1024*1024)	/* default to 3mb */

/*
 * Our world looks like this at startup time.
 *
 * Boot loads the kernel text at e0400000 and kernel data at e0800000.
 * Those addresses are fixed in the binary at link time. If this
 * machine supports large pages (4MB) then boot allocates a large
 * page for both text and data. If this machine supports global
 * pages, they are used also. If this machine is a 486 then lots of
 * little 4K pages are used.
 *
 * On the text page:
 * unix/genunix/krtld/module text loads.
 *
 * On the data page:
 * unix/genunix/krtld/module data loads and space for page_t's.
 */
/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	unsigned int i;
	pgcnt_t npages;
	pfn_t pfn;
	pte32_t *e_textpte;
	struct segmap_crargs a;
	int memblocks, memory_limit_warning;
	struct memlist *bootlistp, *previous, *current;
	size_t pp_giveback, pp_extra, memspace_sz, segkmem_size;
	uintptr_t segkp_base, segkmem_base, aligned_kbase;
	size_t  derived_segkmem_size, segkp_len;
	caddr_t va;
	caddr_t memspace;
	caddr_t ndata_base;
	caddr_t ndata_end;
	size_t ndata_space;
	uint_t nppstr;
	uint_t memseg_sz;
	uint_t pagehash_sz;
	uint_t memlist_sz;
	uint_t pp_sz;			/* Size in bytes of page struct array */
	int dbug_mem;
	uint64_t avmem, total_memory, highest_addr;
	uint64_t mmu_limited_physmax;
	uintptr_t max_virt_segkp;
	uint64_t max_phys_segkp;
	int	b_ext;
	static char b_ext_prop[] = "bootops-extensions";
	caddr_t	boot_end;
	uint_t	first_free_page;
	static char boot_end_prop[] = "boot-end";

	extern int cr4_value;
	extern int kobj_getpagesize();
	extern void setup_mtrr(), setup_mca();
	extern void prom_setup(void);
	extern void impl_bus_initialprobe();
	extern void hat_kern_setup(void);
	extern void memscrub_init(void);
	extern void acpi_init0(void);
	extern void acpi_ld_cancel(void);
	extern void setx86isalist(void);

	(void) check_boot_version(BOP_GETVERSION(bootops));

	/*
	 * This kernel needs bootops extensions to be at least 1
	 * (for the 1275-ish functions).
	 *
	 * jhd: cachefs boot updates boot extensions to rev 2
	 */
	if ((BOP_GETPROPLEN(bootops, b_ext_prop) != sizeof (int)) ||
	    (BOP_GETPROP(bootops, b_ext_prop, &b_ext) < 0) ||
	    (b_ext < 2)) {
		prom_printf("Booting system too old for this kernel.\n");
		prom_panic("halting");
		/*NOTREACHED*/
	}

	/*
	 * BOOT PROTECT. Ask boot for its '_end' symbol - the
	 * first available address above boot. We use this info
	 * to protect boot until it is no longer needed.
	 */
	if ((BOP_GETPROPLEN(bootops, boot_end_prop) != sizeof (caddr_t)) ||
	    (BOP_GETPROP(bootops, boot_end_prop, &boot_end) < 0))
		first_free_page = mmu_btopr(BOOT_END);
	else
		first_free_page = mmu_btopr((uint_t)boot_end);

	/*
	 * Initialize the mmu module
	 */
	mmu_init();

	pp_extra = mmuinfo.mmu_extra_pp_sz;

	/*
	 * Collect node, cpu and memory configuration information.
	 */
	get_system_configuration();

	setcputype();	/* mach/io/autoconf.c - cputype needs definition */

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

#if 0
	/*
	 * Add up how much physical memory /boot has passed us.
	 */
	phys_install = (struct memlist *)bootops->boot_mem->physinstalled;

	phys_avail = (struct memlist *)bootops->boot_mem->physavail;

	virt_avail = (struct memlist *)bootops->boot_mem->virtavail;

	/* XXX - what about shadow ram ???????????????? */
#endif
	/*
	 * the assumption here is, if we have large pages, we used
	 * them to load the kernel. So for x86 boxes that do not support
	 * large pages we have to be more circumspect in how we lay out
	 * the kernel address space.
	 */
	if (kobj_getpagesize()) {
		x86_feature |= X86_LARGEPAGE;
	}

	/*
	 * For MP machines cr4_value must be set or the other
	 * cpus will not be able to start.
	 */
	if (x86_feature & X86_LARGEPAGE)
		cr4_value = cr4();

	/*
	 * Initialize enough of the system to allow kmem_alloc to work by
	 * calling boot to allocate its memory until the time that
	 * kvm_init is completed.  The page structs are allocated after
	 * rounding up end to the nearest page boundary; the memsegs are
	 * intialized and the space they use comes from the kernel heap.
	 * With appropriate initialization, they can be reallocated later
	 * to a size appropriate for the machine's configuration.
	 *
	 * At this point, memory is allocated for things that will never
	 * need to be freed, this used to be "valloced".  This allows a
	 * savings as the pages don't need page structures to describe
	 * them because them will not be managed by the vm system.
	 */
	ndata_base = (caddr_t)roundup((uintptr_t)e_data, MMU_PAGESIZE);
	e_textpte = (pte32_t *)((cr3() & MMU_STD_PAGEMASK)) +
			MMU32_L1_INDEX(e_text);
	if (four_mb_page(e_textpte)) {
		ndata_end = (caddr_t)roundup((uintptr_t)ndata_base,
			FOURMB_PAGESIZE);
		ndata_space = ndata_end - ndata_base;
	/*
	 * Reserve space for loadable modules.
	 */
		modtext = (caddr_t)roundup((uintptr_t)e_text, MMU_PAGESIZE);
		e_modtext = modtext + MODTEXT;
		extra_et = (caddr_t)roundup((uintptr_t)e_modtext, MMU_PAGESIZE);
		extra_etpa = va_to_pfn(extra_et);
		extra_etpa = extra_etpa << MMU_STD_PAGESHIFT;
		if (ndata_base + MODDATA < ndata_end) {
			moddata = ndata_base;
			e_moddata = moddata + MODDATA;
			ndata_base = e_moddata;
			ndata_space = ndata_end - ndata_base;
		}
	} else {
		/*
		 * No large pages so don't bother loading
		 * modules on them.
		 */
		modtext = e_modtext = e_text;
		moddata = e_moddata = e_data;
		extra_et = NULL;
		ndata_space = 0;
		ndata_end = ndata_base;
	}
	/*
	 * Remember what the physically available highest page is
	 * so that dumpsys works properly, and find out how much
	 * memory is installed.
	 */

	installed_top_size(bootops->boot_mem->physinstalled, &physmax,
	    &physinstalled, mmuinfo.mmu_highest_pfn);


	/*
	 * We support more than 32Gb of memory only if eprom_kernelbase is
	 * set to a value less than KERNELBASE_ABI_MIN.
	 */
	if ((physmax > PFN_32GB) &&
	    ((eprom_kernelbase == 0) ||
	    (eprom_kernelbase >= (uintptr_t)KERNELBASE_ABI_MIN))) {
		mmuinfo.mmu_highest_pfn = PFN_32GB;
		installed_top_size(bootops->boot_mem->physinstalled, &physmax,
		    &physinstalled, mmuinfo.mmu_highest_pfn);
	}
	/*
	 * physinstalled is of type pgcnt_t. The macro ptob() relies
	 * on the type of argument passed.
	 * #define ptob(x)	((x) << PAGESHIFT)
	 */
	pmeminstall = ptob((unsigned long long)physinstalled);

	if (extra_et) {
		struct memlist *tmp;
		uint64_t seglen;

		seglen = (uint64_t)((caddr_t)roundup((uintptr_t)extra_et,
			FOURMB_PAGESIZE) - extra_et);

		if (ndata_space) {
			tmp = (struct memlist *)ndata_base;
			ndata_base += sizeof (*tmp);
			ndata_space = ndata_end - ndata_base;

			memlist_add(extra_etpa, seglen, &tmp,
				&bootops->boot_mem->physavail);
		}
#ifdef DEBUGGING_MEM
		if (debugging_mem)
			printmemlist("Add nucleus",
				bootops->boot_mem->physavail);
#endif	/* DEBUGGING_MEM */
	}

	/*
	 * Get the list of physically available memory to size
	 * the number of page structures needed.
	 * This is something of an overestimate, as it include
	 * the boot and any DMA (low continguous) reserved space.
	 */
	size_physavail(bootops->boot_mem->physavail, &npages, &memblocks,
	    mmuinfo.mmu_highest_pfn);

	/*
	 * If physmem is patched to be non-zero, use it instead of
	 * the monitor value unless physmem is larger than the total
	 * amount of memory on hand.
	 */
	if (physmem == 0 || physmem > npages)
		physmem = npages;
	else {
		npages = physmem;
	}


	/*
	 * total memory rounded to multiple of 4MB.
	 * physmem is of type pgcnt_t.
	 */

	total_memory = roundup(ptob((unsigned long long)physmem), PTSIZE);

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit(page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * Some of the locks depend on page_hashsz being set!
	 */
	page_lock_init();

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kvm_init(); twice as many are allocated
	 * as are currently needed.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + POSS_NEW_FRAGMENTS);
	pp_sz = sizeof (struct machpage) * npages;
	memspace_sz = roundup(pagehash_sz + memseg_sz + pp_sz, MMU_PAGESIZE);

	/*
	 * We don't need page structs for the memory we are allocating
	 * so we subtract an appropriate amount.
	 */
	nppstr = btop(memspace_sz -
	    (btop(memspace_sz) * sizeof (struct machpage)));
	pp_giveback = nppstr * sizeof (struct machpage);

	memspace_sz -= pp_giveback;
	npages -= btopr(memspace_sz);
	pp_sz -= pp_giveback;
	memspace_sz += (pp_extra * npages);
	valloc_sz  = memspace_sz;

	/*
	 * The page structures are allocated at virtual address valloc_base.
	 * We have the following three cases
	 * 1. The user does not set the eprom variable 'kernelbase' and
	 *    the system has less than 4Gb of memory. kernelbase defaults to
	 *    the Solaris 2.6 value (0xe0000000).
	 * 2. The user has set the variable kernelbase with the
	 *    intention of growing kvseg(kmem_alloc area).
	 * 3. The system has more than 4Gb of memory. In this case we lower
	 *    kernelbase to accommodate the extra page structures, thus
	 *    preventing segkp from shrinking.
	 */


	/*
	 * Since we can not allocate more than physmem
	 * of memory, we limit ekernelheap to SYSBASE + (physmem * 2).
	 */
	if (total_memory < 64 * 1024 * 1024)
		segkmem_size = total_memory * 2;
	else
		segkmem_size = SEGKMEMSIZE_DEFAULT;
	max_phys_segkp = ptob((uint64_t)physmem * 2);

	if ((eprom_kernelbase == 0) && (total_memory <= FOURGB)) {
		/*
		 * User has not specified KERNELBASE and the system
		 * has less than 4Gb of memory,  we use the default
		 * value of 0xE0000000 for kernelbase.
		 * 'pp structures' start at kernelbase (valloc_base). We
		 * work from kernelbase allocating kvseg followed by segkp.
		 * Bescause of this, segkp shrinks as we install more
		 * memory on the system. This is the how 2.6 and 2.7 beta
		 * Solaris worked.
		 * The first page starting at valloc_base is
		 * unmapped (REDZONE) and hence the extra "MMU_PAGESIZE"
		 * in the equation below.
		 * segkmem_base is 4Mb aligned.
		 */
		kernelbase = valloc_base = (uintptr_t)KERNELBASE;
		segkmem_base = valloc_base +
			roundup(valloc_sz + MMU_PAGESIZE, FOURMB_PAGESIZE);
		ekernelheap = (caddr_t)(segkmem_base + segkmem_size);
		segkp_base = (uintptr_t)ekernelheap;
		max_phys_segkp = ptob((uint64_t)physmem * 2);
		max_virt_segkp = SEGKMAP_START - segkp_base;
		segkp_len = (size_t)MIN(max_virt_segkp, max_phys_segkp);
	} else  {
		/*
		 * Here, we start from SEGKMAP_START and move towards lower
		 * virtual address allocating segkp, segkmem and the pp
		 * structures.
		 * segkp is rounded to a multiple of 4Mb as we want segkmem
		 * to start at a 4Mb boundary.
		 */
	    segkp_len = roundup(MIN(SEGKPSIZE_DEFAULT, max_phys_segkp),
			FOURMB_PAGESIZE);
	    segkp_base = (uintptr_t)SEGKMAP_START - segkp_len;

	    ekernelheap = (caddr_t)segkp_base;
	    if (eprom_kernelbase) {

		/*
		 * User has specified KERNELBASE
		 */
		aligned_kbase = roundup(eprom_kernelbase, FOURMB_PAGESIZE);
		derived_segkmem_size = (uintptr_t)ekernelheap - (aligned_kbase +
		    roundup(valloc_sz + MMU_PAGESIZE, FOURMB_PAGESIZE));
		if (derived_segkmem_size < segkmem_size)
			/*
			 * kvseg can't be smaller than segkmem_size
			 */
			segkmem_base = (uintptr_t)ekernelheap - segkmem_size;
		else
			segkmem_base =
			    (uintptr_t)ekernelheap - derived_segkmem_size;
	    } else {
		/*
		 * The system has more than 4Gb of memory. We don't want to
		 * shrink segkp, so we lower kernelbase.
		 */
		segkmem_base = (uintptr_t)ekernelheap - segkmem_size;
	    }
	    valloc_base = segkmem_base -
		    roundup(valloc_sz + MMU_PAGESIZE, FOURMB_PAGESIZE);
	    kernelbase = valloc_base;
		/*
		 * Now that we know the real value of kernelbase,
		 * update variables that were initalized with a value of
		 * KERNELBASE (in common/conf/param.c).
		 * lint complains since _kernelbase is  'const u_inptr_t',
		 * don't know how to get rid of this.
		 */
#if	!defined(lint)
	    _kernelbase = kernelbase;
	    _userlimit = _kernelbase;
	    _userlimit32 = _kernelbase;
	    _dsize_limit = _kernelbase - USRTEXT;
	    rlim_infinity_map[RLIMIT_DATA] = _dsize_limit;
#endif
	}
	eecontig = (caddr_t)(segkp_base + segkp_len);
	kernelheap = (caddr_t)segkmem_base;
	ASSERT((kernelbase & (FOURMB_PAGESIZE - 1)) == 0);
	/*
	 * BOP_ALLOC allocates MMU_PAGESIZE pages.
	 * If it ever allocates large pages, then REDZONE has to be of
	 * the same size as a large page.
	 */
	valloc_base += MMU_PAGESIZE;

	memspace = BOP_ALLOC(bootops, (caddr_t)valloc_base, valloc_sz,
		BO_NO_ALIGN);
	ASSERT(memspace == (caddr_t)valloc_base);

	page_hash = (struct page **)memspace;
	memseg_base = (struct memseg *)((uint_t)page_hash + pagehash_sz);
	pp_base = (struct machpage *)((uint_t)memseg_base + memseg_sz);
	mmuinfo.mmu_extra_ppp = (caddr_t)((uint_t)pp_base + pp_sz);

	bzero(memspace, valloc_sz);

	econtig = (caddr_t)ndata_end;

	/*
	 * the memory lists from boot are allocated from the heap arena
	 * so that later they can be freed and/or reallocated.
	 */
	memlist_sz = bootops->boot_mem->extent;
	/*
	 * Between now and when we finish copying in the memory lists,
	 * allocations happen so the space gets fragmented and the
	 * lists longer.  Leave enough space for lists twice as long
	 * as what boot says it has now; roundup to a pagesize.
	 */
	va = kernelheap + PAGESIZE;	/* redzone page at kernelbase */
	memlist_sz *= 2;
	memlist_sz = roundup(memlist_sz, MMU_PAGESIZE);
	memspace_sz = memlist_sz;
	memspace = (caddr_t)BOP_ALLOC(bootops, va, memspace_sz, BO_NO_ALIGN);
	va += memspace_sz;
	if (memspace == NULL)
		halt("Boot allocation failed.");
	bzero(memspace, memspace_sz);

	memlist = (struct memlist *)((uint_t)memspace);

	kernelheap_init(kernelheap, ekernelheap, va);

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	phys_avail = current = previous = memlist;

	/*
	 * This block is used to copy the memory lists from boot's
	 * address space into the kernel's address space.  The lists
	 * represent the actual state of memory including boot and its
	 * resources.  kvm_init will use these lists to initialize the
	 * vm system.
	 */

	highest_addr = 0x1000000;
	bootlistp = bootops->boot_mem->physavail;
	for (; bootlistp; bootlistp = bootlistp->next) {

		if (bootlistp->address + bootlistp->size > FOURGB)
			continue;

		if (bootlistp->address + bootlistp->size > highest_addr)
			highest_addr = bootlistp->address + bootlistp->size;
	}

	/*
	 * Now copy the memlist into kernel space.
	 */
	mmu_limited_physmax = ptob((uint64_t)mmuinfo.mmu_highest_pfn + 1);
	bootlistp = bootops->boot_mem->physavail;
	memory_limit_warning = 0;
	for (; bootlistp; bootlistp = bootlistp->next) {
		/*
		 * Reserve page zero - see use of 'p0_va'
		 */
		if (bootlistp->address == 0) {
			if (bootlistp->size > PAGESIZE) {
				bootlistp->address += PAGESIZE;
				bootlistp->size -= PAGESIZE;
			} else
				continue;
		}

		if ((previous != current) && (bootlistp->address ==
		    previous->address + previous->size)) {
			/* coalesce */
			previous->size += bootlistp->size;
			if ((previous->address + previous->size) >
			    mmu_limited_physmax) {
				previous->size = mmu_limited_physmax -
				    previous->address;
				memory_limit_warning = 1;
			}
			continue;
		}
		if (bootlistp->address >= mmu_limited_physmax) {
			memory_limit_warning = 1;
			break;
		}
		current->address = bootlistp->address;
		current->size = bootlistp->size;
		current->next = (struct memlist *)0;
		if ((current->address + current->size) >
		    mmu_limited_physmax) {
			current->size = mmu_limited_physmax - current->address;
			memory_limit_warning = 1;
		}
		if (previous == current) {
			current->prev = (struct memlist *)0;
			current++;
		} else {
			current->prev = previous;
			previous->next = current;
			current++;
			previous++;
		}
	}

	/*
	 * Initialize the page structures from the memory lists.
	 */
	kphysm_init(pp_base, memseg_base, npages);

	availrmem_initial = availrmem = freemem;

	/*
	 * BOOT PROTECT. All of boots pages are now in the available
	 * page list, but we still have a few boot chores remaining.
	 * Acquire locks on boot pages before they get can get consumed
	 * by kmem_allocs after kvm_init() is called. Unlock the
	 * pages once boot is really gone.
	 */
	lowpages_pp = (page_t *)NULL;
	for (pfn = 0; pfn < first_free_page; pfn++) {
		page_t *pp;
		if ((pp = page_numtopp_alloc(pfn)) != NULL) {
			pp->p_next = lowpages_pp;
			lowpages_pp = pp;
		}
	}

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize bp_mapin().
	 */
	bp_init(PAGESIZE, HAT_STORECACHING_OK);

	if (eprom_kernelbase && (eprom_kernelbase != kernelbase))
		cmn_err(CE_WARN, "kernelbase value, User specified 0x%x, "
		    "System using 0x%x\n",
		    (int)eprom_kernelbase, (int)kernelbase);

	if (kernelbase < (uintptr_t)KERNELBASE_ABI_MIN) {
		cmn_err(CE_WARN, "kernelbase set to 0x%x, system is not "
		    "386 ABI compliant.", (int)kernelbase);
	}


	if (memory_limit_warning)
		cmn_err(CE_WARN, "%s: limiting physical memory to %d MB\n",
		    mmuinfo.mmu_name,
		    (uint32_t)(mmu_limited_physmax/(1024 * 1024)));
	/*
	 * Initialize ten-micro second timer so that drivers will
	 * not get short changed in their init phase. This was
	 * not getting called until clkinit which, on fast cpu's
	 * caused the drv_usecwait to be way too short.
	 */
	microfind();

	/*
	 * Read the GMT lag from /etc/rtc_config.
	 */
	gmt_lag = process_rtc_config_file();

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	param_calc(HAT_MAXHAT);

	mod_setup();

	/*
	 * Setup machine check architecture on P6
	 */
	setup_mca();

	/*
	 * Initialize system parameters.
	 */
	param_init();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	setup_kernel_page_directory(CPU);


	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit();

	/*
	 * This is needed here to initialize hw_serial[] for cluster booting.
	 */
	if ((i = modload("misc", "sysinit")) != (unsigned int)-1)
		(void) modunload(i);
	else
		cmn_err(CE_CONT, "sysinit load failed");

	/* Read cluster configuration data. */
	clconf_init();

#if 0
	/*
	 * Initialize the instance number data base--this must be done
	 * after mod_setup and before the bootops are given up
	 */

	e_ddi_instance_init();
#endif
	/*
	 * Make the in core copy of the prom tree - used for
	 * emulation of ieee 1275 boot environment
	 *
	 * impl_bus_initialprobe() complete the prom setup by
	 * loading the prom emulator module misc/pci_autoconfig.
	 */
	prom_setup();
	impl_bus_initialprobe();
	setup_ddi();

	/*
	 * Lets take this opportunity to load the root device.
	 */
	if (loadrootmodules() != 0)
		halt("Can't load the root filesystem");

	/*
	 * Load all platform specific modules
	 */
	acpi_init0();
	psm_modload();

	/*
	 * Call back into boot and release boots resources.
	 */
	BOP_QUIESCE_IO(bootops);

	/*
	 * Setup MTRR registers in P6
	 */
	setup_mtrr();

	/*
	 * Copy physinstalled list into kernel space.
	 */
	phys_install = current;
	copy_memlist(bootops->boot_mem->physinstalled, &current);

	/*
	 * Virtual available next.
	 */
	virt_avail = current;
	copy_memlist(bootops->boot_mem->virtavail, &current);

	/*
	 * Copy in boot's page tables,
	 * set up extra page tables for the kernel,
	 * and switch to the kernel's context.
	 */
	hat_kern_setup();

	/*
	 * If we have more than 4Gb of memory, kmem_alloc's have
	 * to be restricted to less than 4Gb. Setup pp freelist
	 * for kvseg.
	 */
	mmu_setup_kvseg(physmax);

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();


	if (x86_feature & X86_P5) {
		extern void setup_idt_base(caddr_t);
		extern void pentium_pftrap(void);
		extern struct gate_desc idt[];
		struct gdscr *pf_idtp;
		caddr_t new_idt_addr;

		new_idt_addr = kmem_zalloc(MMU_PAGESIZE, KM_NOSLEEP);
		if (new_idt_addr == NULL) {
			cmn_err(CE_PANIC, "failed to allocate a page"
				"for pentium bug workaround\n");
		}
		bcopy((caddr_t)&idt[0], (caddr_t)new_idt_addr,
			IDTSZ * sizeof (struct gate_desc));
		pf_idtp = (struct gdscr *)new_idt_addr;
		pf_idtp += T_PGFLT;
		pf_idtp->gd_off0015 = ((uint_t)pentium_pftrap) & 0xffff;
		pf_idtp->gd_off1631 = ((uint_t)pentium_pftrap >> 16);
		(void) as_setprot(&kas, new_idt_addr, MMU_PAGESIZE,
			(uint_t)(PROT_READ|PROT_EXEC));
		(void) setup_idt_base((caddr_t)new_idt_addr);
		CPU->cpu_idt = (struct gate_desc *)new_idt_addr;
	}

	/*
	 * Map page 0 for drivers, such as kd, that need to pick up
	 * parameters left there by controllers/BIOS.
	 */
	p0_va = i86devmap(btop(0x0), 1, (PROT_READ));  /* 4K */

	/*
	 * If the following is true, someone has patched
	 * phsymem to be less than the number of pages that
	 * the system actually has.  Remove pages until system
	 * memory is limited to the requested amount.  Since we
	 * have allocated page structures for all pages, we
	 * correct the amount of memory we want to remove
	 * by the size of the memory used to hold page structures
	 * for the non-used pages.
	 */
	if (physmem < npages) {
		uint_t diff;
		offset_t off;
		struct page *pp;
		caddr_t rand_vaddr;
		struct seg kseg;

		cmn_err(CE_WARN, "limiting physmem to %lu pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		kseg.s_as = &kas;
		while (diff--) {
			rand_vaddr = (caddr_t)(((uint_t)&unused_pages_vp >> 7) ^
						((u_offset_t)off >> PAGESHIFT));
			pp = page_create_va(&unused_pages_vp, off, MMU_PAGESIZE,
				PG_WAIT | PG_EXCL, &kseg, rand_vaddr);
			if (pp == NULL)
				cmn_err(CE_PANIC, "limited physmem too much!");
			page_io_unlock(pp);
			page_downgrade(pp);
			availrmem--;
			off += MMU_PAGESIZE;
		}
	}

	/*
	 * When printing memory, show the total as physmem less
	 * that stolen by a debugger.
	 * XXX - do we know how much memory kadb uses?
	 */
	dbug_mem = 0;	/* XXX */
	cmn_err(CE_CONT, "?mem = %luK (0x%lx)\n",
	    (physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    ptob(physinstalled - dbug_mem));

	avmem = ptob((unsigned long long)freemem);
	cmn_err(CE_CONT, "?avail mem = %lld\n", (unsigned long long)avmem);

	/*
	 * Initialize the segkp segment type.
	 */


	rw_enter(&kas.a_lock, RW_WRITER);
	segkp = seg_alloc(&kas, (caddr_t)segkp_base, segkp_len);
	if (segkp == NULL)
		cmn_err(CE_PANIC, "startup: cannot allocate segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);

	/*
	 * Now create generic mapping segment.  This mapping
	 * goes NCARGS beyond ekernelheap up to DEBUGSTART.
	 * But if the total virtual address is greater than the
	 * amount of free memory that is available, then we trim
	 * back the segment size to that amount.
	 */
	va = (caddr_t)SEGKMAP_START;
	i = SEGMAPSIZE;
	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((uint_t)va & MAXBOFFSET) == 0);

#ifdef XXX
	/*
	 * If there's a debugging ramdisk, we want to replace DEBUGSTART to
	 * the start of the ramdisk.
	 */
#endif XXX
	if (i > mmu_ptob(freemem))
		i = mmu_ptob(freemem);
	i &= MAXBMASK;	/* 1201049: segkmap size must be MAXBSIZE aligned */

	rw_enter(&kas.a_lock, RW_WRITER);
	segkmap = seg_alloc(&kas, va, i);
	if (segkmap == NULL)
		cmn_err(CE_PANIC, "cannot allocate segkmap");

	a.prot = PROT_READ | PROT_WRITE;
	a.shmsize = 0;
	a.nfreelist = 2;

	if (segmap_create(segkmap, (caddr_t)&a) != 0)
		panic("segmap_create segkmap");
	rw_exit(&kas.a_lock);

	setup_vaddr_for_ppcopy(CPU);

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Startup memory scrubber.
	 */
	(void) memscrub_init();

	/*
	 * Configure the system.
	 */
	configure();		/* set up devices */

	/*
	 * Set the isa_list string to the defined instruction sets we
	 * support. Default to i386
	 */

	setx86isalist();

	init_intr_threads(CPU);

	psm_install();
	acpi_ld_cancel();	/* undo ACPI lockdown after pcplusmp */

	/*
	 * We're done with bootops.  We don't unmap the bootstrap yet because
	 * we're still using bootsvcs.
	 */
	*bootopsp = (struct bootops *)0;
	bootops = (struct bootops *)NULL;

	(*picinitf)();
	sti();

	(void) add_avsoftintr((void *)NULL, 1, softlevel1,
		"softlevel1", NULL); /* XXX to be moved later */

	/*
	 * Allocate contiguous, memory below 16 mb
	 * with corresponding data structures to control its use.
	 */
	lomem_init();
}


#define	TBUF	1024

void
setx86isalist(void)
{
	char *tp;
	char *rp;
	size_t len;
	extern char *isa_list;

	tp = kmem_alloc(TBUF, KM_SLEEP);

	*tp = '\0';

	switch (cputype & CPU_ARCH) {
	/* The order of these case statements is very important! */

	case I86_P5_ARCH:
		if (x86_feature & X86_CMOV) {
			/* PentiumPro */
			(void) strcat(tp, "pentium_pro");
			(void) strcat(tp, (x86_feature & X86_MMX) ?
				"+mmx pentium_pro " : " ");
		}
		/* fall through to plain Pentium */
		(void) strcat(tp, "pentium");
		(void) strcat(tp, (x86_feature & X86_MMX) ?
			"+mmx pentium " : " ");
		/* FALLTHROUGH */

	case I86_486_ARCH:
		(void) strcat(tp, "i486 ");

		/* FALLTHROUGH */
	case I86_386_ARCH:
	default:
		(void) strcat(tp, "i386 ");

		/*
		 * We need this completely generic one to avoid
		 * confusion between a subdirectory of e.g. /usr/bin
		 * and the contents of /usr/bin e.g. /usr/bin/i386
		 */
		(void) strcat(tp, "i86");
	}

	/*
	 * Allocate right-sized buffer, copy temporary buf to it,
	 * and free temporary buffer.
	 */
	len = strlen(tp) + 1;   /* account for NULL at end of string */
	rp = kmem_alloc(len, KM_SLEEP);
	if (rp == NULL)
		return;
	isa_list = strcpy(rp, tp);
	kmem_free(tp, TBUF);

}

extern char hw_serial[];
char *_hs1107 = hw_serial;
ulong_t  _bdhs34;

void
post_startup(void)
{
	void add_cpunode2promtree();

	/*
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);
	/*
	 * complete mmu initialization, now that kernel and critical
	 * modules have been loaded.
	 */
	(void) post_startup_mmu_initialization();

	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	/*
	 * Load the floating point emulator if necessary.
	 */
	if (fp_kind == FP_NO) {
		if (modload("misc", "emul_80387") == -1)
			cmn_err(CE_CONT, "No FP emulator found\n");
		cmn_err(CE_CONT, "FP hardware will %sbe emulated by software\n",
			fp_kind == FP_SW ? "" : "not ");
	}

	maxmem = freemem;

	add_cpunode2promtree(CPU->cpu_id);

	(void) spl0();		/* allow interrupts */
}

void
release_bootstrap(void)
{
	extern page_t *lowpages_pp;
	pfn_t pfn;

	/*
	 * *Now* we're done with the bootstrap.  Unmap it.
	 */
	(void) clear_bootpde(CPU);

	/*
	 * ... and free the pages.
	 */
	while (lowpages_pp) {
		page_t *pp;
		pp = lowpages_pp;
		lowpages_pp = pp->p_next;
		pp->p_next = (struct page *)0;
		page_free(pp, 1);
	}

	/* Boot pages available - allocate any needed low phys pages */

	/*
	 * Get 1 page below 1 MB so that other processors can boot up.
	 */
	for (pfn = 1; pfn < btop(1*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) != NULL) {
			rm_platter_va = i86devmap(pfn, 1, PROT_READ|PROT_WRITE);
			rm_platter_pa = ptob(pfn);
			break;
		}
	}
	if (pfn == btop(1*1024*1024)) {
		cmn_err(CE_WARN,
		    "No page available for starting up auxiliary processors\n");
	}
}

/*
 * kphysm_init() initializes physical memory.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, pgcnt_t npages)
{
	struct memlist *pmem;
	struct memseg *cur_memseg;
	struct memseg *tmp_memseg;
	struct memseg **prev_memsegp;
	ulong_t pnum;
	extern void page_coloring_init(void);

	reload_cr3();
	ASSERT(page_hash != NULL && page_hashsz != 0);

	page_coloring_init();

	cur_memseg = memsegp;
	for (pmem = phys_avail; pmem && npages;
	    pmem = pmem->next, cur_memseg++) {
		ulong_t base;
		uint_t num;

		/*
		 * Build the memsegs entry
		 */
		num = btop(pmem->size);
		if (num > npages)
			num = npages;

		npages -= num;
		/* need to roundup in case address is not page aligned */
		base = btop(roundup(pmem->address, MMU_PAGESIZE));

		cur_memseg->pages = pp;
		cur_memseg->epages = pp + num;
		cur_memseg->pages_base = base;
		cur_memseg->pages_end = base + num;

		/* insert in memseg list, decreasing number of pages order */
		for (prev_memsegp = &memsegs, tmp_memseg = memsegs;
		    tmp_memseg;
		    prev_memsegp = &(tmp_memseg->next),
		    tmp_memseg = tmp_memseg->next) {
			if (num > tmp_memseg->pages_end -
			    tmp_memseg->pages_base)
				break;
		}
		cur_memseg->next = *prev_memsegp;
		*prev_memsegp = cur_memseg;

		/*
		 * Initialize the PSM part of the page struct
		 */
		pnum = cur_memseg->pages_base;
		for (pp = cur_memseg->pages; pp < cur_memseg->epages; pp++) {
			psm_pageinit(pp, pnum);
			pnum++;
		}

		/*
		 * have the PIM initialize things for this
		 * chunk of physical memory.
		 */
		add_physmem((page_t *)cur_memseg->pages, num);
	}

	build_pfn_hash();
}

/*
 * Kernel VM initialization.
 */
static void
kvm_init(void)
{
	register caddr_t va;
	uint_t prot;

	extern void _start();

	/*
	 * Put the kernel segments in kernel address space.  Make it a
	 * "kernel memory" segment objects.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

	(void) seg_attach(&kas, (caddr_t)_start,
	    (uintptr_t)econtig - (uintptr_t)_start, &ktextseg);
	(void) segkmem_create(&ktextseg);

	(void) seg_attach(&kas, (caddr_t)valloc_base, valloc_sz, &kvalloc);
	(void) segkmem_create(&kvalloc);

	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, kernelheap,
	    (uintptr_t)ekernelheap - (uintptr_t)kernelheap, &kvseg);
	(void) segkmem_create(&kvseg);

	rw_exit(&kas.a_lock);

	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * All level 1 entries other than the kernel were set invalid
	 * when our prototype level 1 table was created.  Thus, we only need
	 * to deal with addresses above kernelbase here.  Also, all ptes
	 * for this region have been allocated and locked, or they are not
	 * used.  Thus, all we need to do is set protections.  Invalid until
	 * start.
	 */
	ASSERT((((uint_t)_start) & PAGEOFFSET) == 0);
	for (va = (caddr_t)kernelbase; va < (caddr_t)valloc_base;
	    va += PAGESIZE) {
		/* user copy red zone */
		(void) as_setprot(&kas, va, PAGESIZE, 0);
	}
	prot = PROT_READ | PROT_EXEC;
	prot |= (kernprot) ? 0 : PROT_WRITE;

	/*
	 * (Normally) Read-only until end of text.
	 */
	(void) as_setprot(&kas, (caddr_t)_start,
	    (size_t)(e_modtext - (uintptr_t)_start), prot);

	va = s_data;

	/*
	 * Writable until end.
	 */
	(void) as_setprot(&kas, va, (uint_t)(econtig - va),
	    PROT_READ | PROT_WRITE | PROT_EXEC);


	rw_exit(&kas.a_lock);

	/*
	 * Flush the PDC of any old mappings.
	 */
	mmu_tlbflush_all();

}

int
insert_into_pmemlist(head, cur)
struct 	memlist **head;
struct	memlist	*cur;
{
	struct memlist *tmemlist, *ptmemlist;

	if (*head == (struct memlist *)0) {
		*head = cur;
		cur->next = (struct memlist *)0;
	} else {
		tmemlist = *head;
		ptmemlist = (struct memlist *)0;
		while (tmemlist) {
			if (cur->address < tmemlist->address) {
				if (((uint_t)cur->address + cur->size) ==
				    tmemlist->address) {
					tmemlist->address = cur->address;
					tmemlist->size += cur->size;
					return (0);
				} else break;
			} else if (cur->address == ((uint_t)tmemlist->address +
				tmemlist->size)) {
				tmemlist->size += cur->size;
				return (0);
			}
			ptmemlist = tmemlist;
			tmemlist = tmemlist->next;
		}
		if (tmemlist == (struct memlist *)0) {
			/* get to the tail of the list */
			ptmemlist->next = cur;
			cur->next = (struct memlist *)0;
		} else if (ptmemlist == (struct memlist *)0) {
			/* get to the head of the list */
			cur->next = *head;
			*head = cur;
		} else {
			/* insert in between */
			cur->next = ptmemlist->next;
			ptmemlist->next = cur;
		}
	}
	return (1);
}

/*
 * These are MTTR registers supported by P6
 */
static struct	mtrrvar	mtrrphys_arr[MAX_MTRRVAR];
static uint64_t mtrr64k, mtrr16k1, mtrr16k2;
static uint64_t mtrr4k1, mtrr4k2, mtrr4k3;
static uint64_t mtrr4k4, mtrr4k5, mtrr4k6;
static uint64_t mtrr4k7, mtrr4k8, mtrrcap;
uint64_t mtrrdef, pat_attr_reg;

/*
 * Disable reprogramming of MTRRs by default.
 */
int	enable_relaxed_mtrr = 0;

static uint_t	mci_ctl[] = {REG_MC0_CTL, REG_MC1_CTL, REG_MC2_CTL,
		    REG_MC3_CTL, REG_MC4_CTL};
static uint_t	mci_status[] = {REG_MC0_STATUS, REG_MC1_STATUS, REG_MC2_STATUS,
		    REG_MC3_STATUS, REG_MC4_STATUS};
static uint_t	mci_addr[] = {REG_MC0_ADDR, REG_MC1_ADDR, REG_MC2_ADDR,
		    REG_MC3_ADDR, REG_MC4_ADDR};
static int	mca_cnt;


void
setup_mca()
{
	int 		i;
	uint64_t	allzeros;
	uint64_t	allones;
	uint64_t	buf;
	long long	mca_cap;

	if (!(x86_feature & X86_MCA))
		return;
	(void) rdmsr(REG_MCG_CAP, &buf);
	mca_cap = *(long long *)buf;
	allones = 0xffffffffffffffffULL;
	if (mca_cap & MCG_CAP_CTL_P)
		(void) wrmsr(REG_MCG_CTL, &allones);
	mca_cnt = mca_cap & MCG_CAP_COUNT_MASK;
	if (mca_cnt > P6_MCG_CAP_COUNT)
		mca_cnt = P6_MCG_CAP_COUNT;
	for (i = 1; i < mca_cnt; i++)
		(void) wrmsr(mci_ctl[i], &allones);
	allzeros = 0;
	for (i = 0; i < mca_cnt; i++)
		(void) wrmsr(mci_status[i], &allzeros);
	setcr4(cr4()|CR4_MCE);

}
int
mca_exception(struct regs *rp)
{
	uint64_t	status, addr;
	uint64_t	allzeros;
	uint64_t	buf;
	int		i, ret = 1, errcode, mserrcode;

	allzeros = 0;
	(void) rdmsr(REG_MCG_STATUS, &buf);
	status = buf;
	if (status & MCG_STATUS_RIPV)
		ret = 0;
	if (status & MCG_STATUS_EIPV)
		cmn_err(CE_WARN, "MCE at %x\n", rp->r_eip);
	(void) wrmsr(REG_MCG_STATUS, &allzeros);
	for (i = 0; i < mca_cnt; i++) {
		(void) rdmsr(mci_status[i], &buf);
		status = buf;
		/*
		 * If status register not valid skip this bank
		 */
		if (!(status & MCI_STATUS_VAL))
			continue;
		errcode = status & MCI_STATUS_ERRCODE;
		mserrcode = (status  >> MSERRCODE_SHFT) & MCI_STATUS_ERRCODE;
		if (status & MCI_STATUS_ADDRV) {
			/*
			 * If mci_addr contains the address where
			 * error occurred, display the address
			 */
			(void) rdmsr(mci_addr[i], &buf);
			addr = buf;
			cmn_err(CE_WARN, "MCE: Bank %d: error code %x:"\
			    "addr = %llx, model errcode = %x\n", i,
			    errcode, addr, mserrcode);
		} else {
			cmn_err(CE_WARN,
			    "MCE: Bank %d: error code %x, mserrcode = %x\n",
			    i, errcode, mserrcode);
		}
		(void) wrmsr(mci_status[i], &allzeros);
	}
	return (ret);
}


void
setup_mtrr()
{
	int i, ecx;
	int vcnt;
	struct	mtrrvar	*mtrrphys;

	if (!(x86_feature & X86_MTRR))
		return;

	(void) rdmsr(REG_MTRRCAP, &mtrrcap);
	(void) rdmsr(REG_MTRRDEF, &mtrrdef);
	if (mtrrcap & MTRRCAP_FIX) {
		(void) rdmsr(REG_MTRR64K, &mtrr64k);
		(void) rdmsr(REG_MTRR16K1, &mtrr16k1);
		(void) rdmsr(REG_MTRR16K2, &mtrr16k2);
		(void) rdmsr(REG_MTRR4K1, &mtrr4k1);
		(void) rdmsr(REG_MTRR4K2, &mtrr4k2);
		(void) rdmsr(REG_MTRR4K3, &mtrr4k3);
		(void) rdmsr(REG_MTRR4K4, &mtrr4k4);
		(void) rdmsr(REG_MTRR4K5, &mtrr4k5);
		(void) rdmsr(REG_MTRR4K6, &mtrr4k6);
		(void) rdmsr(REG_MTRR4K7, &mtrr4k7);
		(void) rdmsr(REG_MTRR4K8, &mtrr4k8);
	}
	if ((vcnt = (mtrrcap & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;

	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		(void) rdmsr(ecx, &mtrrphys->mtrrphys_base);
		(void) rdmsr(ecx + 1, &mtrrphys->mtrrphys_mask);
		if ((x86_feature & X86_PAT) && enable_relaxed_mtrr) {
			mtrrphys->mtrrphys_mask &= ~MTRRPHYSMASK_V;
		}
	}
	if (x86_feature & X86_PAT) {
		if (enable_relaxed_mtrr)
			mtrrdef = MTRR_TYPE_WB|MTRRDEF_FE|MTRRDEF_E;
		pat_attr_reg = PAT_DEFAULT_ATTRIBUTE;
	}

	mtrr_sync();
}

/*
 * Sync current cpu mtrr with the incore copy of mtrr.
 * This function has to be invoked with interrupts disabled
 * Currently we do not capture other cpu's. This is invoked on cpu0
 * just after reading /etc/system.
 * On other cpu's its invoked from mp_startup().
 */
void
mtrr_sync()
{
	uint64_t my_mtrrdef;
	uint_t	crvalue, cr0_orig;
	extern	invalidate_cache();
	int	vcnt, i, ecx;
	struct	mtrrvar	*mtrrphys;

	cr0_orig = crvalue = cr0();
	crvalue |= CR0_CD;
	crvalue &= ~CR0_NW;
	setcr0(crvalue);
	invalidate_cache();
	setcr3(cr3());

	if (x86_feature & X86_PAT) {
		(void) wrmsr(REG_MTRRPAT, &pat_attr_reg);
	}
	(void) rdmsr(REG_MTRRDEF, &my_mtrrdef);
	my_mtrrdef &= ~MTRRDEF_E;
	(void) wrmsr(REG_MTRRDEF, &my_mtrrdef);
	if (mtrrcap & MTRRCAP_FIX) {
		(void) wrmsr(REG_MTRR64K, &mtrr64k);
		(void) wrmsr(REG_MTRR16K1, &mtrr16k1);
		(void) wrmsr(REG_MTRR16K2, &mtrr16k2);
		(void) wrmsr(REG_MTRR4K1, &mtrr4k1);
		(void) wrmsr(REG_MTRR4K2, &mtrr4k2);
		(void) wrmsr(REG_MTRR4K3, &mtrr4k3);
		(void) wrmsr(REG_MTRR4K4, &mtrr4k4);
		(void) wrmsr(REG_MTRR4K5, &mtrr4k5);
		(void) wrmsr(REG_MTRR4K6, &mtrr4k6);
		(void) wrmsr(REG_MTRR4K7, &mtrr4k7);
		(void) wrmsr(REG_MTRR4K8, &mtrr4k8);
	}
	if ((vcnt = (mtrrcap & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		(void) wrmsr(ecx, &mtrrphys->mtrrphys_base);
		(void) wrmsr(ecx + 1, &mtrrphys->mtrrphys_mask);
	}
	(void) wrmsr(REG_MTRRDEF, &mtrrdef);
	setcr3(cr3());
	invalidate_cache();
	setcr0(cr0_orig);
}

/*
 * resync mtrr so that BIOS is happy. Called from mdboot
 */
void
mtrr_resync()
{
	if ((x86_feature & X86_PAT) && enable_relaxed_mtrr) {
		/*
		 * We could have changed the default mtrr definition.
		 * Put it back to uncached which is what it is at power on
		 */
		mtrrdef = MTRR_TYPE_UC|MTRRDEF_FE|MTRRDEF_E;
		mtrr_sync();
	}
}

void
get_system_configuration()
{
	char	prop[32];
	uint64_t nodes_ll, cpus_pernode_ll, lvalue;
	extern int getvalue(char *token, uint64_t *valuep);


	if (((BOP_GETPROPLEN(bootops, "nodes") > sizeof (prop)) ||
		(BOP_GETPROP(bootops, "nodes", prop) < 0) 	||
		(getvalue(prop, &nodes_ll) == -1) ||
		(nodes_ll > MAXNODES))			   ||
	    ((BOP_GETPROPLEN(bootops, "cpus_pernode") > sizeof (prop)) ||
		(BOP_GETPROP(bootops, "cpus_pernode", prop) < 0) ||
		(getvalue(prop, &cpus_pernode_ll) == -1))) {

		system_hardware.hd_nodes = 1;
		system_hardware.hd_cpus_per_node = 0;
	} else {
		system_hardware.hd_nodes = (int)nodes_ll;
		system_hardware.hd_cpus_per_node = (int)cpus_pernode_ll;
	}
	if ((BOP_GETPROPLEN(bootops, "kernelbase") > sizeof (prop)) ||
		(BOP_GETPROP(bootops, "kernelbase", prop) < 0) 	||
		(getvalue(prop, &lvalue) == -1))
			eprom_kernelbase = NULL;
	else
			eprom_kernelbase = (uintptr_t)lvalue;
}

/*
 * Add to a memory list.
 * start = start of new memory segment
 * len = length of new memory segment in bytes
 * memlistp = pointer to array of available memory segment structures
 * curmemlistp = memory list to which to add segment.
 */
static void
memlist_add(uint64_t start, uint64_t len, struct memlist **memlistp,
	struct memlist **curmemlistp)
{
	struct memlist *cur, *new, *last;
	uint64_t end = start + len;

	new = *memlistp;

	new->address = start;
	new->size = len;
	*memlistp = new + 1;

	for (cur = *curmemlistp; cur; cur = cur->next) {
		last = cur;
		if (cur->address >= end) {
			new->next = cur;
			new->prev = cur->prev;
			cur->prev = new;
			if (cur == *curmemlistp)
				*curmemlistp = new;
			else
				new->prev->next = new;
			return;
		}
		if (cur->address + cur->size > start)
			panic("munged memory list = 0x%x\n", curmemlistp);
	}
	new->next = NULL;
	new->prev = last;
	last->next = new;
}

void
kobj_vmem_init(vmem_t **text_arena, vmem_t **data_arena)
{
	size_t tsize = e_modtext - modtext;
	size_t dsize = e_moddata - moddata;

	*text_arena = vmem_create("module_text", tsize ? modtext : NULL, tsize,
	    1, segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);
	*data_arena = vmem_create("module_data", dsize ? moddata : NULL, dsize,
	    1, segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);
}

#define	CPUVEND_STR_SZ	16

#define	LINE_SIZE_STR		"-line-size"
#define	ASSOCIATIVITY_STR	"-associativity"
#define	SIZE_STR		"-size"

#define	INVALID_CACHE_DESC(x)	((uchar_t)(x) & 0x80)
/*
 * The compiler is currently not smart enough to recognise identical
 * strings in a const structure and optimise it. SO, we explicitly
 * declare l2-cache which is used multiple times below
 */
static const char l2_cache_str[] = {"l2-cache"};

static const struct	intel_cachedesc {
	unsigned char	code;
	unsigned char	associativity;
	unsigned short	line_size;
	uint32_t size;
	const char *cache_lbl;
} IA32_cache_d[] = {
	{ 1, 4, 0, 32, "itlb4K"},
	{ 2, 0, 0, 2, "itlb4M"},
	{ 3, 4, 0, 64, "dtlb4K"},
	{ 4, 4, 0, 8, "dtlb4M"},
	{ 6, 4, 32, 8*1024, "icache"},
	{ 8, 4, 32, 16*1024, "icache"},
	{ 0x0a, 2, 32, 8*1024, "dcache"},
	{ 0x0c, 4, 32, 16*1024, "dcache"},
	{ 0x40, 0, 0, 0, NULL},
	{ 0x41, 4, 32, 128*1024, l2_cache_str},
	{ 0x42, 4, 32, 256*1024, l2_cache_str},
	{ 0x43, 4, 32, 512*1024, l2_cache_str},
	{ 0x44, 4, 32, 1024*1024, l2_cache_str},
	{ 0x45, 4, 32, 2*1024*1024, l2_cache_str},
	{ 0x46, 4, 32, 4*1024*1024, l2_cache_str},
	{ 0x47, 4, 32, 8*1024*1024, l2_cache_str}
};

/*
 * even though 4 & 8M ecache above is not specified by Intel, it is derived
 * from the pattern to accommodate chips which may come in the next
 * few years
 */

kmutex_t cpu_node_lock;

void
create_prom_and_ddi_prop(
	dnode_t	nodeid,
	char *label,
	caddr_t value,
	dev_info_t *devi,
	int	length)
{
	/*
	 * XXX: The OS should not be creating prom properties
	 */
	promif_create_prop_external(nodeid, label, value, length);

	(void) ddi_prop_create(DDI_DEV_T_NONE, devi,
		DDI_PROP_CANSLEEP, label, value, length);
}

/*
 * create a node for the given cpu under the prom root node.
 * Also, create a cpu node in the device tree.
 */
void
add_cpunode2promtree(int cpu_id)
{
	char	*vendor;
	uint32_t 	*family, *model, *stepid;
	uint8_t	*conf_descp;
	int	highest_eax, num_entries, i;
	char	label[128];
	struct	cpuid_regs {
		uint_t	reg_eax;
		uint_t	reg_ebx;
		uint_t	reg_edx;
		uint_t	reg_ecx;
	} cpuid_regs;
#define	eax	cpuid_regs.reg_eax
#define	ebx	cpuid_regs.reg_ebx
#define	ecx	cpuid_regs.reg_ecx
#define	edx	cpuid_regs.reg_edx

	uint32_t	*iptr, *cpu_freq_val;
	dnode_t	nodeid;
	dev_info_t *cpu_devi;
	dnode_t		cpu_prom_nodes[NCPU];
	dev_info_t	cpu_devi_nodes[NCPU];
	extern	int cpu_freq;
	extern	uint_t cpuid();


	mutex_enter(&cpu_node_lock);
	/*
	 * create a prom node for the cpu identified as 'cpu_id' under
	 * the root node.
	 *
	 * XXX: The OS should not be creating nodes or properties in the
	 * XXX: prom's device tree.
	 */
	(void) impl_ddi_alloc_nodeid(&nodeid);
	(void) promif_add_child(prom_rootnode(), nodeid, (char *)"cpu");
	cpu_prom_nodes[cpu_id] = nodeid;

	cpu_freq_val = kmem_alloc(sizeof (uint32_t), KM_SLEEP);
	*cpu_freq_val = (cpu_freq * 1000000)/hz;
	/*
	 * add the property 'clock_frequency' to the cpu node.
	 */
	promif_create_prop_external(nodeid, "clock-frequency",
		cpu_freq_val, sizeof (uint32_t));

	/*
	 * XXX: We can get away with this because we're single threaded
	 * XXX: The call to ddi_add_child will 'take' it again.
	 */
	impl_ddi_free_nodeid(nodeid);

	/*
	 * The device tree has been built by the function setup_ddi(),
	 * following prom_init(). Since, we created a prom node for the
	 * cpu 'cpu_id', we will create a device node under the 'root_node'.
	 */
	cpu_devi_nodes[cpu_id] = NULL;
	cpu_devi = ddi_add_child(ddi_root_node(), "cpu", nodeid, 0);
	if (cpu_devi == NULL) {
		mutex_exit(&cpu_node_lock);
		return;
	}
	cpu_devi_nodes[cpu_id] = cpu_devi;

	(void) ddi_prop_create(DDI_DEV_T_NONE, cpu_devi, DDI_PROP_CANSLEEP,
		"clock-frequency", (caddr_t)cpu_freq_val, sizeof (uint32_t));

	if (!(x86_feature & X86_CPUID)) {
		mutex_exit(&cpu_node_lock);
		return;
	}
	/*
	 * The cpu supports 'cpuid' inst.
	 * get the highest value of eax. Vendor Id is returned in ebx, edx, ecx
	 */
	vendor = kmem_zalloc(CPUVEND_STR_SZ, KM_SLEEP);
	iptr = (uint_t *)vendor;
	/* vendor id string is made up of ebx, edx and ecx in that order */
	highest_eax = cpuid(0, iptr, iptr+2, iptr+1);
	create_prom_and_ddi_prop(nodeid, "vendor-id", vendor, cpu_devi,
		strlen(vendor) + 1);

	if ((x86_feature & X86_INTEL) && (highest_eax >= 1)) {
		/*
		 * Identify the cpu family, model and stepping-id.
		 */
		eax = cpuid(1, &ebx, &ecx, &edx);
		family = kmem_alloc(sizeof (uint32_t), KM_SLEEP);
		*family = (eax >> 8) & 0x0f;
		/*
		 * add the property 'family'
		 */
		create_prom_and_ddi_prop(nodeid, "family", (caddr_t)family,
			cpu_devi, sizeof (uint32_t));

		model = kmem_alloc(sizeof (uint32_t), KM_SLEEP);
		*model = (eax >> 4) & 0x0f;
		/*
		 * add the property 'model'
		 */
		create_prom_and_ddi_prop(nodeid, "model", (caddr_t)model,
			cpu_devi, sizeof (uint32_t));

		stepid = kmem_alloc(sizeof (uint32_t), KM_SLEEP);
		*stepid = eax & 0x0f;
		/*
		 * add the property 'stepping-id'
		 */
		create_prom_and_ddi_prop(nodeid, "stepping-id",
			(caddr_t)stepid, cpu_devi, sizeof (uint32_t));
	}
	if ((x86_feature & X86_INTEL) && (highest_eax >= 2)) {
	    eax = cpuid(2, &ebx, &ecx, &edx);
		/*
		 * Intel says we need to get the registers as many times as
		 * specified by LSbyte of eax.But, based on table specified
		 * by Intel, there cannot be more than 7 different entries
		 * and they can be specified by eax & edx. When the table
		 * is expanded, fix code & remove assert below. Reason why
		 * this is not coded now is because it is not clear if
		 * Intel will specify duplicate entries or do something
		 * odd in their first implementation of such a chip.
		 * Intel also recommends going from MSB to LSB for each
		 * register. Beats me why that is important.
		 */
	    ASSERT((eax & 0xff) == 1);

	    num_entries = sizeof (IA32_cache_d)/
		sizeof (struct  intel_cachedesc);

	    conf_descp = (uchar_t *)&cpuid_regs.reg_eax;
	    conf_descp += 15;	/* point it to last descriptor */
	    /* scan all 15 8 bit descriptors. We exclude the LSB of eax */
	    for (; conf_descp != (uchar_t *)&eax; conf_descp--) {
		if (INVALID_CACHE_DESC(*conf_descp) || (*conf_descp == 0))
		    continue;

		for (i = 0; i < num_entries; i++) {
		    if (IA32_cache_d[i].code == *conf_descp) {

/* define macro to add associativity, line size and size props of cache */
#define	ADD_CACHE_PROP(type_str, field) \
	(void) strcpy(label, IA32_cache_d[i].cache_lbl); \
	(void) strcat(label, type_str); \
	iptr = kmem_zalloc(sizeof (uint32_t), KM_SLEEP); \
	*iptr = (uint32_t)IA32_cache_d[i].field; \
	create_prom_and_ddi_prop(nodeid, label, (caddr_t)iptr, \
			cpu_devi, sizeof (uint32_t));

			ADD_CACHE_PROP(ASSOCIATIVITY_STR, associativity);
			ADD_CACHE_PROP(LINE_SIZE_STR, line_size);
			ADD_CACHE_PROP(SIZE_STR, size);
			break;
		    }
		}
	    }
	}
	mutex_exit(&cpu_node_lock);
}
