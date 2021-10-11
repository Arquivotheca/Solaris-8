/*
 * Copyright (c) 1987-1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)tsu_module.c	1.1	96/05/17 SMI"

#include <sys/types.h>
#include <sys/cpu_module.h>
#include <sys/machparam.h>
#include <sys/module.h>

/*
 * This file is linked with the cpu specific Tsunami module
 *
 * It provides the common interfaces and stubs all cpu modules
 * must contain.
 */

extern int	tsu_module_identify();
extern void	tsu_module_setup();

struct module_linkage module_info[] = {
	{ tsu_module_identify, tsu_module_setup },
};

int	module_info_size = sizeof (module_info) / sizeof (module_info[0]);
