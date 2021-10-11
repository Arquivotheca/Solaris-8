/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fillsysinfo.c	1.78	99/07/28 SMI"

#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/clock.h>

#include <sys/promif.h>
#include <sys/promimpl.h>
#include <sys/systm.h>
#include <sys/machsystm.h>
#include <sys/debug.h>
#include <sys/sysiosbus.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/spitregs.h>
#include <sys/kobj.h>

/*
 * The OpenBoot Standalone Interface supplies the kernel with
 * implementation dependent parameters through the devinfo/property mechanism
 */
typedef enum { XDRBOOL, XDRINT, XDRSTRING } xdrs;

/*
 * structure describing properties that we are interested in querying the
 * OBP for.
 */
struct getprop_info {
	char	*name;
	xdrs	type;
	uint_t	*var;
};

/*
 * structure used to convert between a string returned by the OBP & a type
 * used within the kernel. We prefer to paramaterize rather than type.
 */
struct convert_info {
	char	*name;
	uint_t	var;
	char	*realname;
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

int ncpunode;
struct cpu_node cpunodes[NCPU];

static void	check_cpus(void);
void	fill_cpu(dnode_t);
static void	adj_ecache_size(int);
void	empty_cpu(int);

uint_t	system_clock_freq;

extern dnode_t iommu_nodes[];

/*
 * list of well known devices that must be mapped, and the variables that
 * contain their addresses.
 */
caddr_t		v_auxio_addr = NULL;
caddr_t		v_eeprom_addr = NULL;
caddr_t		v_timecheck_addr = NULL;
caddr_t		v_rtc_addr_reg = NULL;
volatile unsigned char *v_rtc_data_reg = NULL;

int		niobus = 0;

/*
 * Hardware watchdog support.
 */
#define	CHOSEN_EEPROM	"eeprom"
#define	WATCHDOG_ENABLE "watchdog-enable"
static dnode_t 		chosen_eeprom;

/*
 * Appropriate tod module will be dynamically selected while booting
 * based on finding a device tree node with a "device_type" property value
 * of "tod". If such a node describing tod is not found, for backward
 * compatibility, a node with a "name" property value of "eeprom" and
 * "model" property value of "mk48t59" will be used. Failing to find a
 * node matching either of the above criteria will result in no tod module
 * being selected; this will cause the boot process to halt.
 */
char	*tod_module_name;

/*
 * Some nodes have functions that need to be called when they're seen.
 */
static void	have_sbus(dnode_t);
static void	have_pci(dnode_t);
static void	have_eeprom(dnode_t);
static void	have_auxio(dnode_t);
static void	have_rtc(dnode_t);
#ifndef __sparcv9
static void	have_flashprom(dnode_t);
#endif
static void	have_tod(dnode_t);

static struct wkdevice {
	char *wk_namep;
	void (*wk_func)(dnode_t);
	caddr_t *wk_vaddrp;
	ushort_t wk_flags;
#define	V_OPTIONAL	0x0000
#define	V_MUSTHAVE	0x0001
#define	V_MAPPED	0x0002
#define	V_MULTI		0x0003	/* optional, may be more than one */
} wkdevice[] = {
	{ "sbus", have_sbus, NULL, V_MULTI },
	{ "pci", have_pci, NULL, V_MULTI },
	{ "eeprom", have_eeprom, NULL, V_MULTI },
	{ "auxio", have_auxio, NULL, V_OPTIONAL },
#ifndef __sparcv9
	{ "flashprom", have_flashprom, NULL, V_MULTI },
#endif
	{ "rtc", have_rtc, NULL, V_OPTIONAL },
	{ 0, },
};

static void map_wellknown(dnode_t);

void
map_wellknown_devices()
{
	struct wkdevice *wkp;
	phandle_t	ieeprom;
	dnode_t	root;

	/*
	 * if there is a chosen eeprom, note it (for have_eeprom())
	 */
	if (GETPROPLEN(prom_chosennode(), CHOSEN_EEPROM) ==
	    sizeof (phandle_t) &&
	    GETPROP(prom_chosennode(), CHOSEN_EEPROM, (caddr_t)&ieeprom) != -1)
		chosen_eeprom = (dnode_t)prom_decode_int(ieeprom);

	root = prom_nextnode((dnode_t)0);
	/*
	 * Get System clock frequency from root node if it exists.
	 */
	(void) GETPROP(root, "stick-frequency", (caddr_t)&system_clock_freq);

	map_wellknown(NEXT((dnode_t)0));

	/*
	 * See if it worked
	 */
	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (wkp->wk_flags == V_MUSTHAVE) {
			cmn_err(CE_PANIC, "map_wellknown_devices: required "
			    "device %s not mapped", wkp->wk_namep);
		}
	}

	/*
	 * all sun4u systems must have an IO bus, i.e. sbus or pcibus
	 */
	if (niobus == 0)
		cmn_err(CE_PANIC, "map_wellknown_devices: no i/o bus node");

	check_cpus();
}

/*
 * map_wellknown - map known devices & registers
 */
static void
map_wellknown(dnode_t curnode)
{
	extern int status_okay(int, char *, int);
	char tmp_name[MAXSYSNAME];
	static void fill_address(dnode_t, char *);

#ifdef VPRINTF
	VPRINTF("map_wellknown(%x)\n", curnode);
#endif /* VPRINTF */

	for (curnode = CHILD(curnode); curnode; curnode = NEXT(curnode)) {
		/*
		 * prune subtree if status property indicating not okay
		 */
		if (!status_okay((int)curnode, (char *)NULL, 0)) {
			char devtype_buf[OBP_MAXPROPNAME];
			int size;

#ifdef VPRINTF
			VPRINTF("map_wellknown: !okay status property\n");
#endif /* VPRINTF */
			/*
			 * a status property indicating bad memory will be
			 * associated with a node which has a "device_type"
			 * property with a value of "memory-controller"
			 */
			if ((size = GETPROPLEN(curnode,
			    OBP_DEVICETYPE)) == -1)
				continue;
			if (size > OBP_MAXPROPNAME) {
				cmn_err(CE_CONT, "node %x '%s' prop too "
				    "big\n", curnode, OBP_DEVICETYPE);
				continue;
			}
			if (GETPROP(curnode, OBP_DEVICETYPE,
			    devtype_buf) == -1) {
				cmn_err(CE_CONT, "node %x '%s' get failed\n",
				    curnode, OBP_DEVICETYPE);
				continue;
			}
			if (strcmp(devtype_buf, "memory-controller") != 0)
				continue;
			/*
			 * ...else fall thru and process the node...
			 */
		}
		bzero(tmp_name, MAXSYSNAME);
		if (GETPROP(curnode, OBP_NAME, (caddr_t)tmp_name) != -1)
			fill_address(curnode, tmp_name);
		if (GETPROP(curnode, OBP_DEVICETYPE, tmp_name) != -1 &&
		    strcmp(tmp_name, "cpu") == 0) {
			fill_cpu(curnode);
		}
		if (strcmp(tmp_name, "tod") == 0)
			have_tod(curnode);
		map_wellknown(curnode);
	}
}

static void
fill_address(dnode_t curnode, char *namep)
{
	struct wkdevice *wkp;
	int size;
	uint32_t vaddr;

	for (wkp = wkdevice; wkp->wk_namep; ++wkp) {
		if (strcmp(wkp->wk_namep, namep) != 0)
			continue;
		if (wkp->wk_flags == V_MAPPED)
			return;
		if (wkp->wk_vaddrp != NULL) {
			if ((size = GETPROPLEN(curnode, OBP_ADDRESS)) == -1) {
				cmn_err(CE_CONT, "device %s size %d\n",
				    namep, size);
				continue;
			}
			if (size != sizeof (vaddr)) {
				cmn_err(CE_CONT, "device %s address prop too "
				    "big\n", namep);
				continue;
			}
			if (GETPROP(curnode, OBP_ADDRESS,
			    (caddr_t)&vaddr) == -1) {
				cmn_err(CE_CONT, "device %s not mapped\n",
				    namep);
				continue;
			}

			/* make into a native pointer */
			*wkp->wk_vaddrp = (caddr_t)vaddr;
#ifdef VPRINTF
			VPRINTF("fill_address: %s mapped to %x\n", namep,
			    *wkp->wk_vaddrp);
#endif /* VPRINTF */
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

/*
 * set ecache size to the largest Ecache of all the cpus
 * in the system. setting it to largest E$ is desirable
 * for page coloring.
 * We also need ecache_size for finding a displacement flush region.
 */
void
adj_ecache_size(int new_size)
{
	if (new_size > ecache_size) {
		ecache_size = new_size;
	}
}

void
fill_cpu(dnode_t node)
{
	void fiximp_obp();
	struct cpu_node *cpunode;
	int upaid;

	/*
	 * use upa port id as the index to cpunodes[]
	 */
	if (GETPROP(node, "portid", (caddr_t)&upaid) == -1)
		if (GETPROP(node, "upa-portid", (caddr_t)&upaid) == -1)
			cmn_err(CE_PANIC, "portid not found");
	cpunode = &cpunodes[upaid];
	cpunode->upaid = upaid;
	(void) GETPROP(node, "name", cpunode->name);
	(void) GETPROP(node, "implementation#",
	    (caddr_t)&cpunode->implementation);
	(void) GETPROP(node, "mask#", (caddr_t)&cpunode->version);
	(void) GETPROP(node, "clock-frequency", (caddr_t)&cpunode->clock_freq);

	/*
	 * If we didn't find it in the CPU node, look in the root node.
	 */
	if (cpunode->clock_freq == 0) {
		dnode_t root = prom_nextnode((dnode_t)0);
		(void) GETPROP(root, "clock-frequency",
		    (caddr_t)&cpunode->clock_freq);
	}

	cpunode->nodeid = node;

	if (ncpunode == 0) {
		(void) fiximp_obp(node);
		cpunode->ecache_size = ecache_size;
	} else {
		int size = 0;
		(void) GETPROP(node, "ecache-size", (caddr_t)&size);
		ASSERT(size != 0);
		cpunode->ecache_size = size;
		adj_ecache_size(size);
	}
	ncpunode++;
}

void
empty_cpu(int cpuid)
{
	bzero(&cpunodes[cpuid], sizeof (struct cpu_node));
	ncpunode--;
}

#ifdef SF_ERRATA_30 /* call causes fp-disabled */
int spitfire_call_bug = 0;
#endif
#ifdef SF_V9_TABLE_28	/* fp over/underflow traps may cause wrong fsr.cexc */
int spitfire_bb_fsr_bug = 0;
#endif

static void
check_cpus(void)
{
	int i;
	int impl, cpuid = getprocessorid();
	char *msg = NULL;
	extern use_mp;
	int min_supported_rev;

	ASSERT(cpunodes[cpuid].nodeid != 0);

	/*
	 * We check here for illegal cpu combinations.
	 * Currently, we check that the implementations are the same.
	 */
	impl = cpunodes[cpuid].implementation;
	switch (impl) {
	default:
		min_supported_rev = 0;
		break;
	case SPITFIRE_IMPL:
		min_supported_rev = SPITFIRE_MINREV_SUPPORTED;
		break;
	}

	for (i = 0; i < NCPU; i++) {
		if (cpunodes[i].nodeid == 0)
			continue;

		if (IS_SPITFIRE(impl) &&
		    cpunodes[i].version < min_supported_rev) {
			cmn_err(CE_PANIC, "UltraSPARC versions older than "
		"%d.%d are no longer supported (cpu #%d)",
			SPITFIRE_MAJOR_VERSION(min_supported_rev),
			SPITFIRE_MINOR_VERSION(min_supported_rev), i);
		}

		/*
		 * Min supported rev is 2.1 but we've seen problems
		 * with that so we still want to warn if we see one.
		 */
		if (IS_SPITFIRE(impl) && cpunodes[i].version < 0x22) {
			cmn_err(CE_WARN, "UltraSPARC versions older than "
			    "2.2 are not supported (cpu #%d)", i);
		}

#ifdef SF_ERRATA_30 /* call causes fp-disabled */
		if (IS_SPITFIRE(impl) && cpunodes[i].version < 0x22)
			spitfire_call_bug = 1;
#endif /* SF_ERRATA_30 */
#ifdef SF_V9_TABLE_28	/* fp over/underflow traps may cause wrong fsr.cexc */
		if (IS_SPITFIRE(impl) || IS_BLACKBIRD(impl))
			spitfire_bb_fsr_bug = 1;
#endif /* SF_V9_TABLE_28 */

		if (cpunodes[i].implementation != impl) {
			msg = " on mismatched modules";
			break;
		}
	}
	if (msg != NULL) {
		cmn_err(CE_NOTE, "MP not supported%s, booting UP only", msg);
		for (i = 0; i < NCPU; i++) {
			if (cpunodes[i].nodeid == 0)
				continue;
			cmn_err(CE_NOTE, "cpu%d: %s version 0x%x",
			    cpunodes[i].upaid,
			    cpunodes[i].name, cpunodes[i].version);
		}
		use_mp = 0;
	}
	/*
	 * Set max cpus we can have based on ncpunode and use_mp
	 */

	if (use_mp) {
		int (*set_max_ncpus)(void);

		set_max_ncpus = (int (*)(void))
			kobj_getsymvalue("set_platform_max_ncpus", 0);

		if (set_max_ncpus) {
			max_ncpus = set_max_ncpus();
			boot_max_ncpus = ncpunode;
		} else {
			max_ncpus = ncpunode;
		}
	} else {
		max_ncpus = 1;
	}
}

/*
 * The first sysio must always programmed up for the system clock and error
 * handling purposes, referenced by v_sysio_addr in machdep.c.
 */
static void
have_sbus(dnode_t node)
{
	int size;
	uint_t portid;

	size = GETPROPLEN(node, "upa-portid");
	if (size == -1 || size > sizeof (portid))
		cmn_err(CE_PANIC, "upa-portid size");
	if (GETPROP(node, "upa-portid", (caddr_t)&portid) == -1)
		cmn_err(CE_PANIC, "upa-portid");

	niobus++;

	/*
	 * mark each entry that needs a physical TSB
	 */
	iommu_nodes[portid] = node;
}


/*
 * The first psycho must always programmed up for the system clock and error
 * handling purposes.
 */
static void
have_pci(dnode_t node)
{
	int size;
	uint_t portid;

	size = GETPROPLEN(node, "portid");
	if (size == -1) size = GETPROPLEN(node, "upa-portid");
	if (size == -1)
		return;
	if (size > sizeof (portid))
		cmn_err(CE_PANIC, "portid size wrong");

	if (GETPROP(node, "portid", (caddr_t)&portid) == -1)
		if (GETPROP(node, "upa-portid", (caddr_t)&portid) == -1)
			cmn_err(CE_PANIC, "portid not found");

	niobus++;

	/*
	* mark each entry that needs a physical TSB
	*/
	iommu_nodes[portid] = node;
}

/*
 * The first eeprom is used as the TOD clock, referenced
 * by v_eeprom_addr in locore.s.
 */
static void
have_eeprom(dnode_t node)
{
	int size;
	uint32_t eaddr;

	/*
	 * "todmostek" module will be selected based on finding a "model"
	 * property value of "mk48t59" in the "eeprom" node.
	 */
	if (tod_module_name == NULL) {
		char buf[MAXSYSNAME];

		if ((GETPROP(node, "model", buf) != -1) &&
		    (strcmp(buf, "mk48t59") == 0))
			tod_module_name = "todmostek";
	}

	/*
	 * If we have found two distinct eeprom's, then we're done.
	 */
	if (v_eeprom_addr && v_timecheck_addr != v_eeprom_addr)
		return;

	/*
	 * multiple eeproms may exist but at least
	 * one must have an "address" property
	 */
	if ((size = GETPROPLEN(node, OBP_ADDRESS)) == -1)
		return;
	if (size != sizeof (eaddr))
		cmn_err(CE_PANIC, "eeprom addr size");
	if (GETPROP(node, OBP_ADDRESS, (caddr_t)&eaddr) == -1)
		cmn_err(CE_PANIC, "eeprom addr");

	/*
	 * If we have a chosen eeprom and it is not this node, keep looking.
	 */
	if (chosen_eeprom != NULL && chosen_eeprom != node) {
		v_timecheck_addr = (caddr_t)eaddr;
		return;
	}

	v_eeprom_addr = (caddr_t)eaddr;

	/*
	 * If we don't find an I/O board to use to check the clock,
	 * we'll fall back on whichever TOD is available.
	 */
	if (v_timecheck_addr == NULL)
		v_timecheck_addr = v_eeprom_addr;

	/*
	 * Does this eeprom have watchdog support?
	 */
	if (GETPROPLEN(node, WATCHDOG_ENABLE) != -1)
		watchdog_available = 1;
}

static void
have_rtc(dnode_t node)
{
	int size;
	uint32_t eaddr;

	/*
	 * "ds1287" module will be selected based on finding a "model"
	 * property value of "ds1287" in the "rtc" node.
	 */
	if (tod_module_name == NULL) {
		char buf[MAXSYSNAME];

		if ((GETPROP(node, "model", buf) != -1) &&
		    (strcmp(buf, "ds1287") == 0))
			tod_module_name = "todds1287";
	}

	/*
	 * XXX - drives on if address prop doesn't exist, later falls
	 * over in tod module
	 */
	if ((size = GETPROPLEN(node, OBP_ADDRESS)) == -1)
		return;
	if (size != sizeof (eaddr))
		cmn_err(CE_PANIC, "rtc addr size");
	if (GETPROP(node, OBP_ADDRESS, (caddr_t)&eaddr) == -1)
		cmn_err(CE_PANIC, "rtc addr");

	v_rtc_addr_reg = (caddr_t)eaddr;
	v_rtc_data_reg = (volatile unsigned char *)eaddr + 1;

	/*
	 * Does this rtc have watchdog support?
	 */
	if (GETPROPLEN(node, WATCHDOG_ENABLE) != -1)
		watchdog_available = 1;
}

static void
have_auxio(dnode_t node)
{
	size_t size, n;
	uint32_t addr[5];

	/*
	 * Get the size of the auzio's address property.
	 * On some platforms, the address property contains one
	 * entry and on others it contains five entries.
	 * In all cases, the first entries are compatible.
	 *
	 * This routine gets the address property for the auxio
	 * node and stores the first entry in v_auxio_addr which
	 * is used by the routine set_auxioreg in sun4u/ml/locore.s.
	 */
	if ((size = GETPROPLEN(node, OBP_ADDRESS)) == -1)
		cmn_err(CE_PANIC, "no auxio address property");

	switch (n = (size / sizeof (addr[0]))) {
	case 1:
		break;
	case 5:
		break;
	default:
		cmn_err(CE_PANIC, "auxio addr has %lu entries?", n);
	}

	if (GETPROP(node, OBP_ADDRESS, (caddr_t)addr) == -1)
		cmn_err(CE_PANIC, "auxio addr");

	v_auxio_addr = (caddr_t)addr[0];	/* make into a C pointer */
}

static void
have_tod(dnode_t node)
{
	static char tod_name[MAXSYSNAME];

	if (GETPROP(node, OBP_NAME, (caddr_t)tod_name) == -1)
		cmn_err(CE_PANIC, "tod name");
	/*
	 * This is a node with "device_type" property value of "tod".
	 * Name of the tod module is the name from the node.
	 */
	tod_module_name = tod_name;
}


#ifndef __sparcv9
/*
 * Table listing the minimum prom versions supported by this kernel.
 * LP64 See Also: psm/promif/ieee1275/sun4u/prom_vercheck.c
 */
static struct obp_rev_table {
	char *model;
	char *version;
	int level;
} obp_min_revs[] = {
	{ /* neutron */
	"SUNW,525-1410", "OBP 3.0.4 1995/11/26 17:47", CE_WARN },
	{ /* neutron+ */
	"SUNW,525-1448", "OBP 3.0.2 1995/11/26 17:52", CE_WARN },
	{ /* electron */
	"SUNW,525-1411", "OBP 3.0.4 1995/11/26 17:57", CE_WARN },
	{ /* pulsar */
	"SUNW,525-1414", "OBP 3.1.0 1996/03/05 09:00", CE_WARN },
	{ /* sunfire */
	"SUNW,525-1431", "OBP 3.1.0 1996/02/12 18:57", CE_WARN },
	{ NULL, NULL, 0 }
};

#define	NMINS	60
#define	NHOURS	24
#define	NDAYS	31
#define	NMONTHS	12

#define	YEAR(y)	 ((y-1) * (NMONTHS * NDAYS * NHOURS * NMINS))
#define	MONTH(m) ((m-1) * (NDAYS * NHOURS * NMINS))
#define	DAY(d)   ((d-1) * (NHOURS * NMINS))
#define	HOUR(h)  ((h)   * (NMINS))
#define	MINUTE(m) (m)

/*
 * XXX - Having this here isn't cool.  There's another copy
 * in the rpc code.
 */
static int
strtoi(const char *str, const char **pos)
{
	int c;
	int val = 0;

	for (c = *str++; c >= '0' && c <= '9'; c = *str++) {
		val *= 10;
		val += c - '0';
	}
	if (pos)
		*pos = str;
	return (val);
}


/*
 * obp_timestamp: based on the OBP flashprom version string of the
 * format "OBP x.y.z YYYY/MM/DD HH:MM" calculate a timestamp based
 * on the year, month, day, hour and minute by turning that into
 * a number of minutes.
 */
static int
obp_timestamp(const char *v)
{
	const char *c;
	int maj, year, month, day, hour, min;

	if (v[0] != 'O' || v[1] != 'B' || v[2] != 'P')
		return (-1);

	c = v + 3;

	/* Find first non-space character after OBP */
	while (*c != '\0' && (*c == ' ' || *c == '\t'))
		c++;
	if (strlen(c) < 5)		/* need at least "x.y.z" */
		return (-1);

	maj = strtoi(c, &c);
	if (maj < 3)
		return (-1);

#if 0 /* XXX - not used */
	dot = dotdot = 0;
	if (*c == '.') {
		dot = strtoi(c + 1, &c);

		/* optional? dot-dot release */
		if (*c == '.')
			dotdot = strtoi(c + 1, &c);
	}
#endif

	/* Find space at the end of version number */
	while (*c != '\0' && *c != ' ')
		c++;
	if (strlen(c) < 11)		/* need at least " xxxx/xx/xx" */
		return (-1);

	/* Point to first character of date */
	c++;

	/* Validate date format */
	if (c[4] != '/' || c[7] != '/')
		return (-1);

	year = strtoi(c, NULL);
	month = strtoi(c + 5, NULL);
	day = strtoi(c + 8, NULL);

	if (year < 1995 || month == 0 || day == 0)
		return (-1);

	/*
	 * Find space at the end of date number
	 */
	c += 10;
	while (*c != '\0' && *c != ' ')
		c++;
	if (strlen(c) < 6)		/* need at least " xx:xx" */
		return (-1);

	/* Point to first character of time */
	c++;

	if (c[2] != ':')
		return (-1);

	hour = strtoi(c, NULL);
	min = strtoi(c + 3, NULL);

	return (YEAR(year) + MONTH(month) +
	    DAY(day) + HOUR(hour) + MINUTE(min));
}


/*
 * Check the prom against the obp_min_revs table and complain if
 * the system has an older prom installed.  The actual major/minor/
 * dotdot numbers are not checked, only the date/time stamp.
 */
static void
have_flashprom(dnode_t node)
{
	int tstamp, min_tstamp;
	char vers[512], model[64];
	int plen;
	struct obp_rev_table *ortp;

	plen = GETPROPLEN(node, "model");
	if (plen <= 0 || plen > sizeof (model)) {
		cmn_err(CE_WARN, "have_flashprom: invalid model "
		    "property, not checking prom version");
		return;
	}
	if (GETPROP(node, "model", model) == -1) {
		cmn_err(CE_WARN, "have_flashprom: error getting model "
		    "property, not checking prom version");
		return;
	}
	model[plen] = '\0';

	plen = GETPROPLEN(node, "version");
	if (plen == -1) {
		/* no version property, ignore */
		return;
	}
	if (plen == 0 || plen > sizeof (vers)) {
		cmn_err(CE_WARN, "have_flashprom: invalid version "
		    "property, not checking prom version");
		return;
	}
	if (GETPROP(node, "version", vers) == -1) {
		cmn_err(CE_WARN, "have_flashprom: error getting version "
		    "property, not checking prom version");
		return;
	}
	vers[plen] = '\0';

	/* Make sure it's an OBP flashprom */
	if (vers[0] != 'O' && vers[1] != 'B' && vers[2] != 'P')
		return;

#ifdef VPRINTF
	VPRINTF("fillsysinfo: Found OBP flashprom: %s\n", vers);
#endif

	tstamp = obp_timestamp(vers);
	if (tstamp == -1) {
		cmn_err(CE_WARN, "have_flashprom: node contains "
		    "improperly formatted version property,\n"
		    "\tnot checking prom version");
		return;
	}

	for (ortp = obp_min_revs; ortp->model != NULL; ortp++) {
		if (strcmp(model, ortp->model) == 0) {
			min_tstamp = obp_timestamp(ortp->version);
			if (min_tstamp == -1) {
				cmn_err(CE_WARN, "have_flashprom: "
				    "invalid OBP version string in table "
				    " (entry %d)", (int)(ortp - obp_min_revs));
				continue;
			}
			if (tstamp < min_tstamp) {
				cmn_err(ortp->level, "Down-rev OBP detected.  "
				"Please update to at least:\n\t%s",
				    ortp->version);
				break;
			}
		}
	} /* for each obp_rev_table entry */
}
#endif
