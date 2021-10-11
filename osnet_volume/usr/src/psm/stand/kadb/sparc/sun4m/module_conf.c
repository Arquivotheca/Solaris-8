/*
 * Copyright (c) 1991-1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)module_conf.c	1.6	94/05/21 SMI"
	/* From SunOS 4.1.1 */

#include <sys/param.h>
#include <sys/module.h>

/*
 * add extern declarations for module drivers here
 */

extern int	ross_module_identify();
extern void	ross_module_setup();

extern int	ross625_module_identify();
extern void	ross625_module_setup();

extern int	vik_module_identify();
extern void	vik_module_setup();

extern int	tsu_module_identify();
extern void	tsu_module_setup();

extern int	swift_module_identify();
extern void	swift_module_setup();

/*
 * module driver table
 */

struct module_linkage module_info[] = {
	{ ross_module_identify, ross_module_setup },
	{ ross625_module_identify, ross625_module_setup },
	{ vik_module_identify, vik_module_setup },
	{ tsu_module_identify, tsu_module_setup },
	{ swift_module_identify, swift_module_setup },
/*
 * add module driver entries here
 */
};

int	module_info_size = sizeof (module_info) / sizeof module_info[0];
