/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fillsysinfo.c	1.67	99/06/05 SMI"

#include <sys/errno.h>
#include <sys/module.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>

#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/cmn_err.h>
#include <sys/bt.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/memctl.h>
#include <sys/aflt.h>
#include <sys/clock.h>

/*
 * The OpenBoot Standalone Interface supplies the kernel with
 * implementation dependent parameters through the devinfo/property mechanism
 */

#define	MAXSYSNAME 20
#define	NMODULES 4

static void check_cpus(void);
static void fill_cpu(dnode_t);


typedef enum { XDRBOOL, XDRINT, XDRSTRING } xdrs;
/*
 * structure describing properties that we are interested in querying the
 * OBP for.
 */
struct getprop_info {
	char   *name;
	xdrs	type;
	u_int  *var;
};

/*
 * structure used to convert between a string returned by the OBP & a type
 * used within the kernel. We prefer to paramaterize rather than type.
 */
struct convert_info {
	char *name;
	u_int var;
	char *realname;
};

/*
 * structure describing nodes that we are interested in querying the OBP for
 * properties.
 */
struct node_info {
	char			*name;
	int			size;
	struct getprop_info	*prop;
	struct getprop_info	*prop_end;
	unsigned int		*value;
};

/*
 * macro definitions for routines that form the OBP interface
 */
#define	NEXT			prom_nextnode
#define	CHILD			prom_childnode
#define	GETPROP			prom_getprop
#define	GETPROPLEN		prom_getproplen

/* 0=quiet; 1=verbose; 2=debug */
int	debug_fillsysinfo = 0;
#define	VPRINTF if (debug_fillsysinfo) prom_printf

#define	CLROUT(a, l)			\
{					\
	register int c = l;		\
	register char *p = (char *)a;	\
	while (c-- > 0)			\
		*p++ = 0;		\
}

#define	CLRBUF(a)	CLROUT(a, sizeof (a))


static int ncpunode;		/* cpu_nodes in the devtree */
struct cpu_node cpunodes[NCPU];
struct cpu_node *cpuid_to_cpunode[NCPU];

extern struct cpu cpus[];

/*
 * list of well known devices that must be mapped, and the variables that
 * contain their addresses.
 */
caddr_t v_auxio_addr;
caddr_t v_iommu_addr;
caddr_t v_memerr_addr;
volatile struct count14 *v_counter_addr[NCPU + 1];
					/* 4 @ level14, 1 @ level 10 */
struct cpu_intreg *v_interrupt_addr[NCPU + 1];
					/* 4 (1 per module) + 1 (system) */
caddr_t v_eeprom_addr;
caddr_t v_sbusctl_addr;

/*
 * Multiple addresses broken out here to avoid run-time arithmetic
 */
struct sys_intreg *v_sipr_addr;
struct count10 *v_level10clk_addr;

/*
 * Some globals that are set depending on whether a particular device
 * exists in the system.
 */
enum	mc_type mc_type, memctl_type(void);
int	diagleds;
int	nvsimm_present;
struct	memslot memslots[PMEM_NUMSLOTS];

/*
 * Sigh, the name of an nvsimm module for this architecture (should have a
 * node-type property instead).  This is also needed in machdep.c.
 */
char	nvsimm_name[] = {"SUNW,nvone"};

/*
 * Some nodes have functions that need to be called when they're seen.
 */
void	have_eccmemctl();
void	have_counter();
void	have_interrupt();
void	have_auxio();
void	have_leds();
void	have_memory();
void	have_nvsimms();

static struct wkdevice {
	char *wk_namep;
	void (*wk_func)();
	caddr_t *wk_vaddrp;
	u_short wk_flags;
#define	V_MUSTHAVE	0x0001
#define	V_OPTIONAL	0x0000
#define	V_MAPPED	0x0002
#define	V_MULTI		0x0003	/* optional, may be more than one */
} wkdevice[] = {
	{ "iommu", NULL, &v_iommu_addr, V_MUSTHAVE },
	{ "eccmemctl", have_eccmemctl, &v_memerr_addr, V_OPTIONAL },
	{ "parmemctl", NULL, &v_memerr_addr, V_OPTIONAL },
	{ "counter", have_counter, NULL, V_MUSTHAVE },
	{ "interrupt", have_interrupt, NULL, V_MUSTHAVE },
	{ "eeprom", NULL, &v_eeprom_addr, V_MUSTHAVE },
	{ "sbus", NULL, &v_sbusctl_addr, V_MUSTHAVE },
	{ "auxio", have_auxio, &v_auxio_addr, V_OPTIONAL },
	{ "leds", have_leds, NULL, V_OPTIONAL },
	{ "memory", have_memory, NULL, V_OPTIONAL },
	{ nvsimm_name, have_nvsimms, NULL, V_MULTI },
	{ 0, },
};

static void map_wellknown(dnode_t);

void
map_wellknown_devices()
{
	struct wkdevice *wkp;
	int fail = 0;

	map_wellknown(NEXT((dnode_t)0));

	/*
	 * See if it worked
	 */
	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (wkp->wk_flags == V_MUSTHAVE) {
			cmn_err(CE_CONT, "required device %s not mapped\n",
			    wkp->wk_namep);
			fail++;
		}
	}

	if (fail)
		cmn_err(CE_PANIC, "map_wellknown_devices");
	check_cpus();
}

/*
 * map_wellknown - map known devices & registers
 */
static void
map_wellknown(dnode_t curnode)
{
	char tmp_name[MAXSYSNAME];
	static void fill_address(dnode_t, char *);

#ifdef	VPRINTF
	VPRINTF("map_wellknown(%x)\n", curnode);
#endif	VPRINTF

	for (curnode = CHILD(curnode); curnode; curnode = NEXT(curnode)) {
		CLRBUF(tmp_name);
		if (GETPROP(curnode, OBP_NAME, (caddr_t)tmp_name) != -1)
			fill_address(curnode, tmp_name);
		if (GETPROP(curnode, OBP_DEVICETYPE, tmp_name) != -1 &&
		    strcmp(tmp_name, "cpu") == 0)
			fill_cpu(curnode);
		map_wellknown(curnode);
	}
}

static void
fill_address(dnode_t curnode, char *namep)
{
	struct wkdevice *wkp;
	int size;

	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (strcmp(wkp->wk_namep, namep) != 0)
			continue;
		if (wkp->wk_flags == V_MAPPED)
			return;
		if (wkp->wk_vaddrp != NULL) {
			if ((size = GETPROPLEN(curnode, OBP_ADDRESS)) == -1) {
				prom_printf("device %s size %d\n", namep, size);
				continue;
			}
			if (size > sizeof (caddr_t)) {
				prom_printf("device %s address prop too big\n",
					    namep);
				continue;
			}
			if (GETPROP(curnode, OBP_ADDRESS,
				    (caddr_t)wkp->wk_vaddrp) == -1) {
				prom_printf("device %s not mapped\n", namep);
				continue;
			}
#ifdef	VPRINTF
			VPRINTF("fill_address: %s mapped to %x\n", namep,
				*wkp->wk_vaddrp);
#endif	/* VPRINTF */
		}
		if (wkp->wk_func != NULL)
			(*wkp->wk_func)(curnode);
		/*
		 * If this one is optional and there may be more than
		 * one, don't set V_MAPPED, which would cause us to skip it
		 * next time around
		 */
		if (wkp->wk_flags != V_MULTI)
			wkp->wk_flags = V_MAPPED;
	}
}

static void
fill_cpu(dnode_t node)
{
	struct cpu_node *cpunode;

	extern int mxcc_linesize;
	extern int mxcc_cachesize;
	extern int mxcc_tagblockmask;
	int mxcc_numlines;

	if (ncpunode == NCPU)
		return;
	cpunode = &cpunodes[ncpunode];
	(void) GETPROP(node, "name", cpunode->name);
	(void) GETPROP(node, "implementation",
	    (caddr_t)&cpunode->implementation);
	(void) GETPROP(node, "version", (caddr_t)&cpunode->version);
	(void) GETPROP(node, "mid", (caddr_t)&cpunode->mid);
	(void) GETPROP(node, "clock-frequency", (caddr_t)&cpunode->clock_freq);

	/*
	 * If we didn't find it in the CPU node, look in the root node.
	 */
	if (cpunode->clock_freq == 0) {
		dnode_t root = prom_nextnode((dnode_t)0);
		(void) GETPROP(root, "clock-frequency",
		    (caddr_t)&cpunode->clock_freq);
	}

	/* for viking, check if we have an mxcc */
	if (cpunode->implementation == 0) {
		cpunode->u_info.viking_mxcc =
		    (GETPROPLEN(node, "ecache-parity?") != -1);
		if (cpunode->u_info.viking_mxcc) {
			(void) GETPROP(node, "ecache-line-size",
				(caddr_t)&mxcc_linesize);
			(void) GETPROP(node, "ecache-nlines",
				(caddr_t)&mxcc_numlines);
			mxcc_cachesize = mxcc_linesize * mxcc_numlines;
			mxcc_tagblockmask = (mxcc_cachesize - 1) &
						~(mxcc_linesize - 1);
		}
	}
	cpunode->nodeid = (int)node;
	cpuid_to_cpunode[MID2CPU(cpunode->mid) & 7] = cpunode;
	ncpunode++;
}

static void
check_cpus()
{
	int i, uponly = 0;
	int impl, ecache;
	char *msg = NULL;
	extern use_mp;

	/*
	 * We check here for illegal cpu combinations.
	 * Currently, we check that the implementations are the same,
	 * that either all or no vikings have mxccs, and that vo-mp
	 * systems have revision 3.0 (version 1) or greater.
	 */
	impl = cpunodes[0].implementation;
	if (impl == 0) {
		ecache = cpunodes[0].u_info.viking_mxcc;
		if (ecache == 0 && cpunodes[0].version < 1)
			uponly = 1;
	}
	for (i = 1; i < NCPU; i++) {
		if (cpunodes[i].nodeid == 0)
			break;
		if (cpunodes[i].implementation != impl) {
			msg = "mismatched modules";
			break;
		}
		if (impl == 0) {
			if (uponly ||
			    (ecache == 0 && cpunodes[i].version < 1)) {
				msg = "downrev SuperSPARC without SuperCache";
				break;
			}
			if (cpunodes[i].u_info.viking_mxcc != ecache) {
				msg = "mismatched SuperSPARC modules";
				break;
			}
		}
	}
	if (msg != NULL) {
		prom_printf("MP not supported on %s\n", msg);
		prom_printf("booting UP only\n");
		for (i = 0; i < NCPU; i++) {
			if (cpunodes[i].nodeid == 0)
				break;
			prom_printf("cpu%d: %s version 0x%x\n",
				    cpunodes[i].mid - 8,
				    cpunodes[i].name, cpunodes[i].version);
		}
		use_mp = 0;
	}
	/* Set max cpus based on ncpunode and use_mp */
	if (use_mp)
		max_ncpus = ncpunode;
	else
		max_ncpus = 1;
}


/*ARGSUSED*/
void
have_eccmemctl(dnode_t node)
{
	mc_type = memctl_type();
}

void
have_counter(dnode_t node)
{
	int size;

	size = GETPROPLEN(node, OBP_ADDRESS);
	if (size == -1 || size > sizeof (v_counter_addr))
		cmn_err(CE_PANIC, "counter addr size");
	if (GETPROP(node, OBP_ADDRESS, (caddr_t)v_counter_addr) == -1)
		cmn_err(CE_PANIC, "counter address");
	v_level10clk_addr =
		(struct count10 *)v_counter_addr[size / sizeof (caddr_t) - 1];
	clock_addr = (uintptr_t)v_level10clk_addr;
}

void
have_interrupt(dnode_t node)
{
	int size;

	size = GETPROPLEN(node, OBP_ADDRESS);
	if (size == -1 || size > sizeof (v_interrupt_addr))
		cmn_err(CE_PANIC, "interrupt addr size");
	if (GETPROP(node, OBP_ADDRESS, (caddr_t)v_interrupt_addr) == -1)
		cmn_err(CE_PANIC, "interrupt address");
	v_sipr_addr = (struct sys_intreg *)
		v_interrupt_addr[size / sizeof (caddr_t) - 1];
}

void
have_leds()
{
	diagleds = 1;
}

void
have_auxio()
{
}

/*
 * Figure out which slots have DRAM.  This is only an
 * issue for C2, and on galaxy we later ignore the results calculated
 *
 * The memregs array must be larger than the number of physical slots
 * since one physical SIMM can contain multiple discontiguous chunks
 * of memory.  Currently the worst case is 32MB DSIMMS which have two
 * 16MB regions of RAM making the total number of regspecs <= 16.
 * Providing for 4 regions per slot should be adequate.
 */
void
have_memory(dnode_t node)
{
	struct prom_reg *rp, memregs[PMEM_NUMSLOTS * 4];
	u_int size, slot;
	int i;
	extern struct memslot memslots[];

	size = prom_getproplen(node, OBP_REG);
	if (size > sizeof (memregs)) {
		prom_printf("have_memory: too many memregs, remove a DSIMM\n");
		cmn_err(CE_PANIC,
		    "have_memory: too many memregs, remove a DSIMM\n");
		/*NOTREACHED*/
	}
	if (prom_getprop(node, OBP_REG, (caddr_t)memregs) == -1) {
		prom_printf("have_memory: panic mem regs\n");
		cmn_err(CE_PANIC, "have_memory: mem regs");
		/*NOTREACHED*/
	}
	rp = memregs;
	for (i = 0; i < (size / sizeof (struct prom_reg)); i++, rp++) {
		slot = PMEM_SLOT(rp->lo);
		/* ensure all regions in the same physical slot are the same */
		ASSERT(memslots[slot].ms_bustype == 0 ||
		    memslots[slot].ms_bustype == BT_DRAM);
		memslots[slot].ms_bustype = BT_DRAM;
	}
}


/*
 * Figure out which slots have NVRAM .  This is only an
 * issue for C2, and on galaxy we later ignore the results calculated
 */
void
have_nvsimms(dnode_t node)
{
	struct prom_reg *rp, nvsimregs[2];
	u_int size, slot;
	extern struct memslot memslots[];
	extern int nvsimm_present;

	size = prom_getproplen(node, OBP_REG);
	ASSERT(size == sizeof (nvsimregs));
	if (prom_getprop(node, OBP_REG, (caddr_t)nvsimregs) == -1)
		cmn_err(CE_PANIC, "nvsimm regs");
	rp = nvsimregs;
	slot = PMEM_SLOT(rp->lo);
	memslots[slot].ms_bustype = BT_NVRAM;
	/*
	 * Stash address of battery low register to later discard slot error
	 */
	memslots[slot].ms_fault_specific = (void *) ((++rp)->lo);
	nvsimm_present++;
}
