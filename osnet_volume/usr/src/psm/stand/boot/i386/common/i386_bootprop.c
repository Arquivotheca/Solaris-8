/*
 * Copyright (c) 1992-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)i386_bootprop.c	1.2	99/05/21 SMI"

#include <sys/types.h>
#include <sys/bootconf.h>
#include <sys/machine.h>
#include <sys/salib.h>
#include <values.h>
#include "devtree.h"

extern struct memlist *vfreelistp, *vinstalledp, *pfreelistp, *pinstalledp;
extern struct memlist *bphyslistp, *bvirtlistp;

extern char *backfs_dev, *backfs_fstype;
extern char *frontfs_dev, *frontfs_fstype;
extern unsigned int acpi_status_prop;
extern unsigned int acpi_debug_prop;
extern unsigned int acpi_options_prop;

extern int install_memlistptrs();
extern int get_end();

extern int get_memlist();
extern int get_string(), get_word();
extern int chario_get_dev(), chario_put_dev();
extern int chario_get_mode(), chario_put_mode();
extern int chario_get_cd(), chario_put_cd();
extern int chario_get_rts(), chario_put_rts();
extern int serial_direct_put(), serial_direct_get();
extern int put_hex_int();


/*
 *  "global" pseudo-property values used by older kernels:
 */

static int zero = 0; /* File descriptor for stdin/stdout */

extern int bootops_extensions;
extern int vac, cache_state;
extern char *impl_arch_name, *module_path, *kernname, *systype;

/*
 * Pseudo properties:
 *
 * The properties listed in the table below are special in that they
 * are not stored in contiguous memory location.  Hence a simple "memcpy"
 * is not sufficient to extract the corresponding values.  Instead, we
 * use a special-purpose "get" routine, whose address is recorded in the
 * table.  A corresponding "put" routine may be used to set the value
 * of a pseduo-property.
 */

struct pprop pseudo_props[] =
{
	/* BEGIN CSTYLED */
	/*
	 * NOTE:  This table must be maintained in order of property name
	 * within node address (so we use a binary search).  The
	 * partial ordering for nodes is:
	 *
	 * root  >  boot   >  bootmem  >  alias   >  chosen  >
	 * mem   >  mmu    >  prom     >  option  >  package
	 */
	/* END CSTYLED */

	{&root_node, "acpi-status",
		get_word, (void *)&acpi_status_prop,
		put_hex_int, (void *)&acpi_status_prop},
	{&bootmem_node, "reg", get_memlist, (void *)&bphyslistp, 0, 0},
	{&bootmem_node, "virtual", get_memlist, (void *)&bvirtlistp, 0, 0},
	{&chosen_node, "backfs-fstype", get_string, (void *)&backfs_fstype,
	    0, 0},
	{&chosen_node, "backfs-path", get_string, (void *)&backfs_dev, 0, 0},
	{&chosen_node, "boot-end", get_end, 0, 0, 0},
	{&chosen_node, "bootops-extensions", get_word,
	    (void *)&bootops_extensions, 0, 0},
	{&chosen_node, "cache-on?", get_word, (void *)&cache_state, 0, 0},
	{&chosen_node, "default-name", get_string, (void *)&kernname, 0, 0},
	{&chosen_node, "frontfs-fstype", get_string, (void *)&frontfs_fstype,
	    0, 0},
	{&chosen_node, "frontfs-path", get_string, (void *)&frontfs_dev, 0, 0},
	{&chosen_node, "fstype", get_string, (void *)&systype, 0, 0},
	{&chosen_node, "impl-arch-name", get_string, (void *)&impl_arch_name,
	    0, 0},
	{&chosen_node, "memory-update", install_memlistptrs, 0, 0, 0},
	/*
	 * both mfg-name and impl-arch-name point to the same thing which
	 * is the name of the root node
	 */
	{&chosen_node, "mfg-name", get_string, (void *)&impl_arch_name, 0, 0},
	{&chosen_node, "phys-avail", get_memlist, (void *)&pfreelistp, 0, 0},
	{&chosen_node, "phys-installed", get_memlist, (void *)&pinstalledp,
	    0, 0},
	{&chosen_node, "stdin", get_word, (void *)&zero, 0, 0},
	{&chosen_node, "stdout", get_word, (void *)&zero, 0, 0},
	{&chosen_node, "vac", get_word, (void *)&vac, 0, 0},
	{&chosen_node, "virt-avail", get_memlist, (void *)&vfreelistp, 0, 0},
	{&mem_node, "available", get_memlist, (void *)&pfreelistp, 0, 0},
	{&mem_node, "reg", get_memlist, (void *)&pinstalledp, 0, 0},
	{&mmu_node, "available", get_memlist, (void *)&vfreelistp, 0, 0},
	{&mmu_node, "existing", get_memlist, (void *)&vinstalledp, 0, 0},
	{&option_node, "acpi-debug",
		get_word, (void *)&acpi_debug_prop,
		put_hex_int, (void *)&acpi_debug_prop},
	{&option_node, "acpi-user-options",
		get_word, (void *)&acpi_options_prop,
		put_hex_int, (void *)&acpi_options_prop},
	{&option_node, "asy-direct", serial_direct_get, (void *)"asy-direct",
	    serial_direct_put, (void *)"asy-direct"},
	{&option_node, "com1-ignore-cd", chario_get_cd,
	    (void *)"com1-ignore-cd",
	    chario_put_cd, (void *)"com1-ignore-cd"},
	{&option_node, "com1-mode", chario_get_mode, (void *)"com1-mode",
	    chario_put_mode, (void *)"com1-mode"},
	{&option_node, "com1-rts-dtr-off", chario_get_rts,
	    (void *)"com1-rts-dtr-off",
	    chario_put_rts, (void *)"com1-rts-dtr-off"},
	{&option_node, "com2-ignore-cd", chario_get_cd,
	    (void *)"com2-ignore-cd",
	    chario_put_cd, (void *)"com2-ignore-cd"},
	{&option_node, "com2-mode", chario_get_mode, (void *)"com2-mode",
	    chario_put_mode, (void *)"com2-mode"},
	{&option_node, "com2-rts-dtr-off", chario_get_rts,
	    (void *)"com2-rts-dtr-off",
	    chario_put_rts, (void *)"com2-rts-dtr-off"},
	{&option_node, "input-device", chario_get_dev, (void *)"input-device",
	    chario_put_dev, (void *)"input-device"},
	{&option_node, "module-path", get_string, (void *)&module_path, 0, 0},
	{&option_node, "output-device", chario_get_dev, (void *)"output-device",
	    chario_put_dev, (void *)"output-device"},
	{&option_node, "ttya-ignore-cd", chario_get_cd,
	    (void *)"ttya-ignore-cd",
	    chario_put_cd, (void *)"ttya-ignore-cd"},
	{&option_node, "ttya-mode", chario_get_mode, (void *)"ttya-mode",
	    chario_put_mode, (void *)"ttya-mode"},
	{&option_node, "ttya-rts-dtr-off", chario_get_rts,
	    (void *)"ttya-rts-dtr-off",
	    chario_put_rts, (void *)"ttya-rts-dtr-off"},
	{&option_node, "ttyb-ignore-cd", chario_get_cd,
	    (void *)"ttyb-ignore-cd",
	    chario_put_cd, (void *)"ttyb-ignore-cd"},
	{&option_node, "ttyb-mode", chario_get_mode, (void *)"ttyb-mode",
	    chario_put_mode, (void *)"ttyb-mode"},
	{&option_node, "ttyb-rts-dtr-off", chario_get_rts,
	    (void *)"ttyb-rts-dtr-off",
	    chario_put_rts, (void *)"ttyb-rts-dtr-off"},
};

int pseudo_prop_count = sizeof (pseudo_props)/sizeof (struct pprop);
