/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)startup.c	1.213	99/10/22 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/atomic.h>

#include <sys/disp.h>
#include <sys/class.h>
#include <sys/bitmap.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/vmem.h>
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
#include <sys/memlist.h>
#include <sys/memlist_plat.h>
#include <sys/memlist_impl.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/prom_isa.h>
#include <sys/prom_plat.h>
#include <sys/modctl.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/autoconf.h>
#include <sys/clock.h>
#include <sys/scb.h>
#include <sys/stack.h>
#include <sys/eeprom.h>
#include <sys/intreg.h>
#include <sys/ivintr.h>
#include <sys/trap.h>
#include <sys/x_call.h>
#include <sys/privregs.h>
#include <sys/fpu/fpusystm.h>

#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vmparam.h>
#include <vm/hat_sfmmu.h>
#include <sys/iommu.h>
#include <sys/vtrace.h>
#include <sys/instance.h>
#include <sys/kobj.h>
#include <sys/async.h>
#include <sys/spitasi.h>
#include <vm/mach_page.h>
#include <sys/clconf.h>
#include <sys/tuneable.h>
#include <sys/platform_module.h>
#include <sys/machparam.h>
#include <sys/panic.h>

#include <sys/prom_debug.h>
#ifdef TRAPTRACE
#include <sys/traptrace.h>
#endif /* TRAPTRACE */

#include <sys/memnode.h>

extern void parse_idprom(void);

/*
 * External Data:
 */
extern int vac_size;	/* cache size in bytes */
extern uint_t vac_mask;	/* VAC alignment consistency mask */

/*
 * Global Data Definitions:
 */


/*
 * XXX - Don't port this to new architectures
 * A 3rd party volume manager driver (vxdm) depends on the symbol romp.
 * 'romp' has no use with a prom with an IEEE 1275 client interface.
 * The driver doesn't use the value, but it depends on the symbol.
 */
void *romp;		/* veritas driver won't load without romp 4154976 */
/*
 * Declare these as initialized data so we can patch them.
 */
pgcnt_t physmem = 0;	/* memory size in pages, patch if you want less */
pgcnt_t segkpsize =
    btop(SEGKPDEFSIZE);	/* size of segkp segment in pages */
int kernprot = 1;	/* write protect kernel text */
uint_t segmap_percent = 12; /* Size of segmap segment */


#ifdef DEBUG
int forthdebug	= 1;	/* Load the forthdebugger module */
#else
int forthdebug	= 0;	/* Don't load the forthdebugger module */
#endif DEBUG

#define	FDEBUGSIZE (50 * 1024)
#define	FDEBUGFILE "misc/forthdebug"

int use_cache = 1;		/* cache not reliable (605 bugs) with MP */
int vac_copyback = 1;
char	*cache_mode = (char *)0;
int use_mix = 1;
int prom_debug = 0;
int usb_node_debug = 0;

struct bootops *bootops = 0;	/* passed in from boot in %o2 */
caddr_t boot_tba;
uint_t	tba_taken_over = 0;

/*
 * DEBUGADDR is where we expect the debugger to be if it's there.
 * We really should be allocating virtual addresses by looking
 * at the virt_avail list.
 */
#define	DEBUGADDR		((caddr_t)0xedd00000)

caddr_t s_text;			/* start of kernel text segment */
caddr_t e_text;			/* end of kernel text segment */
caddr_t s_data;			/* start of kernel data segment */
caddr_t e_data;			/* end of kernel data segment */

caddr_t modtext;		/* beginning of module text reserve */
caddr_t moddata;		/* beginning of module data reserve */
caddr_t e_moddata;		/* end of module data reserve */

caddr_t		econtig;	/* end of first block of contiguous kernel */
caddr_t		ncbase;		/* beginning of non-cached segment */
caddr_t		ncend;		/* end of non-cached segment */
caddr_t		sdata;		/* beginning of data segment */
caddr_t		extra_etva;	/* beginning of end of text - va */
uint64_t	extra_etpa;	/* beginning of end of text - pa */
size_t		extra_et;	/* end of text + mods to 4MB boundary */

size_t	ndata_remain_sz;	/* bytes from end of data to 4MB boundary */
caddr_t	nalloc_base;		/* beginning of nucleus allocation */
caddr_t nalloc_end;		/* end of nucleus allocatable memory */
caddr_t valloc_base;		/* beginning of kvalloc segment	*/

uintptr_t shm_alignment = 0;	/* VAC address consistency modulus */
struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
int memexp_flag;		/* memory expansion card flag */
uint64_t ecache_flushaddr;	/* physical address used for flushing E$ */
static uint64_t ecache_flush_address(void);

/*
 * VM data structures
 */
long page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
size_t  pp_sz;			/* Size in bytes of page struct array */
struct	page **page_hash;	/* Page hash table */
struct	seg *segkmap;		/* Kernel generic mapping segment */
struct	seg ktextseg;		/* Segment used for kernel executable image */
struct	seg kvalloc;		/* Segment used for "valloc" mapping */
struct	seg *segkp;		/* Segment for pageable kernel virt. memory */
struct	seg *seg_debug;		/* Segment for debugger */
struct	memseg *memseg_base;
size_t	memseg_sz;		/* Used to translate a va to page */
struct	vnode unused_pages_vp;

/*
 * VM data structures allocated early during boot.
 */
size_t pagehash_sz;
uint64_t memlist_sz;

char tbr_wr_addr_inited = 0;

/*
 * Saved beginning page frame for kernel .data and last page
 * frame for up to end[] for the kernel. These are filled in
 * by kvm_init().
 */
pfn_t kpfn_dataseg, kpfn_endbss;

/*
 * Static Routines:
 */
static void memlist_add(uint64_t, uint64_t, struct memlist **,
	struct memlist **);
static void kphysm_init(machpage_t *, struct memseg *, pgcnt_t);
static void kvm_init(void);

static void startup_init(void);
static void startup_memlist(void);
static void startup_modules(void);
static void startup_bop_gone(void);
static void startup_vm(void);
static void startup_end(void);
static void setup_cage_params(void);
static void setup_trap_table(void);
static caddr_t iommu_tsb_alloc(caddr_t);
static void startup_build_mem_nodes(void);
static void startup_create_input_node(void);

static pgcnt_t npages;
static ssize_t dbug_mem;
static ssize_t debug_start_va;
static struct memlist *memlist;
void *memlist_end;

/*
 * Hooks for unsupported platforms and down-rev firmware
 */
static int iam_positron(void);

#ifdef __sparcv9
static void do_prom_version_check(void);
#endif

/*
 * The following variables can be patched to override the auto-selection
 * of dvma space based on the amount of installed physical memory.
 */
int sbus_iommu_tsb_alloc_size = 0;
int pci_iommu_tsb_alloc_size = 0;

/*
 * After receiving a thermal interrupt, this is the number of seconds
 * to delay before shutting off the system, assuming
 * shutdown fails.  Use /etc/system to change the delay if this isn't
 * large enough.
 */
int thermal_powerdown_delay = 1200;

/*
 * Enable some debugging messages concerning memory usage...
 */
#ifdef  DEBUGGING_MEM
static int debugging_mem;
static void
printmemlist(char *title, struct memlist *list)
{
	if (!debugging_mem)
		return;

	printf("%s\n", title);

	while (list) {
		prom_printf("\taddr = 0x%x %8x, size = 0x%x %8x\n",
		    (uint32_t)(list->address >> 32), (uint32_t)list->address,
		    (uint32_t)(list->size >> 32), (uint32_t)(list->size));
		list = list->next;
	}
}

void
printmemseg(struct memseg *memseg)
{
	if (!debugging_mem)
		return;

	printf("memseg\n");

	while (memseg) {
		prom_printf("\tpage = 0x%p, epage = 0x%p, "
		"pfn = 0x%x, epfn = 0x%x\n",
			memseg->pages, memseg->epages,
			memseg->pages_base, memseg->pages_end);
		memseg = memseg->next;
	}
}

#define	debug_pause(str)	if (prom_getversion() > 0) halt((str))
#define	MPRINTF(str)		if (debugging_mem) prom_printf((str))
#define	MPRINTF1(str, a)	if (debugging_mem) prom_printf((str), (a))
#define	MPRINTF2(str, a, b)	if (debugging_mem) prom_printf((str), (a), (b))
#define	MPRINTF3(str, a, b, c) \
	if (debugging_mem) prom_printf((str), (a), (b), (c))
#else	/* DEBUGGING_MEM */
#define	MPRINTF(str)
#define	MPRINTF1(str, a)
#define	MPRINTF2(str, a, b)
#define	MPRINTF3(str, a, b, c)
#endif	/* DEBUGGING_MEM */

/* Simple message to indicate that the bootops pointer has been zeroed */
#ifdef DEBUG
static int bootops_gone_on = 0;
#define	BOOTOPS_GONE() \
	if (bootops_gone_on) \
		prom_printf("The bootops vec is zeroed now!\n");
#else
#define	BOOTOPS_GONE()
#endif DEBUG

/*
 * Monitor pages may not be where this says they are.
 * and the debugger may not be there either.
 *
 * Note that 'pages' here are *physical* pages, which are 8k on sun4u.
 *
 *                        Physical memory layout
 *                     (not necessarily contiguous)
 *                       (THIS IS SOMEWHAT WRONG)
 *                       /-----------------------\
 *                       |       monitor pages   |
 *             availmem -|-----------------------|
 *                       |                       |
 *                       |       page pool       |
 *                       |                       |
 *                       |-----------------------|
 *                       |   configured tables   |
 *                       |       buffers         |
 *            firstaddr -|-----------------------|
 *                       |   hat data structures |
 *                       |-----------------------|
 *                       |    kernel data, bss   |
 *                       |-----------------------|
 *                       |    interrupt stack    |
 *                       |-----------------------|
 *                       |    kernel text (RO)   |
 *                       |-----------------------|
 *                       |    trap table (4k)    |
 *                       |-----------------------|
 *               page 1  |      panicbuf         |
 *                       |-----------------------|
 *               page 0  |       reclaimed       |
 *                       |_______________________|
 *
 *
 *
 *                  32-bit Kernel's Virtual Memory Layout.
 *          0xFFFFFFFF   /-----------------------\ OFW_END_ADDR
 *                       |                       |
 *                       |       OBP             |
 *                       |                       |
 *          0xF0000000  -|-----------------------| OFW_START_ADDR
 *                       |       kadb            |
 *          0xEDD00000  -|-----------------------|- SYSLIMIT
 *                       |                       |
 *                       |                       |
 *                       |  segkmem segment      |  (SYSLIMIT - SYSBASE = ~2G)
 *                       |                       |
 *                       |                       |
 *          0x60000000  -|-----------------------|- SYSBASE
 *                       |                       |
 *                       |  segmap segment       |   SEGMAPSIZE      (256M)
 *                       |                       |
 *          0x50000000  -|-----------------------|- SEGMAPBASE
 *                       |                       |
 *                       |       segkp           |   SEGKPSIZE       (512M)
 *                       |                       |
 *          0x30000000  -|-----------------------|- SEGKPBASE
 *                       |                       |
 *                      -|-----------------------|- MEMSCRUBBASE
 *                       |                       |   (SEGKPBASE - 0x400000)
 *                      -|-----------------------|- ARGSBASE
 *                       |                       |   (MEMSCRUBBASE - NCARGS)
 *                      -|-----------------------|- PPMAPBASE
 *                       |                       |   (ARGSBASE - PPMAPSIZE)
 *                      -|-----------------------|- PPMAP_FAST_BASE
 *                       |                       |
 *                      -|-----------------------|- NARG_BASE
 *                       :                       :
 *                       |                       |
 *                       |-----------------------|- econtig
 *                       |    vm structures      |
 *          0x10800000   |-----------------------|- nalloc_end
 *                       |         tsb           |
 *                       |-----------------------|
 *                       |    hmeblk pool        |
 *                       |-----------------------|
 *                       |    hmeblk hashtable   |
 *                       |-----------------------|- end/nalloc_base
 *                       |  kernel data & bss    |
 *          0x10400000   |-----------------------|
 *                       |                       |
 *                       |-----------------------|- etext
 *                       |       kernel text     |
 *                       |-----------------------|
 *                       |   trap table (48k)    |
 *          0x10000000  -|-----------------------|- KERNELBASE
 *                       |                       |
 *                       |       invalid         |
 *                       |                       |
 *          0x00000000  _|_______________________|
 *
 *
 *
 *                  64-bit Kernel's Virtual Memory Layout.
 *                       /-----------------------\
 * 0xFFFFFFFF.FFFFFFFF  -|                       |-
 *                       |   OBP's virtual page  |
 *                       |        tables         |
 * 0xFFFFFFFC.00000000  -|-----------------------|-
 *                       :                       :
 *                       :                       :
 * 0x00000310.00000000  -|-----------------------|- SYSLIMIT
 *                       |                       |
 *                       |  segkmem segment      | (SYSLIMIT - SYSBASE = 64GB)
 *                       |                       |
 * 0x00000300.00000000  -|-----------------------|- SYSBASE
 *                       :                       :
 *                       :                       :
 *                      -|-----------------------|-
 *                       |                       |
 *                       |  segmap segment       |   SEGMAPSIZE (1/8th physmem,
 *                       |                       |               256G MAX)
 * 0x000002a7.50000000  -|-----------------------|- SEGMAPBASE
 *                       :                       :
 *                       :                       :
 *                      -|-----------------------|-
 *                       |                       |
 *                       |       segkp           |    SEGKPSIZE (512M)
 *                       |                       |
 *                       |                       |
 * 0x000002a1.00000000  -|-----------------------|- SEGKPBASE
 *                       |                       |
 * 0x000002a0.00000000  -|-----------------------|- MEMSCRUBBASE
 *                       |                       |       (SEGKPBASE - 0x400000)
 * 0x0000029F.FFE00000  -|-----------------------|- ARGSBASE
 *                       |                       |       (MEMSCRUBBASE - NCARGS)
 * 0x0000029F.FFD80000  -|-----------------------|- PPMAPBASE
 *                       |                       |       (ARGSBASE - PPMAPSIZE)
 * 0x0000029F.FFD00000  -|-----------------------|- PPMAP_FAST_BASE
 *                       |                       |
 * 0x0000029F.FF900000  -|-----------------------|- NARG_BASE
 *                       :                       :
 *                       :                       :
 * 0x00000000.FFFFFFFF  -|-----------------------|- OFW_END_ADDR
 *                       |                       |
 *                       |         OBP           |
 *                       |                       |
 * 0x00000000.F0000000  -|-----------------------|- OFW_START_ADDR
 *                       |         kadb          |
 * 0x00000000.EDD00000  -|-----------------------|-
 *                       :                       :
 *                       :                       :
 * 0x00000000.7c000000  -|-----------------------|- SYSLIMIT32
 *                       |                       |
 *                       |  segkmem32 segment    | (SYSLIMIT32 - SYSBASE32 =
 *                       |                       |    ~64MB)
 * 0x00000000.78002000  -|-----------------------|
 *                       |     panicbuf          |
 * 0x00000000.78000000  -|-----------------------|- SYSBASE32
 *                       :                       :
 *                       :                       :
 *                       |                       |
 *                       |-----------------------|- econtig
 *                       |    vm structures      |
 * 0x00000000.10800000   |-----------------------|- nalloc_end
 *                       |         tsb           |
 *                       |-----------------------|
 *                       |    hmeblk pool        |
 *                       |-----------------------|
 *                       |    hmeblk hashtable   |
 *                       |-----------------------|- end/nalloc_base
 *                       |  kernel data & bss    |
 * 0x00000000.10400000   |-----------------------|
 *                       |                       |
 *                       |-----------------------|- etext
 *                       |       kernel text     |
 *                       |-----------------------|
 *                       |   trap table (48k)    |
 * 0x00000000.10000000  -|-----------------------|- KERNELBASE
 *                       |                       |
 *                       |       invalid         |
 *                       |                       |
 * 0x00000000.00000000  _|_______________________|
 *
 *
 *
 *                   32-bit User Virtual Memory Layout.
 *                       /-----------------------\
 *                       |                       |
 *                       |        invalid        |
 *                       |                       |
 *          0xF0000000  -|-----------------------|- USERLIMIT
 *                       |       user stack      |
 *                       :                       :
 *                       :                       :
 *                       :                       :
 *                       |       user data       |
 *                      -|-----------------------|-
 *                       |       user text       |
 *          0x00002000  -|-----------------------|-
 *                       |       invalid         |
 *          0x00000000  _|_______________________|
 *
 *
 *
 *                   64-bit User Virtual Memory Layout.
 *                       /-----------------------\
 *                       |                       |
 *                       |        invalid        |
 *  0xFFFFFFFc.00000000  |                       |
 *                      -|-----------------------|- USERLIMIT
 *                       |       user stack      |
 *                       :                       :
 *                       :                       :
 *                       :                       :
 *                       |       user data       |
 *                      -|-----------------------|-
 *                       |       user text       |
 *  0x00000000.00100000 -|-----------------------|-
 *                       |       invalid         |
 *  0x00000000.00000000 _|_______________________|
 */

static void
setup_cage_params(void)
{
	void (*func)(void);

	func = (void (*)(void))kobj_getsymvalue("set_platform_cage_params", 0);
	if (func)
		(*func)();
}

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	startup_init();
	startup_memlist();
	startup_modules();
	setup_cage_params();
	startup_bop_gone();
	startup_vm();
	startup_end();
}

static void
startup_init(void)
{
	char bp[100];
	char sync_str[] =
	    "warning @ warning off : sync h# %p set-pc go ; warning !";
	extern int callback_handler(cell_t *arg_array);
	extern void sync_callback(void);

	(void) check_boot_version(BOP_GETVERSION(bootops));

	/*
	 * Initialize the address map for cache consistent mappings
	 * to random pages, must done after vac_size is set.
	 */
	ppmapinit();

	/*
	 * Initialize the PROM callback handler and install the PROM
	 * callback handler. For this 32 bit client program, we install
	 * "callback_handler" which is the glue that binds the 64 bit
	 * prom callback handler to the 32 bit client program callback
	 * handler: vx_handler.
	 */
	init_vx_handler();
	(void) prom_set_callback((void *)callback_handler);

	/*
	 * have prom call sync_callback() to handle the sync
	 */
	(void) sprintf((char *)bp, sync_str, sync_callback);
	prom_interpret(bp, 0, 0, 0, 0, 0);
}

static u_longlong_t *boot_physinstalled, *boot_physavail, *boot_virtavail;
static size_t boot_physinstalled_len, boot_physavail_len, boot_virtavail_len;

#define	IVSIZE	(MAXIVNUM * sizeof (struct intr_vector))

static void
startup_memlist(void)
{
	size_t real_sz;
	size_t ctrs_sz;
	caddr_t real_base;
	caddr_t alloc_base;
	int memblocks = 0;
	caddr_t memspace;
	struct memlist *cur;
	size_t syslimit = (size_t)SYSLIMIT;
	size_t sysbase = (size_t)SYSBASE;
	caddr_t va;
	caddr_t bop_alloc_base;
	long more_hblks = 0;

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

	/*
	 * We're loaded by boot with the following configuration (as
	 * specified in the sun4u/conf/Mapfile):
	 *
	 * 	text:		4 MB chunk aligned on a 4MB boundary
	 * 	data & bss:	4 MB chunk aligned on a 4MB boundary
	 *
	 * These two chunks will eventually be mapped by 2 locked 4MB
	 * ttes and will represent the nucleus of the kernel.  This gives
	 * us some free space that is already allocated.
	 * 2MB of this free space is reserved for kernel modules text.
	 * The rest of the free space in the text chunk is currently
	 * returned to the physavail list.
	 *
	 * The free space in the data-bss chunk is used for nucleus
	 * allocatable data structures and we reserve it using the
	 * nalloc_base and nalloc_end variables.  This space is currently
	 * being used for hat data structures required for tlb miss
	 * handling operations.  We align nalloc_base to a l2 cache
	 * linesize because this is the line size the hardware uses to
	 * maintain cache coherency.
	 * 256K is carved out for module data.
	 */

	nalloc_base = (caddr_t)roundup((uintptr_t)e_data, MMU_PAGESIZE);
	moddata = nalloc_base;
	e_moddata = nalloc_base + MODDATA;
	nalloc_base = e_moddata;

	nalloc_end = (caddr_t)roundup((uintptr_t)nalloc_base, MMU_PAGESIZE4M);
	valloc_base = nalloc_base;

	/*
	 * Calculate the start of the data segment.
	 */
	sdata = (caddr_t)((uintptr_t)e_data & MMU_PAGEMASK4M);

	PRM_DEBUG(moddata);
	PRM_DEBUG(nalloc_base);
	PRM_DEBUG(nalloc_end);
	PRM_DEBUG(sdata);

	/*
	 * Remember any slop after e_text so we can add it to the
	 * physavail list.
	 */
	PRM_DEBUG(e_text);
	modtext = (caddr_t)roundup((uintptr_t)e_text, MMU_PAGESIZE);
	extra_etva = (caddr_t)roundup((uintptr_t)modtext + MODTEXT,
		MMU_PAGESIZE);
	PRM_DEBUG(modtext);
	PRM_DEBUG((uintptr_t)extra_etva);
	extra_etpa = va_to_pa(extra_etva);
	PRM_DEBUG(extra_etpa);
	if (extra_etpa != (uint64_t)-1) {
		extra_et = roundup((uintptr_t)modtext + MODTEXT,
			MMU_PAGESIZE4M) - (uintptr_t)extra_etva;
	} else {
		extra_et = 0;
		modtext = 0;
	}
	PRM_DEBUG(extra_et);

	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);
	/*
	 * Remember what the physically available highest page is
	 * so that dumpsys works properly, and find out how much
	 * memory is installed.
	 */
	installed_top_size_memlist_array(boot_physinstalled,
	    boot_physinstalled_len, &physmax, &physinstalled);
	PRM_DEBUG(physinstalled);
	PRM_DEBUG(physmax);

	/* Fill out memory nodes config structure */
	startup_build_mem_nodes();

	/*
	 * Get the list of physically available memory to size
	 * the number of page structures needed.
	 */
	size_physavail(boot_physavail, boot_physavail_len, &npages, &memblocks);

	/* Account for any pages after e_text and e_data */
	npages += mmu_btop(extra_et);
	npages += mmu_btopr(nalloc_end - nalloc_base);
	PRM_DEBUG(npages);

	/*
	 * npages is the maximum of available physical memory possible.
	 * (ie. it will never be more than this)
	 */

	/*
	 * Allocate cpus structs from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = (caddr_t)roundup((uintptr_t)ndata_alloc_cpus(nalloc_base),
	    ecache_linesize);

	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC, "no more nucleus memory after cpu alloc");
	}

	/*
	 * Allocate dmv dispatch table from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = (caddr_t)roundup((uintptr_t)ndata_alloc_dmv(nalloc_base),
	    ecache_linesize);

	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC, "no more nucleus memory after dmv alloc");
	}

	/*
	 * Allocate page_freelists bin headers from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = ndata_alloc_page_freelists(nalloc_base);
	nalloc_base = (caddr_t)roundup((uintptr_t)nalloc_base,
	    ecache_linesize);

	if (nalloc_base > nalloc_end) {
		cmn_err(CE_PANIC,
		    "no more nucleus memory after page free lists alloc");
	}

	/*
	 * Allocate hat related structs from the nucleus data area and
	 * update nalloc_base.
	 */
	nalloc_base = ndata_alloc_hat(nalloc_base, nalloc_end, npages,
	    &more_hblks);
	nalloc_base = (caddr_t)roundup((uintptr_t)nalloc_base,
	    ecache_linesize);
	if (nalloc_base > nalloc_end)
		cmn_err(CE_PANIC, "no more nucleus memory after hat alloc");

	/*
	 * Given our current estimate of npages we do a premature calculation
	 * on how much memory we are going to need to support this number of
	 * pages.  This allows us to calculate a good start virtual address
	 * for other BOP_ALLOC operations.
	 * We want to do the BOP_ALLOCs before the real allocation of page
	 * structs in order to not have to allocate page structs for this
	 * memory.  We need to calculate a virtual address because we want
	 * the page structs to come before other allocations in virtual address
	 * space.  This is so some (if not all) of page structs can actually
	 * live in the nucleus.
	 */
	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit((ulong_t)page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * Size up per page size free list counters.
	 */
	ctrs_sz = page_ctrs_sz();

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kphysm_init().  Currently, there are two
	 * allocations before then, so we assume each causes fragmen-
	 * tation, and add a couple more for good measure.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + 4);
	pp_sz = sizeof (struct machpage) * npages;

	real_sz = pagehash_sz + memseg_sz + pp_sz + ctrs_sz;
	PRM_DEBUG(real_sz);

	bop_alloc_base = (caddr_t)roundup((uintptr_t)nalloc_end + real_sz,
	    MMU_PAGESIZE);
	PRM_DEBUG(bop_alloc_base);

	/*
	 * Add other BOP_ALLOC operations here
	 */
	alloc_base = bop_alloc_base;

	/*
	 * See if we need to bop_alloc more hblks
	 * This happens when the nucleus don't have enough
	 * space to accommodate the initial hblks allocation
	 * in ndata_alloc_hat()
	 */
	if (more_hblks) {
		alloc_base = alloc_more_hblks(alloc_base, more_hblks);
	}

	alloc_base = sfmmu_tsb_alloc(alloc_base, npages);
	alloc_base = (caddr_t)roundup((uintptr_t)alloc_base, ecache_linesize);
	PRM_DEBUG(alloc_base);
#ifdef	TRAPTRACE
	alloc_base = trap_trace_alloc(alloc_base);
#endif	/* TRAPTRACE */

	/*
	 * Allocate IOMMU TSB array.  We do this here so that the physical
	 * memory gets deducted from the PROM's physical memory list.
	 */
	alloc_base = (caddr_t)roundup((uintptr_t)iommu_tsb_alloc(alloc_base),
	    ecache_linesize);
	PRM_DEBUG(alloc_base);

	/*
	 * The only left to allocate for the kvalloc segment should be the
	 * vm data structures.
	 */

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);
	npages = 0;
	memblocks = 0;
	size_physavail(boot_physavail, boot_physavail_len, &npages, &memblocks);
	PRM_DEBUG(npages);
	/* account for memory after etext */
	npages += mmu_btop(extra_et);

	/*
	 * Calculate the remaining memory in nucleus data area.
	 * We need to figure out if page structs can fit in there or not.
	 * We also make sure enough page structs get created for any physical
	 * memory we might be returning to the system.
	 */
	ndata_remain_sz = (size_t)(nalloc_end - nalloc_base);
	PRM_DEBUG(ndata_remain_sz);
	pp_sz = sizeof (struct machpage) * npages;
	if (ndata_remain_sz > pp_sz) {
		npages += mmu_btop(ndata_remain_sz - pp_sz);
	}
	/*
	 * If physmem is patched to be non-zero, use it instead of
	 * the monitor value unless physmem is larger than the total
	 * amount of memory on hand.
	 */
	if (physmem == 0 || physmem > npages)
		physmem = npages;

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit((ulong_t)page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kphysm_init().  Currently, there are two
	 * allocations before then, so we assume each causes fragmen-
	 * tation, and add a couple more for good measure.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + 4);
	pp_sz = sizeof (struct machpage) * npages;
	real_sz = pagehash_sz + memseg_sz;
	real_sz = roundup(real_sz, ecache_linesize) + pp_sz;
	real_sz = roundup(real_sz, ecache_linesize) + ctrs_sz;
	PRM_DEBUG(real_sz);

	/*
	 * Allocate the page structures from the remaining memory in the
	 * nucleus data area.
	 */
	real_base = nalloc_base;

	if (ndata_remain_sz >= real_sz) {
		/*
		 * Figure out the base and size of the remaining memory.
		 */
		nalloc_base += real_sz;
		ASSERT(nalloc_base <= nalloc_end);
		ndata_remain_sz = nalloc_end - nalloc_base;
	} else if (ndata_remain_sz < real_sz) {
		/*
		 * The page structs need extra memory allocated through
		 * BOP_ALLOC.
		 */
		real_sz = roundup((real_sz - ndata_remain_sz),
		    MMU_PAGESIZE);
		memspace = (caddr_t)BOP_ALLOC(bootops, nalloc_end, real_sz,
		    MMU_PAGESIZE);
		if (memspace != nalloc_end)
			panic("system page struct alloc failure");

		nalloc_base = nalloc_end;
		ndata_remain_sz = 0;
		if ((nalloc_end + real_sz) > bop_alloc_base) {
			prom_panic("vm structures overwrote other bop alloc!");
		}
	}
	PRM_DEBUG(nalloc_base);
	PRM_DEBUG(ndata_remain_sz);
	PRM_DEBUG(real_base + real_sz);
	nalloc_base = (caddr_t)roundup((uintptr_t)nalloc_base, MMU_PAGESIZE);
	ndata_remain_sz = nalloc_end - nalloc_base;

	page_hash = (struct page **)real_base;

	memseg_base = (struct memseg *)
	roundup((uintptr_t)page_ctrs_alloc((caddr_t)((uintptr_t)page_hash +
		pagehash_sz)),
		ecache_linesize);

	pp_base = (struct machpage *)roundup((uintptr_t)memseg_base + memseg_sz,
	    ecache_linesize);

	ASSERT(((uintptr_t)pp_base + pp_sz) <= (uintptr_t)bop_alloc_base);

	PRM_DEBUG(page_hash);
	PRM_DEBUG(memseg_base);
	PRM_DEBUG(pp_base);
	econtig = alloc_base;
	if (econtig > (caddr_t)KERNEL_LIMIT)
		cmn_err(CE_PANIC, "econtig too big");
	PRM_DEBUG(econtig);

	/*
	 * Allocate space for the interrupt vector table.
	 */
	memspace = (caddr_t)BOP_ALLOC(bootops, (caddr_t)intr_vector,
	    IVSIZE, MMU_PAGESIZE);
	if (memspace != (caddr_t)intr_vector)
		panic("interrupt table allocation failure");

	/*
	 * the memory lists from boot are allocated from the heap arena
	 * so that later they can be freed and/or reallocated.
	 */
	if (BOP_GETPROP(bootops, "extent", &memlist_sz) == -1)
		panic("could not retrieve property \"extent\"");
	/*
	 * Between now and when we finish copying in the memory lists,
	 * allocations happen so the space gets fragmented and the
	 * lists longer.  Leave enough space for lists twice as long
	 * as what boot says it has now; roundup to a pagesize.
	 * Also add space for the final phys-avail copy in the fixup
	 * routine.
	 */
	va = (caddr_t)(sysbase + PAGESIZE + PANICBUFSIZE +
	    roundup(IVSIZE, MMU_PAGESIZE));
	memlist_sz *= 4;
	memlist_sz = roundup(memlist_sz, MMU_PAGESIZE);
	memspace = (caddr_t)BOP_ALLOC(bootops, va, memlist_sz, BO_NO_ALIGN);
	if (memspace == NULL)
		halt("Boot allocation failed.");

	memlist = (struct memlist *)memspace;
	memlist_end = (char *)memspace + memlist_sz;

	kernelheap_init((void *)sysbase, (void *)syslimit,
	    (caddr_t)sysbase + PAGESIZE);

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);

	/*
	 * Remove the space used by BOP_ALLOC from the kernel heap
	 * plus the area actually used by the OBP (if any)
	 * ignoring virtual addresses in virt_avail, above SYSLIMIT.
	 */

	virt_avail = memlist;
	copy_memlist(boot_virtavail, boot_virtavail_len, &memlist);

	for (cur = virt_avail; cur->next; cur = cur->next) {
		uint64_t range_base, range_size;

		if ((range_base = cur->address + cur->size) < (uint64_t)sysbase)
			continue;
		if (range_base >= (uint64_t)syslimit)
			break;
		/*
		 * Limit the range to end at SYSLIMIT.
		 */
		range_size = MIN(cur->next->address,
		    (uint64_t)syslimit) - range_base;
		(void) vmem_xalloc(heap_arena, (size_t)range_size, PAGESIZE,
		    0, 0, (void *)range_base, (void *)(range_base + range_size),
		    VM_NOSLEEP | VM_BESTFIT | VM_PANIC);
	}

	phys_avail = memlist;
	(void) copy_physavail(boot_physavail, boot_physavail_len,
	    &memlist, 0, 0);

	/*
	 * Add any extra mem after e_text to physavail list.
	 */
	if (extra_et) {
		memlist_add(extra_etpa, (uint64_t)extra_et, &memlist,
			&phys_avail);
	}
	/*
	 * Add any extra nucleus mem to physavail list.
	 */
	if (ndata_remain_sz) {
		ASSERT(nalloc_end == (nalloc_base + ndata_remain_sz));
		memlist_add(va_to_pa(nalloc_base),
			(uint64_t)ndata_remain_sz, &memlist, &phys_avail);
	}

	if ((caddr_t)memlist > (memspace + memlist_sz))
		panic("memlist overflow");

	/*
	 * Initialize the page structures from the memory lists.
	 */
	kphysm_init(pp_base, memseg_base, npages);

	availrmem_initial = availrmem = freemem;
	PRM_DEBUG(availrmem);

	/*
	 * Some of the locks depend on page_hashsz being set!
	 * kmem_init() depends on this; so, keep it here.
	 */
	page_lock_init();

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize bp_mapin().
	 */
	bp_init(shm_alignment, HAT_STRICTORDER);

#ifdef __sparcv9
	/*
	 * Reserve space for panicbuf and intr_vector from the 32-bit heap
	 */
	(void) vmem_xalloc(heap32_arena, PANICBUFSIZE, PAGESIZE, 0, 0,
	    panicbuf, panicbuf + PANICBUFSIZE,
	    VM_NOSLEEP | VM_BESTFIT | VM_PANIC);

	(void) vmem_xalloc(heap32_arena, IVSIZE, PAGESIZE, 0, 0,
	    intr_vector, (caddr_t)intr_vector + IVSIZE,
	    VM_NOSLEEP | VM_BESTFIT | VM_PANIC);
#endif
}

static void
startup_modules(void)
{
	int proplen;

	/* Log any optional messages from the boot program */

	proplen = (size_t)BOP_GETPROPLEN(bootops, "boot-message");
	if (proplen > 0) {
		size_t len = (size_t)proplen;
		char *msg;

		msg = kmem_zalloc(len, KM_SLEEP);
		(void) BOP_GETPROP(bootops, "boot-message", msg);
		cmn_err(CE_CONT, "?%s\n", msg);
		kmem_free(msg, len);
	}

	/*
	 * Let the platforms have a chance to change default
	 * values before reading system file.
	 */
	set_platform_defaults();

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	param_calc(0);

	mod_setup();

	/*
	 * If this is a positron, complain and halt.
	 */
	if (iam_positron()) {
		cmn_err(CE_WARN, "This hardware platform is not supported"
		    " by this release of Solaris.\n");
#ifdef DEBUG
		prom_enter_mon();	/* Type 'go' to resume */
		cmn_err(CE_WARN, "Booting an unsupported platform.\n");

#ifdef __sparcv9
		cmn_err(CE_WARN, "Booting with down-rev firmware.\n");
#endif /* __sparcv9 */

#else /* DEBUG */
		halt(0);
#endif /* DEBUG */
	}

#ifdef __sparcv9
	/*
	 * If we are running firmware that isn't 64-bit ready
	 * then complain and halt.
	 */
	do_prom_version_check();
#endif /* __sparcv9 */


	/*
	 * Initialize system parameters
	 */
	param_init();

	/*
	 * If debugger is in memory, note the pages it stole from physmem.
	 * XXX: Should this happen with V2 Proms?  I guess it would be
	 * set to zero in this case?
	 */
	if (boothowto & RB_DEBUG)
		dbug_mem = *dvec->dv_pages;
	else
		dbug_mem = 0;

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/* Set segkp limits. */
	ncbase = DEBUGADDR;
	ncend = DEBUGADDR;

	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

	/*
	 * Create the va>tte handler, so the prom can understand
	 * kernel translations.  The handler is installed later, just
	 * as we are about to take over the trap table from the prom.
	 */
	create_va_to_tte();

	/*
	 * If obpdebug or forthdebug is set, load the obpsym kernel
	 * symbol support module, now.
	 */
	if ((obpdebug) || (forthdebug)) {
		obpdebug = 1;
		(void) modload("misc", "obpsym");
	}

	/*
	 * Load the forthdebugger if forthdebug is set.
	 */
	if (forthdebug)
		forthdebug_init();


	/*
	 * Create OBP node for console input callbacks
	 * if it is needed.
	 */
	startup_create_input_node();

	if (modloadonly("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit();

	/*
	 * Infer meanings to the members of the idprom buffer.
	 */
	parse_idprom();

	/* Read cluster configuration data. */
	clconf_init();

	setup_ddi();

	/*
	 * Lets take this opportunity to load the root device.
	 */
	if (loadrootmodules() != 0)
		debug_enter("Can't load the root filesystem");

	/*
	 * Load tod driver module for the tod part found on this system.
	 * Recompute the cpu frequency/delays based on tod as tod part
	 * tends to keep time more accurately.
	 */
	if (tod_module_name == NULL || modload("tod", tod_module_name) == -1)
		halt("Can't load tod module");

	setcpudelay();
}

static void
startup_bop_gone(void)
{
	/*
	 * Call back into boot and release boots resources.
	 */
	BOP_QUIESCE_IO(bootops);

	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);
	/*
	 * Copy physinstalled list into kernel space.
	 */
	phys_install = memlist;
	copy_memlist(boot_physinstalled, boot_physinstalled_len, &memlist);

	/*
	 * setup physically contiguous area twice as large as the ecache.
	 * this is used while doing displacement flush of ecaches
	 */
	ecache_flushaddr = ecache_flush_address();
	if (ecache_flushaddr == (uint64_t)-1) {
		cmn_err(CE_PANIC, "startup: no memory to set ecache_flushaddr");
	}

	/*
	 * Virtual available next.
	 */
	ASSERT(virt_avail != NULL);
	memlist_free_list(virt_avail);
	virt_avail = memlist;
	copy_memlist(boot_virtavail, boot_virtavail_len, &memlist);

	/*
	 * Last chance to ask our booter questions ..
	 */

	/*
	 * For checkpoint-resume:
	 * Get kadb start address from prom "debugger-start" property,
	 * which is the same as segkp_limit at this point.
	 */
	debug_start_va = 0;
	(void) BOP_GETPROP(bootops, "debugger-start", (caddr_t)&debug_start_va);
}

/*
 * Find a physically contiguous area of twice the largest ecache size
 * to be used while doing displacement flush of ecaches.
 */
static uint64_t
ecache_flush_address()
{
	struct memlist *pmem;
	uint64_t flush_size;

	flush_size = ecache_size * 2;
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (pmem->size >= flush_size) {
			return (pmem->address);
		}
	}
	return ((uint64_t)-1);
}

/*
 * Called with the memlist lock held to say that phys_install has
 * changed.
 */
void
phys_install_has_changed()
{
	uint64_t new_addr;

	/*
	 * Get the new address into a temporary just in case panic'ing
	 * involves use of ecache_flushaddr.
	 */
	new_addr = ecache_flush_address();
	if (new_addr == (uint64_t)-1) {
		cmn_err(CE_PANIC,
		    "ecache_flush_address(): failed, ecache_size=%x",
		    ecache_size);
		/*NOTREACHED*/
	}
	ecache_flushaddr = new_addr;
	membar_producer();
}

/*
 * startup_fixup_physavail - called from mach_sfmmu.c after the final
 * allocations have been performed.  We can't call it in startup_bop_gone
 * since later operations can cause obp to allocate more memory.
 */
void
startup_fixup_physavail(void)
{
	struct memlist *cur;

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	copy_boot_memlists(&boot_physinstalled, &boot_physinstalled_len,
	    &boot_physavail, &boot_physavail_len,
	    &boot_virtavail, &boot_virtavail_len);

	/*
	 * Copy phys_avail list, again.
	 * Both the kernel/boot and the prom have been allocating
	 * from the original list we copied earlier.
	 */
	cur = memlist;
	(void) copy_physavail(boot_physavail, boot_physavail_len,
	    &memlist, 0, 0);

	/*
	 * Make sure we add any memory we added back to the old list.
	 */
	if (extra_et) {
		memlist_add(extra_etpa, (uint64_t)extra_et, &memlist,
		    &cur);
	}
	if (ndata_remain_sz) {
		memlist_add(va_to_pa(nalloc_base),
		    (uint64_t)ndata_remain_sz, &memlist, &cur);
	}

	/*
	 * There isn't any bounds checking on the memlist area
	 * so ensure it hasn't overgrown.
	 */
	if ((caddr_t)memlist > (caddr_t)memlist_end)
		cmn_err(CE_PANIC, "startup: memlist size exceeded");

	/*
	 * The kernel removes the pages that were allocated for it from
	 * the freelist, but we now have to find any -extra- pages that
	 * the prom has allocated for it's own book-keeping, and remove
	 * them from the freelist too. sigh.
	 */
	fix_prom_pages(phys_avail, cur);

	ASSERT(phys_avail != NULL);
	memlist_free_list(phys_avail);
	phys_avail = cur;

	/*
	 * We're done with boot.  Just after this point in time, boot
	 * gets unmapped, so we can no longer rely on its services.
	 * Zero the bootops to indicate this fact.
	 */
	bootops = (struct bootops *)NULL;
	BOOTOPS_GONE();
}

static void
startup_vm(void)
{
	size_t	i;
	struct segmap_crargs a;
	uint64_t avmem;
	caddr_t va;
	pgcnt_t	max_phys_segkp;
	int	mnode;

	/*
	 * get prom's mappings, create hments for them and switch
	 * to the kernel context.
	 */
	hat_kern_setup();

	/*
	 * Take over trap table
	 */
	setup_trap_table();

	/*
	 * Install the va>tte handler, so that the prom can handle
	 * misses and understand the kernel table layout in case
	 * we need call into the prom.
	 */
	install_va_to_tte();

	/*
	 * Set a flag to indicate that the tba has been taken over.
	 */
	tba_taken_over = 1;

	/*
	 * The boot cpu can now take interrupts, x-calls, x-traps
	 */
	CPUSET_ADD(cpu_ready_set, CPU->cpu_id);
	CPU->cpu_flags |= (CPU_READY | CPU_ENABLE | CPU_EXISTS);

	/*
	 * Set a flag to tell write_scb_int() that it can access V_TBR_WR_ADDR.
	 */
	tbr_wr_addr_inited = 1;

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();

	/*
	 * XXX4U: previously, we initialized and turned on
	 * the caches at this point. But of course we have
	 * nothing to do, as the prom has already done this
	 * for us -- main memory must be E$able at all times.
	 */

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
		pgcnt_t diff, off;
		struct page *pp;
		struct seg kseg;

		cmn_err(CE_WARN, "limiting physmem to %ld pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		kseg.s_as = &kas;
		while (diff--) {
			pp = page_create_va(&unused_pages_vp, (offset_t)off,
			    MMU_PAGESIZE, PG_WAIT | PG_EXCL,
			    &kseg, (caddr_t)off);
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
	 */
	cmn_err(CE_CONT, "?mem = %ldK (0x%lx000)\n",
	    (ulong_t)(physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    (ulong_t)(physinstalled - dbug_mem) << (PAGESHIFT - 12));

	avmem = (uint64_t)freemem << PAGESHIFT;
	cmn_err(CE_CONT, "?avail mem = %lld\n", (unsigned long long)avmem);

	/*
	 * Coalesce the freelist.
	 */
	for (mnode = 0; mnode < MAX_MEM_NODES; mnode++) {
		if (mem_node_config[mnode].exists) {
			page_freelist_coalesce(mnode);
			plat_freelist_process(mnode);
		}
	}

	/*
	 * Initialize the segkp segment type.  We position it
	 * after the configured tables and buffers (whose end
	 * is given by econtig) and before V_WKBASE_ADDR.
	 * Also in this area are the debugger (if present)
	 * and segkmap (size SEGMAPSIZE).
	 */

	/* XXX - cache alignment? */
	va = (caddr_t)SEGKPBASE;
	ASSERT(((uintptr_t)va & PAGEOFFSET) == 0);

	max_phys_segkp = (physmem * 2);

	if (segkpsize < btop(SEGKPMINSIZE) || segkpsize > btop(SEGKPMAXSIZE)) {
		segkpsize = btop(SEGKPDEFSIZE);
		cmn_err(CE_WARN, "Illegal value for segkpsize. "
		    "segkpsize has been reset to %ld pages", segkpsize);
	}

	i = ptob(MIN(segkpsize, max_phys_segkp));

	/*
	 * 1201049: segkmap assumes that its segment base and size are
	 * at least MAXBSIZE aligned.  We can guarantee this without
	 * introducing a hole in the kernel address space by ensuring
	 * that the previous segment -- segkp -- *ends* on a MAXBSIZE
	 * boundary.  (Avoiding a hole between segkp and segkmap is just
	 * paranoia in case anyone assumes that they're contiguous.)
	 *
	 * The following statement ensures that (va + i) is at least
	 * MAXBSIZE aligned.  Note that it also results in correct page
	 * alignment regardless of page size (exercise for the reader).
	 */

#ifndef	__sparcv9
	i -= (uintptr_t)va & MAXBOFFSET;
#endif

	rw_enter(&kas.a_lock, RW_WRITER);
	segkp = seg_alloc(&kas, va, i);
	if (segkp == NULL)
		cmn_err(CE_PANIC, "startup: cannot allocate segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);

	/*
	 * Now create generic mapping segment.  This mapping
	 * goes SEGMAPSIZE beyond SEGMAPBASE.  But if the total
	 * virtual address is greater than the amount of free
	 * memory that is available, then we trim back the
	 * segment size to that amount
	 */
	va = (caddr_t)SEGMAPBASE;

	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((uintptr_t)va & MAXBOFFSET) == 0);

	/*
	 * Set size of segmap to percentage of freemem at boot,
	 * but stay within the allowable range
	 */
	i = (mmu_ptob(freemem) * segmap_percent) / 100;

	if (i < MINMAPSIZE)
		i = MINMAPSIZE;

	if (i > MIN(SEGMAPSIZE, mmu_ptob(freemem)))
		i = MIN(SEGMAPSIZE, mmu_ptob(freemem));

	i &= MAXBMASK;	/* 1201049: segkmap size must be MAXBSIZE aligned */

	rw_enter(&kas.a_lock, RW_WRITER);
	segkmap = seg_alloc(&kas, va, i);
	if (segkmap == NULL)
		cmn_err(CE_PANIC, "cannot allocate segkmap");

	a.prot = PROT_READ | PROT_WRITE;
	a.shmsize = shm_alignment;
	a.nfreelist = 4;

	if (segmap_create(segkmap, (caddr_t)&a) != 0)
		panic("segmap_create segkmap");
	rw_exit(&kas.a_lock);

	/*
	 * Create a segment for kadb for checkpoint-resume.
	 */
	if (debug_start_va != 0) {
		rw_enter(&kas.a_lock, RW_WRITER);
		seg_debug = seg_alloc(&kas, (caddr_t)debug_start_va,
			DEBUGSIZE);
		if (seg_debug == NULL)
			cmn_err(CE_PANIC, "cannot allocate seg_debug");
		(void) segkmem_create(seg_debug);
		rw_exit(&kas.a_lock);
	}

	/*
	 * Allocate initial hmeblks for the sfmmu hat layer, could not do
	 * it in sfmmu_init(). XXX is there a better place for this ?
	 */
	sfmmu_hblk_init();
}

static void
startup_end(void)
{
	if ((caddr_t)memlist > (caddr_t)memlist_end)
		panic("memlist overflow 2");
	memlist_free_block((caddr_t)memlist,
	    ((caddr_t)memlist_end - (caddr_t)memlist));
	memlist = NULL;

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Initialize interrupt related stuff
	 */
	init_intr_threads(CPU);

	(void) splzs();			/* allow hi clock ints but not zs */

	/*
	 * Initialize errors.
	 */
	error_init();

	/*
	 * Startup memory scrubber, if not running fpu emulation code.
	 * Note that we may have already used kernel bcopy before this
	 * point - but if you really care about this, adb the use_hw_*
	 * variables to 0 before rebooting.
	 */
	if (fpu_exists) {
		if (memscrub_init()) {
			cmn_err(CE_WARN,
			    "Memory scrubber failed to initialize");
		}
	} else {
#ifdef DEBUG
		use_hw_bcopy = 0;
		use_hw_copyio = 0;
		use_hw_bzero = 0;
#endif
	}

	/*
	 * Install the "real" pre-emption guards before DDI services
	 * are available.
	 */
	mutex_init(&prom_mutex, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&prom_cv, NULL, CV_DEFAULT, NULL);
	(void) prom_set_preprom(kern_preprom);
	(void) prom_set_postprom(kern_postprom);
	CPU->cpu_m.mutex_ready = 1;

	/*
	 * Initialize segnf (kernel support for non-faulting loads).
	 */
	segnf_init();

	/*
	 * Configure the root devinfo node.
	 */
	configure();		/* set up devices */
}

static void
setup_trap_table(void)
{
	extern struct scb trap_table;

	intr_init(CPU);			/* init interrupt request free list */
	setwstate(WSTATE_KERN);
	prom_set_traptable((void *)&trap_table);
}

void
post_startup(void)
{
#ifdef	PTL1_PANIC_DEBUG
	extern void init_tl1_panic_thread(void);
#endif	/* PTL1_PANIC_DEBUG */

	/*
	 * Configure the rest of the system.
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);
	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	load_platform_drivers();

	/* load vis simulation module, if we are running w/fpu off */
	if (!fpu_exists) {
		if (modload("misc", "vis") == -1)
			halt("Can't load vis");
	}

	maxmem = freemem;

	(void) spl0();		/* allow interrupts */

#ifdef	PTL1_PANIC_DEBUG
	init_tl1_panic_thread();
#endif	/* PTL1_PANIC_DEBUG */
}

#ifdef	PTL1_PANIC_DEBUG
int		test_tl1_panic = 0;
kthread_id_t	tl1_panic_thread = NULL;
kcondvar_t	tl1_panic_cv;
kmutex_t	tl1_panic_mutex;

void
tl1_panic_recurse(int n)
{
	if (n != 0)
		tl1_panic_recurse(n - 1);
	else
		asm("ta	0x7C");
}

/* ARGSUSED */
void
tl1_panic_wakeup(void *arg)
{
	mutex_enter(&tl1_panic_mutex);
	cv_signal(&tl1_panic_cv);
	mutex_exit(&tl1_panic_mutex);
}

void
ptl1_panic_thread(void)
{
	int n = 8;

	mutex_enter(&tl1_panic_mutex);
	while (tl1_panic_thread) {
		if (test_tl1_panic) {
			test_tl1_panic = 0;
			tl1_panic_recurse(n);
		}
		(void) timeout(tl1_panic_wakeup, NULL, 60);
		(void) cv_wait(&tl1_panic_cv, &tl1_panic_mutex);
	}
	mutex_exit(&tl1_panic_mutex);
}

void
init_tl1_panic_thread(void)
{
	kthread_id_t tp;

	mutex_init(&tl1_panic_mutex, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&tl1_panic_cv, NULL, CV_DEFAULT, NULL);
	tp = thread_create(NULL, PAGESIZE, ptl1_panic_thread,
		NULL, 0, &p0, TS_RUN, 0);
	if (tp == NULL) {
		cmn_err(CE_WARN,
			"init_tl1_panic_thread: cannot start tl1 panic thread");
		cv_destroy(&tl1_panic_cv);
		return;
	}
	tl1_panic_thread = tp;
}
#endif	/* PTL1_PANIC_DEBUG */


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
	struct memlist *new;

	new = *memlistp;
	new->address = start;
	new->size = len;
	*memlistp = new + 1;

	memlist_insert(new, curmemlistp);
}

/*
 * In the case of architectures that support dynamic addition of
 * memory at run-time there are two cases where memsegs need to
 * be initialized and added to the memseg list.
 * 1) memsegs that are contructed at startup.
 * 2) memsegs that are constructed at run-time on
 *    hot-plug capable architectures.
 * This code was originally part of the function kphysm_init().
 */

static void
memseg_list_add(struct memseg *memsegp)
{
	struct memseg **prev_memsegp;
	pgcnt_t num;

	/* insert in memseg list, decreasing number of pages order */

	num = MSEG_NPAGES(memsegp);

	for (prev_memsegp = &memsegs; *prev_memsegp;
	    prev_memsegp = &((*prev_memsegp)->next)) {
		if (num > MSEG_NPAGES(*prev_memsegp))
			break;
	}

	memsegp->next = *prev_memsegp;
	*prev_memsegp = memsegp;
}


/*
 * kphysm_init() tackles the problem of initializing physical memory.
 * The old startup made some assumptions about the kernel living in
 * physically contiguous space which is no longer valid.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, pgcnt_t npages)
{
	struct memlist *pmem;
	struct memseg *cur_memseg;
	pfn_t pnum;
	extern void page_coloring_init(void);

	ASSERT(page_hash != NULL && page_hashsz != 0);

	page_coloring_init();

	cur_memseg = memsegp;
	for (pmem = phys_avail; pmem && npages;
	    pmem = pmem->next, cur_memseg++) {
		pfn_t base;
		pgcnt_t num;

		/*
		 * Build the memsegs entry
		 */
		num = btop(pmem->size);
		if (num > npages)
			num = npages;
		npages -= num;
		base = btop(pmem->address);

		cur_memseg->pages = pp;
		cur_memseg->epages = pp + num;
		cur_memseg->pages_base = base;
		cur_memseg->pages_end = base + num;

		memseg_list_add(cur_memseg);

		/*
		 * Initialize the "platform-dependent" part of the page struct
		 */
		pnum = cur_memseg->pages_base;
		for (pp = cur_memseg->pages; pp < cur_memseg->epages; pp++) {
			pp->p_pagenum = pnum;
			pnum++;
		}

		/*
		 * Tell the rest of the kernel about this chunk of memory.
		 */
		add_physmem((page_t *)cur_memseg->pages, num);
	}

	build_pfn_hash();
}

/*
 * Kernel VM initialization.
 * Assumptions about kernel address space ordering:
 *	(1) gap (user space)
 *	(2) kernel text
 *	(3) kernel data/bss
 *	(4) gap
 *	(5) kernel data structures
 *	(6) gap
 *	(7) debugger (optional)
 *	(8) monitor
 *	(9) gap (possibly null)
 *	(10) dvma
 *	(11) devices
 */
static void
kvm_init(void)
{
	pfn_t pfnum;
	struct memlist *cur;
	size_t syslimit = (size_t)SYSLIMIT;
	caddr_t sysbase = (caddr_t)SYSBASE;

#ifndef KVM_DEBUG
#define	KVM_DEBUG 0	/* 0 = no debugging, 1 = debugging */
#endif

#if KVM_DEBUG > 0
#define	KVM_HERE \
	printf("kvm_init: checkpoint %d line %d\n", ++kvm_here, __LINE__);
#define	KVM_DONE	{ printf("kvm_init: all done\n"); kvm_here = 0; }
	int kvm_here = 0;
#else
#define	KVM_HERE
#define	KVM_DONE
#endif

KVM_HERE
	/*
	 * Put the kernel segments in kernel address space.  Make it a
	 * "kernel memory" segment objects.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

	(void) seg_attach(&kas, (caddr_t)KERNELBASE,
	    (size_t)(e_moddata - KERNELBASE), &ktextseg);
	(void) segkmem_create(&ktextseg);

KVM_HERE
	(void) seg_attach(&kas, (caddr_t)valloc_base,
	    (size_t)(econtig - valloc_base), &kvalloc);
	(void) segkmem_create(&kvalloc);

KVM_HERE
	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, (caddr_t)SYSBASE, SYSLIMIT - SYSBASE, &kvseg);
	(void) segkmem_create(&kvseg);

#ifdef __sparcv9
	(void) seg_attach(&kas, (caddr_t)SYSBASE32, SYSLIMIT32 - SYSBASE32,
	    &kvseg32);
	(void) segkmem_create(&kvseg32);
#endif

	rw_exit(&kas.a_lock);

KVM_HERE
	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * Validate to SYSLIMIT.  There may be several fragments of
	 * 'used' virtual memory in this range, so we hunt 'em all down.
	 */
	for (cur = virt_avail; cur->next; cur = cur->next) {
		uint64_t range_base, range_size;

		if ((range_base = cur->address + cur->size) < (uint64_t)sysbase)
			continue;
		if (range_base >= (uint64_t)syslimit)
			break;
		/*
		 * Limit the range to end at SYSLIMIT.
		 */
		range_size = MIN(cur->next->address, (uint64_t)syslimit) -
		    range_base;
		(void) as_setprot(&kas, (caddr_t)range_base, (size_t)range_size,
		    PROT_READ | PROT_WRITE | PROT_EXEC);
	}

	/*
	 * Invalidate unused portion of heap_arena.
	 * (We know that the PROM never allocates any mappings here by
	 * itself without updating the 'virt-avail' list, so that we can
	 * simply render anything that is on the 'virt-avail' list invalid)
	 * (Making sure to ignore virtual addresses above 2**32.)
	 */
	for (cur = virt_avail; cur && cur->address < (uint64_t)syslimit;
	    cur = cur->next) {
		uint64_t range_base, range_end;

		range_base = MAX(cur->address, (uint64_t)sysbase);
		range_end  = MIN(cur->address + cur->size,
		    (uint64_t)syslimit);
		if (range_end > range_base)
			(void) as_setprot(&kas, (caddr_t)range_base,
			    (size_t)(range_end - range_base), 0);
	}
	rw_exit(&kas.a_lock);

	/*
	 * Find the begining page frames of the kernel data
	 * segment and the ending page frame (-1) for bss.
	 */
	/*
	 * FIXME - nobody seems to use them but we could later on.
	 */
	pfnum = va_to_pfn((caddr_t)roundup((size_t)e_text, DATA_ALIGN));
	if (pfnum != PFN_INVALID)
		kpfn_dataseg = pfnum;
	if ((pfnum = va_to_pfn(e_data)) != PFN_INVALID)
		kpfn_endbss = pfnum;

KVM_DONE
}

/*
 * Use boot to allocate the physical memory needed for the IOMMU's TSB arrays.
 * Memory allocated for IOMMU TSBs is accessed via virtual addresses.
 * We can relinquish the virtual address mappings to this space if IOMMU
 * code uses physical addresses with MMU bypass mode, but virtual addresses
 * are easier to deal with.
 *
 * WARNING - since this routine uses boot to allocate memory, it MUST
 * be called before the kernel takes over memory allocation from boot.
 */
caddr_t iommu_tsb_vaddr[MAX_UPA];
int iommu_tsb_alloc_size[MAX_UPA];
dnode_t iommu_nodes[MAX_UPA];
uchar_t iommu_tsb_spare[MAX_UPA];
int iommu_tsb_spare_count = 0;
/* Let user disable Dynamic Reconfiguration */
int enable_dynamic_reconfiguration = 1;

caddr_t
iommu_tsb_alloc(caddr_t alloc_base)
{
	char name[128];
	char compatible[128];
	caddr_t vaddr;
	int i, total_size, size;
	caddr_t iommu_alloc_base = (caddr_t)roundup((uintptr_t)alloc_base,
	    MMU_PAGESIZE);

	/*
	 * determine the amount of physical memory required for the TSB arrays
	 *
	 * assumes iommu_tsb_alloc_size[] has already been initialized, i.e.
	 * map_wellknown_devices()
	 */
	for (i = total_size = 0; i < MAX_UPA; i++) {
		/*
		 * If the system has 32 mb or less of physical memory,
		 * allocate enough space for 64 mb of dvma space.
		 * Otherwise allocate enough space for 256 mb of dvma
		 * space.
		 */
		if (iommu_nodes[i] == NULL) {
			if (iommu_tsb_spare_count < set_platform_tsb_spares()) {
				/* is a spare and available */
				size =
					(pci_iommu_tsb_alloc_size >
					sbus_iommu_tsb_alloc_size) ?
						pci_iommu_tsb_alloc_size:
						sbus_iommu_tsb_alloc_size;
				iommu_tsb_spare[i] = IOMMU_TSB_ISASPARE;
				if (size == 0)
					size = (physinstalled <= 0x1000 ?
						0x10000 : 0x40000);
				size = size * 2;
				iommu_tsb_spare_count++;
			} else {
				iommu_tsb_spare[i] = 0;
				continue;
			}
		} else {
			(void) prom_getprop(iommu_nodes[i], "name", name);
			if (strcmp(name, "sbus") == 0) {
				iommu_tsb_spare[i] = 0;
				if (sbus_iommu_tsb_alloc_size != 0)
					size = sbus_iommu_tsb_alloc_size;
				else
					size = (physinstalled <= 0x1000 ?
						0x10000 : 0x40000);
			} else if (strcmp(name, "pci") == 0) {
				iommu_tsb_spare[i] = 0;
				if (pci_iommu_tsb_alloc_size != 0)
					size = pci_iommu_tsb_alloc_size;
				else
					size = (physinstalled <= 0x1000 ?
						0x10000 : 0x40000);

				/*
				 * PsychoNG (schizo) has two iommu's so we must
				 * allocate twice the amount of space.
				 */
				(void) prom_getprop(iommu_nodes[i],
					"compatible",
					compatible);
				if (strcmp(compatible, "pci108e,8001") == 0)
					size = size * 2;
			} else {
				iommu_tsb_spare[i] = 0;
				continue;	/* unknown i/o bus bridge */
			}
		}
		total_size += size;
		iommu_tsb_alloc_size[i] = size;
	}

	if (total_size == 0)
		return (alloc_base);

	/*
	 * allocate the physical memory for the TSB arrays
	 */
	if ((vaddr = (caddr_t)BOP_ALLOC(bootops, iommu_alloc_base,
	    total_size, MMU_PAGESIZE)) == NULL)
		cmn_err(CE_PANIC, "Cannot allocate IOMMU TSB arrays");

	/*
	 * assign the virtual addresses for each TSB
	 */
	for (i = 0; i < MAX_UPA; i++) {
		if ((size = iommu_tsb_alloc_size[i]) != 0) {
			iommu_tsb_vaddr[i] = vaddr;
			vaddr += size;
		}
	}

	return (iommu_alloc_base + total_size);
}

char obp_tte_str[] =
	"h# %x constant MMU_PAGESHIFT "
	"h# %x constant TTE8K "
	"h# %x constant SFHME_SIZE "
	"h# %x constant SFHME_TTE "
	"h# %x constant HMEBLK_TAG "
	"h# %x constant HMEBLK_NEXT "
	"h# %x constant HMEBLK_MISC "
	"h# %x constant HMEBLK_HME1 "
	"h# %x constant NHMENTS "
	"h# %x constant HBLK_SZMASK "
	"h# %x constant HBLK_RANGE_SHIFT "
	"h# %x constant HMEBP_HBLK "
	"h# %x constant HMEBUCKET_SIZE "
	"h# %x constant HTAG_SFMMUPSZ "
	"h# %x constant HTAG_REHASHSZ "
	"h# %x constant MAX_HASHCNT "
	"h# %p constant uhme_hash "
	"h# %p constant khme_hash "
	"h# %x constant UHMEHASH_SZ "
	"h# %x constant KHMEHASH_SZ "
	"h# %p constant KHATID "
	"h# %x constant CTX_SIZE "
	"h# %x constant CTX_SFMMU "
	"h# %p constant ctxs "
	"h# %x constant ASI_MEM "

	": PHYS-X@ ( phys -- data ) "
	"   ASI_MEM spacex@ "
	"; "

	": PHYS-W@ ( phys -- data ) "
	"   ASI_MEM spacew@ "
	"; "

	": PHYS-L@ ( phys -- data ) "
	"   ASI_MEM spaceL@ "
	"; "

	": TTE_PAGE_SHIFT ( ttesz -- hmeshift ) "
	"   3 * MMU_PAGESHIFT + "
	"; "

	": TTE_IS_VALID ( ttep -- flag ) "
	"   PHYS-X@ 0< "
	"; "

	": HME_HASH_SHIFT ( ttesz -- hmeshift ) "
	"   dup TTE8K =  if "
	"      drop HBLK_RANGE_SHIFT "
	"   else "
	"      TTE_PAGE_SHIFT "
	"   then "
	"; "

	": HME_HASH_BSPAGE ( addr hmeshift -- bspage ) "
	"   tuck >> swap MMU_PAGESHIFT - << "
	"; "

	": HME_HASH_FUNCTION ( sfmmup addr hmeshift -- hmebp ) "
	"   >> over xor swap                    ( hash sfmmup ) "
	"   KHATID <>  if                       ( hash ) "
	"      UHMEHASH_SZ and                  ( bucket ) "
	"      HMEBUCKET_SIZE * uhme_hash +     ( hmebp ) "
	"   else                                ( hash ) "
	"      KHMEHASH_SZ and                  ( bucket ) "
	"      HMEBUCKET_SIZE * khme_hash +     ( hmebp ) "
	"   then                                ( hmebp ) "
	"; "

#ifdef	__sparcv9

	": HME_HASH_TABLE_SEARCH "
	"       ( sfmmup hmebp hblktag --  sfmmup null | sfmmup hmeblkp ) "
	"   >r hmebp_hblk + x@ begin 	( sfmmup hmeblkp ) ( r: hblktag ) "
	"      dup if   		( sfmmup hmeblkp ) ( r: hblktag ) "
	"         dup hmeblk_tag + phys-x@ r@ = if ( sfmmup hmeblkp )	  "
	"	     dup hmeblk_tag + 8 + phys-x@ 2 pick = if		  "
	"		  true 	( sfmmup hmeblkp true ) ( r: hblktag )	  "
	"	     else						  "
	"	     	  hmeblk_next + phys-x@ false 			  "
	"			( sfmmup hmeblkp false ) ( r: hblktag )   "
	"	     then  						  "
	"	  else							  "
	"	     hmeblk_next + phys-x@ false 			  "
	"			( sfmmup hmeblkp false ) ( r: hblktag )   "
	"	  then 							  "
	"      else							  "
	"         true 							  "
	"      then  							  "
	"   until r> drop 						  "
	"; "

#else /* ! __sparcv9 */

	": HME_HASH_TABLE_SEARCH "
	"        ( sfmmup hmebp hblktag --  sfmmup null | sfmmup hmeblkp ) "
	"   >r HMEBP_HBLK + x@			( hmeblkp ) ( r: hblktag ) "
	"   begin                               ( hmeblkp ) ( r: hblktag ) "
	"      dup  if                          ( hmeblkp ) ( r: hblktag ) "
	"         dup HMEBLK_TAG + PHYS-X@ r@ =  if ( hmeblkp ) ( r: hblktag ) "
	"            true                       ( hmeblkp true ) 	   "
						"( r: hblktag ) 	   "
	"         else                          ( hmeblkp ) ( r: hblktag ) "
	"            HMEBLK_NEXT + PHYS-X@ false     ( hmeblkp' false )    "
						"( r: hblktag ) 	   "
	"         then                          ( hmeblkp flag ) 	   "
						"( r: hblktag ) 	   "
	"      else                             ( null ) ( r: hblktag )    "
	"         true                          ( null true ) ( r: hblktag ) "
	"      then                             ( hmeblkp flag ) 	   "
						"( r: hblktag ) 	   "
	"   until                               ( null | hmeblkp ) 	   "
						"( r: hblktag ) 	   "
	"   r> drop                             ( null | hmeblkp ) 	   "
	"; "

#endif /* __sparcv9 */

	": CNUM_TO_SFMMUP ( cnum -- sfmmup ) "
	"   CTX_SIZE * ctxs + CTX_SFMMU + "
#ifdef __sparcv9
	"x@ "
#else
	"l@ "
#endif
	"; "

#ifdef	__sparcv9

	": HME_HASH_TAG ( sfmmup rehash addr -- hblktag ) "
	"   over HME_HASH_SHIFT HME_HASH_BSPAGE      ( sfmmup rehash bspage ) "
	"   HTAG_REHASHSZ << or nip		     ( hblktag ) "
	"; "

#else	/* ! __sparcv9 */

	": HME_HASH_TAG ( sfmmup rehash addr -- hblktag ) "
	"   over HME_HASH_SHIFT HME_HASH_BSPAGE      ( sfmmup rehash bspage ) "
	"   HTAG_REHASHSZ << or HTAG_SFMMUPSZ << or  ( hblktag ) "
	"; "

#endif	/* __sparcv9 */

	": HBLK_TO_TTEP ( hmeblkp addr -- ttep ) "
	"   over HMEBLK_MISC + PHYS-L@ HBLK_SZMASK and  ( hmeblkp addr ttesz ) "
	"   TTE8K =  if                            ( hmeblkp addr ) "
	"      MMU_PAGESHIFT >> NHMENTS 1- and     ( hmeblkp hme-index ) "
	"   else                                   ( hmeblkp addr ) "
	"      drop 0                              ( hmeblkp 0 ) "
	"   then                                   ( hmeblkp hme-index ) "
	"   SFHME_SIZE * + HMEBLK_HME1 +           ( hmep ) "
	"   SFHME_TTE +                            ( ttep ) "
	"; "

	": unix-tte ( addr cnum -- false | tte-data true ) "
#ifndef	__sparcv9
	/*
	 * Don't clear upper 32 bits for LP64 kernel.
	 */
	"   over h# 20 >> 0<>  if             ( addr cnum ) "
	"      2drop false                    ( false ) "
	"   else                              ( addr cnum ) "
#endif
	"      CNUM_TO_SFMMUP                 ( addr sfmmup ) "
	"      MAX_HASHCNT 1+ 1  do           ( addr sfmmup ) "
	"         2dup swap i HME_HASH_SHIFT  "
					"( addr sfmmup sfmmup addr hmeshift ) "
	"         HME_HASH_FUNCTION           ( addr sfmmup hmebp ) "
	"         over i 4 pick               "
				"( addr sfmmup hmebp sfmmup rehash addr ) "
	"         HME_HASH_TAG                ( addr sfmmup hmebp hblktag ) "
	"         HME_HASH_TABLE_SEARCH       "
					"( addr sfmmup { null | hmeblkp } ) "
	"         ?dup  if                    ( addr sfmmup hmeblkp ) "
	"            nip swap HBLK_TO_TTEP    ( ttep ) "
	"            dup TTE_IS_VALID  if     ( valid-ttep ) "
	"               PHYS-X@ true          ( tte-data true ) "
	"            else                     ( invalid-tte ) "
	"               drop false            ( false ) "
	"            then                     ( false | tte-data true ) "
	"            unloop exit              ( false | tte-data true ) "
	"         then                        ( addr sfmmup ) "
	"      loop                           ( addr sfmmup ) "
	"      2drop false                    ( false ) "
#ifndef	__sparcv9
	"   then                              ( false ) "
#endif	/* !__sparcv9 */
	"; "
;

void
create_va_to_tte(void)
{
	char *bp;
	extern int khmehash_num, uhmehash_num;
	extern struct hmehash_bucket *khme_hash, *uhme_hash;

#define	OFFSET(type, field)	((uintptr_t)(&((type *)0)->field))

	bp = (char *)kobj_zalloc(MMU_PAGESIZE, KM_SLEEP);

	/*
	 * Teach obp how to parse our sw ttes.
	 */
	(void) sprintf(bp, obp_tte_str,
		MMU_PAGESHIFT,
		TTE8K,
		sizeof (struct sf_hment),
		OFFSET(struct sf_hment, hme_tte),
		OFFSET(struct hme_blk, hblk_tag),
		OFFSET(struct hme_blk, hblk_nextpa),
		OFFSET(struct hme_blk, hblk_misc),
		OFFSET(struct hme_blk, hblk_hme),
		NHMENTS,
		HBLK_SZMASK,
		HBLK_RANGE_SHIFT,
		OFFSET(struct hmehash_bucket, hmeh_nextpa),
		sizeof (struct hmehash_bucket),
		HTAG_SFMMUPSZ,
		HTAG_REHASHSZ,
		MAX_HASHCNT,
		uhme_hash,
		khme_hash,
		UHMEHASH_SZ,
		KHMEHASH_SZ,
		KHATID,
		sizeof (struct ctx),
		OFFSET(struct ctx, c_sfmmu),
		ctxs,
		ASI_MEM);
	prom_interpret(bp, 0, 0, 0, 0, 0);

	kobj_free(bp, MMU_PAGESIZE);
}

void
install_va_to_tte(void)
{
	/*
	 * advise prom that he can use unix-tte
	 */
	prom_interpret("' unix-tte is va>tte-data", 0, 0, 0, 0, 0);
}


void
forthdebug_init(void)
{
	char *bp = NULL;
	struct _buf *file = NULL;
	int read_size, ch;
	int buf_size = 0;

	file = kobj_open_path(FDEBUGFILE, 1, 1);
	if (file == (struct _buf *)-1) {
		cmn_err(CE_CONT, "Can't open %s\n", FDEBUGFILE);
		goto bad;
	}

	/*
	 * the first line should be \ <size>
	 * XXX it would have been nice if we could use lex() here
	 * instead of doing the parsing here
	 */
	while (((ch = kobj_getc(file)) != -1) && (ch != '\n')) {
		if ((ch) >= '0' && (ch) <= '9') {
			buf_size = buf_size * 10 + ch - '0';
		} else if (buf_size) {
			break;
		}
	}

	if (buf_size == 0) {
		cmn_err(CE_CONT, "can't determine size of %s\n", FDEBUGFILE);
		goto bad;
	}

	/*
	 * skip to next line
	 */
	while ((ch != '\n') && (ch != -1)) {
		ch = kobj_getc(file);
	}

	/*
	 * Download the debug file.
	 */
	bp = (char *)kobj_zalloc(buf_size, KM_SLEEP);
	read_size = kobj_read_file(file, bp, buf_size, 0);
	if (read_size < 0) {
		cmn_err(CE_CONT, "Failed to read in %s\n", FDEBUGFILE);
		goto bad;
	}
	if (read_size == buf_size && kobj_getc(file) != -1) {
		cmn_err(CE_CONT, "%s is larger than %d\n",
			FDEBUGFILE, buf_size);
		goto bad;
	}
	bp[read_size] = 0;
	cmn_err(CE_CONT, "Read %d bytes from %s\n", read_size, FDEBUGFILE);
	prom_interpret(bp, 0, 0, 0, 0, 0);

bad:
	if (file != (struct _buf *)-1) {
		kobj_close_file(file);
	}

	/*
	 * Make sure the bp is valid before calling kobj_free.
	 */
	if (bp != NULL) {
		kobj_free(bp, buf_size);
	}
}

struct mem_node_conf	mem_node_config[MAX_MEM_NODES];
int			mem_node_pfn_shift;

/*
 * This routine will differ on NUMA platforms.
 */

static void
startup_build_mem_nodes(void)
{
	/* LINTED */
	ASSERT(MAX_MEM_NODES == 1);

	mem_node_pfn_shift = 0;

	mem_node_config[0].exists = 1;
	mem_node_config[0].physmax = physmax;
}

/*
 * This routine will evolve on NUMA platforms.
 */
void
mem_node_config_adjust(int mnode, int pnum)
{
	mem_node_config[mnode].physmax = pnum;
}

/*
 * Return true if the machine we're running on is a Positron.
 * (Positron is an unsupported developers platform.)
 */
static int
iam_positron(void)
{
	char model[32];
	const char proto_model[] = "SUNW,501-2732";
	dnode_t root = prom_rootnode();

	if (prom_getproplen(root, "model") != sizeof (proto_model))
		return (0);

	(void) prom_getprop(root, "model", model);
	if (strcmp(model, proto_model) == 0)
		return (1);
	return (0);
}

static char *create_node =
	"root-device "
	"new-device "
	"\" os-io\" device-name "
	": cb-r/w  ( adr,len method$ -- #read/#written ) "
	"   2>r swap 2 2r> ['] $callback  catch  if "
	"      2drop 3drop 0 "
	"   then "
	"; "
	": read ( adr,len -- #read ) "
	"       \" read\" ['] cb-r/w catch  if  2drop 2drop -2 exit then "
	"       ( retN ... ret1 N ) "
	"       ?dup  if "
	"               swap >r 1-  0  ?do  drop  loop  r> "
	"       else "
	"               -2 "
	"       then l->n "
	";    "
	": write ( adr,len -- #written ) "
	"       \" write\" ['] cb-r/w catch  if  2drop 2drop 0 exit  then "
	"       ( retN ... ret1 N ) "
	"       ?dup  if "
	"               swap >r 1-  0  ?do  drop  loop  r> "
	"        else "
	"               0 "
	"       then "
	"; "
	": poll-tty ( -- ) ; "
	": install-abort  ( -- )  ['] poll-tty d# 10 alarm ; "
	": remove-abort ( -- )  ['] poll-tty 0 alarm ; "
	": cb-give/take ( $method -- ) "
	"       0 -rot ['] $callback catch  ?dup  if "
	"               >r 2drop 2drop r> throw "
	"       else "
	"               0  ?do  drop  loop "
	"       then "
	"; "
	": give ( -- )  \" exit-input\" cb-give/take ; "
	": take ( -- )  \" enter-input\" cb-give/take ; "
	": open ( -- ok? )  true ; "
	": close ( -- ) ; "
	"finish-device "
	"device-end ";

/*
 * Create the obp input/output node only if the USB keyboard is the
 * standard input device.  When the USB software takes over the
 * input device at the time consconfig runs, it will switch OBP's
 * notion of the input device to this node.  Whenever the
 * forth user interface is used after this switch, the node will
 * call back into the kernel for console input.
 *
 * This callback mechanism is currently only used when the USB keyboard
 * is the input device.  If a serial device such as ttya or
 * a UART with a Type 5 keyboard attached is used, obp takes over the
 * serial device when the system goes to the debugger after the system is
 * booted.  This sharing of the relatively simple serial device is difficult
 * but possible.  Sharing the USB host controller is impossible due
 * its complexity
 */
static void
startup_create_input_node(void)
{
	char *stdin_path;

	/*
	 * If usb_node_debug is set in /etc/system
	 * then the user would like to test the callbacks
	 * from the input node regardless of whether or
	 * not the USB keyboard is the console input.
	 * This variable is useful for debugging.
	 */
	if (usb_node_debug) {

		prom_interpret(create_node, 0, 0, 0, 0, 0);

		return;
	}

	/* Obtain the console input device */
	stdin_path = prom_stdinpath();

	/*
	 * If the string "usb" and "keyboard" are in the path
	 * then a USB keyboard is the console input device,
	 * create the node.
	 */
	if ((strstr(stdin_path, "usb") != 0) &&
		(strstr(stdin_path, "keyboard") != 0)) {

		prom_interpret(create_node, 0, 0, 0, 0, 0);
	}
}


#ifdef __sparcv9
static void
do_prom_version_check(void)
{
	int i;
	dnode_t node;
	char buf[64];
	static char drev[] = "Down-rev firmware detected%s\n"
		"\tPlease upgrade to the following minimum version:\n"
		"\t\t%s\n";

	i = prom_version_check(buf, sizeof (buf), &node);

	if (i == PROM_VER64_OK)
		return;

	if (i == PROM_VER64_UPGRADE) {
		cmn_err(CE_WARN, drev, "", buf);

#ifdef	DEBUG
		prom_enter_mon();	/* Type 'go' to continue */
		cmn_err(CE_WARN, "Booting with down-rev firmware\n");
		return;
#else
		halt(0);
#endif
	}

	/*
	 * The other possibility is that this is a server running
	 * good firmware, but down-rev firmware was detected on at
	 * least one other cpu board. We just complain if we see
	 * that.
	 */
	cmn_err(CE_WARN, drev, " on one or more CPU boards", buf);
}
#endif /* __sparcv9 */

void
kobj_vmem_init(vmem_t **text_arena, vmem_t **data_arena)
{
	*text_arena = vmem_create("module_text", modtext, MODTEXT, 1,
	    segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);
	*data_arena = vmem_create("module_data", moddata, MODDATA, 1,
	    segkmem_alloc, segkmem_free, heap32_arena, 0, VM_SLEEP);
}
