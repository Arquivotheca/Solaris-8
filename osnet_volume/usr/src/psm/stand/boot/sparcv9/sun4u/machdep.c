/*
 * Copyright (c) 1994-1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident "@(#)machdep.c	1.13	99/10/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/salib.h>

int pagesize = PAGESIZE;
int vac = 1;

void
fiximp(void)
{
	extern int use_align;

	use_align = 1;
}

void
setup_aux(void)
{
	pstack_t *stk;
	dnode_t node;
	dnode_t sp[OBP_STACKDEPTH];
	char name[OBP_MAXDRVNAME];
	static char cpubuf[2 * OBP_MAXDRVNAME];
	extern u_int icache_flush;
	extern char *cpulist;

	icache_flush = 1;
	stk = prom_stack_init(sp, sizeof (sp));
	node = prom_findnode_bydevtype(prom_rootnode(), "cpu", stk);
	if (node != OBP_NONODE && node != OBP_BADNODE) {
		if (prom_getprop(node, OBP_NAME, name) <= 0)
			prom_panic("no name in cpu node");
		(void) strcpy(cpubuf, name);
		if (prom_getprop(node, OBP_COMPATIBLE, name) > 0) {
			(void) strcat(cpubuf, ":");
			(void) strcat(cpubuf, name);
		}
		cpulist = cpubuf;
	} else
		prom_panic("no cpu node");
	prom_stack_fini(stk);
}


#ifdef MPSAS

void sas_symtab(int start, int end);
extern int sas_command(char *cmdstr);

/*
 * SAS support - inform SAS of new symbols being dynamically added
 * during simulation via the first standalone.
 */

#ifndef	BUFSIZ
#define	BUFSIZ	1024		/* for cmd string buffer allocation */
#endif

int	sas_symdebug = 0;		/* SAS support */

void
sas_symtab(int start, int end)
{
	char *addstr = "symtab add $LD_KERNEL_PATH/%s%s 0x%x 0x%x\n";
	char *file, *prefix, cmdstr[BUFSIZ];
	extern char filename[];

	file = filename;
	prefix = *file == '/' ? "../../.." : "";

	(void) sprintf(cmdstr, addstr, prefix, file, start, end);

	/* add the symbol table */
	if (sas_symdebug) (void) printf("sas_symtab: %s", cmdstr);
	sas_command(cmdstr);
}

void
sas_bpts()
{
	sas_command("file $KERN_SCRIPT_FILE\n");
}
#endif	/* MPSAS */

/*
 * Check if the CPU should default to 64-bit or not.
 * UltraSPARC-1's default to 32-bit mode.
 * Everything else defaults to 64-bit mode.
 */

/*
 * Manufacturer codes for the CPUs we're interested in
 */
#define	TI_JEDEC	0x17
#define	SUNW_JEDEC	0x22

/*
 * Implementation codes for the CPUs we're interested in
 */
#define	IMPL_US_I	0x10

static dnode_t
visit(dnode_t node)
{
	int impl, manu;
	char name[32];
	static char ultrasparc[] = "SUNW,UltraSPARC";
	static char implementation[] = "implementation#";
	static char manufacturer[] = "manufacturer#";

	/*
	 * if name isn't 'SUNW,UltraSPARC', continue.
	 */
	if (prom_getproplen(node, "name") != sizeof (ultrasparc))
		return ((dnode_t)0);
	(void) prom_getprop(node, "name", name);
	if (strncmp(name, ultrasparc, sizeof (ultrasparc)) != 0)
		return ((dnode_t)0);

	if (prom_getproplen(node, manufacturer) != sizeof (int))
		return ((dnode_t)0);
	(void) prom_getprop(node, manufacturer, (caddr_t)&manu);

	if ((manu != SUNW_JEDEC) && (manu != TI_JEDEC))
		return ((dnode_t)0);

	if (prom_getproplen(node, implementation) != sizeof (int))
		return ((dnode_t)0);
	(void) prom_getprop(node, implementation, (caddr_t)&impl);

	if (impl != IMPL_US_I)
		return ((dnode_t)0);

	return (node);
}

/*
 * visit each node in the device tree, until we get a non-null answer
 */
static dnode_t
walk(dnode_t node)
{
	dnode_t id;

	if (visit(node))
		return (node);

	for (node = prom_childnode(node); node; node = prom_nextnode(node))
		if ((id = walk(node)) != (dnode_t)0)
			return (id);

	return ((dnode_t)0);
}

/*
 * Check if the CPU is an UltraSPARC-1 or not.
 */
int
cpu_is_ultrasparc_1(void)
{
	static int cpu_checked;
	static int cpu_default;

	/*
	 * If we already checked, we already know the answer.
	 */
	if (cpu_checked == 0) {
		if (walk(prom_rootnode()))
			cpu_default = 1;
		cpu_checked = 1;
	}

	return (cpu_default);
}

/*
 * Retain a page or reclaim a previously retained page of physical
 * memory for use by the prom upgrade. If successful, leave
 * an indication that a page was retained by creating a boolean
 * property in the root node.
 *
 * XXX: SUNW,retain doesn't work as expected on server systems,
 * so we don't try to retain any memory on those systems.
 *
 * XXX: do a '0 to my-self' as a workaround for 4160914
 */

int dont_retain_memory;

void
retain_nvram_page(void)
{
	unsigned long long phys = 0;
	int len;
	char name[32];
	static char create_prop[] =
	    "0 to my-self dev / 0 0 \" boot-retained-page\" property";
	static char ue10000[] = "SUNW,Ultra-Enterprise-10000";
	static char ue[] = "SUNW,Ultra-Enterprise";
	extern int verbosemode;

	if (dont_retain_memory)
		return;

	len = prom_getproplen(prom_rootnode(), "name");
	if ((len != -1) && (len <= sizeof (name))) {
		(void) prom_getprop(prom_rootnode(), "name", name);
		if ((strcmp(name, ue) == 0) || (strcmp(name, ue10000) == 0))
			return;
	}

	if (prom_retain("OBPnvram", PAGESIZE, PAGESIZE, &phys) != 0) {
		printf("prom_retain failed\n");
		return;
	}
	if (verbosemode)
		printf("retained OBPnvram page at 0x%llx\n", phys);

	prom_interpret(create_prop, 0, 0, 0, 0, 0);
}
