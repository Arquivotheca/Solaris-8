/*
 * Copyright (c) 1993-2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)util.c	1.12	99/11/05 SMI"

#include <sys/cpr.h>
#include <sys/promimpl.h>
#include "cprboot.h"


/*
 * Ask prom to open a disk file given either the OBP device path, or the
 * device path representing the target drive/partition and the fs-relative
 * path of the file.  Handle file pathnames with or without leading '/'.
 * if fs points to a null char (sorry, but it always points to a static
 * buffer, and this is much less change to other code than passing in NULL)
 * it indicates that we are opening a character special device, not a UFS
 * file
 */
/* ARGSUSED */
int
cpr_statefile_open(char *path, char *fs)
{
	char full_path[OBP_MAXPATHLEN];
	char *fp;
	int handle;
	int c;

	/*
	 * instead of using specialstate, we use fs as the flag
	 */
	if (*fs == '\0') {	/* char statefile open */
		handle = prom_open(path);
		/* IEEE1275 prom_open returns 0 on failure; we return -1 */
		return (handle ? handle : -1);
	}

	/*
	 * IEEE 1275 prom needs "device-path,|file-path" where
	 * file-path can have embedded |'s.
	 */
	fp = full_path;
	(void) prom_strcpy(fp, fs);
	fp += prom_strlen(fp);
	*fp++ = ',';
	*fp++ = '|';

	/* Skip a leading slash in file path -- we provided for it above. */
	if (*path == '/')
		path++;

	/* Copy file path and convert separators. */
	while ((c = *path++) != '\0')
		if (c == '/')
			*fp++ = '|';
		else
			*fp++ = c;
	*fp = '\0';

	handle = prom_open(full_path);
	if (verbose) {
		if (fp = prom_strrchr(full_path, '/'))
			fp++;
		else
			fp = full_path;
		prom_printf("cso: prom_open(\"%s\") = 0x%x\n", fp, handle);
	}

	/*
	 * IEEE1275 prom_open returns 0 on failure; we return -1
	 */
	return (handle ? handle : -1);
}


/*
 * Ask prom to open a disk file given the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.  Handle file pathnames with or without leading '/'.
 * if fs points to a null char (sorry, but it always points to a static
 * buffer, and this is much less change to other code than passing in NULL)
 * it indicates that we are opening a character special device, not a UFS
 * file
 */
/* ARGSUSED */
int
cpr_ufs_open(char *path, char *fs)
{
	CB_VENTRY(cpr_ufs_open);

	/*
	 * screen invalid state, then just use the other code rather than
	 * duplicating it
	 */
	if (*fs == '\0') {	/* char statefile open */
		prom_printf("cpr_ufs_open: NULL fs, path %s\n", path);
		return (ERR);
	}
	return (cpr_statefile_open(path, fs));
}


/*
 * On sun4u there's no difference here, since prom groks ufs directly
 */
int
cpr_read(int fd, caddr_t buf, int len)
{
	return (prom_read(fd, buf, len, 0, 0));
}


int
cpr_ufs_read(int fd, caddr_t buf, int len)
{
	return (prom_read(fd, buf, len, 0, 0));
}


int
cpr_ufs_close(int fd)
{
	CB_VPRINTF(("cpr_ufs_close 0x%x\n", fd));
	return (prom_close(fd));
}


int
cpr_statefile_close(int fd)
{
	return (prom_close(fd));
}


void
cb_spin(void)
{
	static int spindex = 0;
	static char *spin_pairs[] = { "|\b", "/\b", "-\b", "\\\b" };

	prom_printf(spin_pairs[spindex]);
	if (++spindex == 3)
		spindex = 0;
}


/*
 * translate vaddr to phys page number
 */
pfn_t
cpr_vatopfn(caddr_t vaddr)
{
	physaddr_t paddr;
	int valid, mode;

	(void) prom_translate_virt(vaddr, &valid, &paddr, &mode);
	if (valid != -1)
		return (PFN_INVALID);
	return (paddr >> MMU_PAGESHIFT);
}


/*
 * unmap virt, then map virt to new phys;
 * see remap definition below
 */
int
prom_remap(size_t size, caddr_t virt, physaddr_t phys)
{
	ihandle_t immu;
	cell_t ci[8];
	int rv;

	immu = prom_mmu_ihandle();
	if (immu == (ihandle_t)-1)
		return (ERR);

	ci[0] = p1275_ptr2cell("call-method");	/* Service name */
	ci[1] = (cell_t)5;			/* #argument cells */
	ci[2] = (cell_t)0;			/* #result cells */
	ci[3] = p1275_ptr2cell("remap");	/* Arg1: Method name */
	ci[4] = p1275_ihandle2cell(immu);	/* Arg2: memory ihandle */
	ci[5] = p1275_size2cell(size);		/* remap arg0 */
	ci[6] = p1275_ptr2cell(virt);		/* remap arg1 */
	ci[7] = p1275_ull2cell_low(phys);	/* remap arg2 */

	promif_preprom();
	rv = p1275_cif_handler(ci);
	promif_postprom();

	if (rv)
		return (rv);		/* Service "call-method" failed */
	return (0);
}


/*
 * install remap definition in /virtual-memory node;
 * used for replacing a virt->phys mapping in one promif call;
 * this needs to be atomic from the client's perspective to
 * avoid faults while relocating client text.
 */
void
install_remap(void)
{
	static char remap_def[] =
	    "\" /virtual-memory\" find-device "
	    ": remap ( phys.lo virt size -- )"
	    "	2dup unmap ( phys.lo virt size )"
	    "	0 -rot -1 map ( ) ;";

	prom_interpret(remap_def, 0, 0, 0, 0, 0);
}


/*
 * allocate virt and phys space without any mapping;
 * stores virt and phys addrs at *vap and *pap
 */
int
cb_alloc(size_t size, uint_t align, caddr_t *vap, physaddr_t *pap)
{
	physaddr_t phys;
	caddr_t virt;

	if (align == 0)
		align = 1;
	virt = prom_allocate_virt(align, size);
	if (virt == (caddr_t)-1)
		return (ERR);
	if (prom_allocate_phys(size, align, &phys) == -1) {
		prom_free_virt(size, virt);
		return (ERR);
	}

	*vap = virt;
	*pap = phys;
	return (0);
}


static int
get_intprop(dnode_t node, caddr_t prop, void *dst)
{
	int len, glen;

	len = sizeof (uint_t);
	glen = prom_getprop(node, prop, dst);
	if (glen != len)
		return (ERR);

	return (0);
}


/*
 * find cpu node for the boot processor
 *
 * sets globals:
 * 	cb_mid
 */
static dnode_t
get_cpu_node(void)
{
	static char *props[] = { "upa-portid", "portid", NULL };
	dnode_t node, space[OBP_STACKDEPTH];
	char *str, *name, **propp;
	uint_t cpu_id;
	pstack_t *ps;
	int err;

	str = "get_cpu_node";
	name = "cpu";

	cb_mid = getmid();
	for (node = prom_rootnode(); ; node = prom_nextnode(node)) {
		ps = prom_stack_init(space, OBP_STACKDEPTH);
		node = prom_findnode_bydevtype(node, name, ps);
		prom_stack_fini(ps);
		if (node == OBP_NONODE) {
			prom_printf("\n%s: cant find node for devtype \"%s\"\n",
			    str, name);
			break;
		}

		cpu_id = (uint_t)-1;
		for (propp = props; *propp; propp++) {
			err = get_intprop(node, *propp, &cpu_id);
			CB_VPRINTF(("    cpu node 0x%x, "
			    "prop \"%s\", cpu_id %d\n",
			    node, *propp, (int)cpu_id));
			if (err == 0)
				break;
		}

		if (cpu_id == cb_mid)
			return (node);
	}

	return (OBP_NONODE);
}


/*
 * lookup prom properties
 *
 * sets globals:
 *	cpu_delay
 */
int
cb_get_props(void)
{
	uint_t clock_mhz;
	dnode_t node;
	struct cb_props *cbp;
	static struct cb_props cpu_data[] = {
		"#dtlb-entries", &cb_dents,
		"clock-frequency", &cb_clock_freq,
		NULL, NULL,
	};

	CB_VENTRY(cb_get_props);

	node = get_cpu_node();
	if (node == OBP_NONODE)
		return (ERR);
	for (cbp = cpu_data; cbp->prop; cbp++) {
		if (get_intprop(node, cbp->prop, cbp->datap)) {
			prom_printf("\n%s: getprop error, "
			    "node 0x%x, prop \"%s\"\n",
			    prog, node, cbp->prop);
			return (ERR);
		}
		CB_VPRINTF(("    \"%s\" = 0x%x\n",
		    cbp->prop, *cbp->datap));
	}

	/*
	 * setup cpu_delay for cb_usec_wait
	 */
	clock_mhz = (cb_clock_freq + 500000) / 1000000;
	cpu_delay = clock_mhz - 7;
	CB_VPRINTF(("    clock_mhz %d, cpu_delay %d\n",
	    clock_mhz, cpu_delay));

	return (0);
}


/*
 * map-in data pages
 * size should fit tte_bit.sz
 * rw should be 0 or TTE_HWWR_INT
 */
void
cb_mapin(caddr_t vaddr, pfn_t ppn, uint_t size, uint_t rw, uint_t dtlb_index)
{
	tte_t tte;

	tte.tte_inthi = TTE_VALID_INT | TTE_SZ_INT(size) |
	    TTE_PFN_INTHI(ppn);
	tte.tte_intlo = TTE_PFN_INTLO(ppn) | TTE_LCK_INT |
	    TTE_CP_INT | TTE_CV_INT | TTE_PRIV_INT | rw;
	set_dtlb_entry(dtlb_index, vaddr, &tte);
}
