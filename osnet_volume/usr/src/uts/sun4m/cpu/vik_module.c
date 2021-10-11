/*
 * Copyright (c) 1987-1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)vik_module.c	1.1	96/05/17 SMI"

#include <sys/types.h>
#include <sys/cpu_module.h>
#include <sys/machparam.h>
#include <sys/module.h>

/*
 * This file is linked with the cpu specific TI module
 *
 * It provides the common interfaces and stubs all cpu modules
 * must contain.
 */

extern int	vik_module_identify();
extern void	vik_module_setup();

struct module_linkage module_info[] = {
	{ vik_module_identify, vik_module_setup }
};

int	module_info_size = sizeof (module_info) / sizeof (module_info[0]);
