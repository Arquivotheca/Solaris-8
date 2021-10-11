
/*
 * Copyright (c) 1987-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PLATFORM_MODULE_H
#define	_SYS_PLATFORM_MODULE_H

#pragma ident	"@(#)platform_module.h	1.6	99/06/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _KERNEL

/*
 * The functions that are expected of the platform modules.
 */

extern int set_platform_tsb_spares(void);
extern void set_platform_defaults(void);
extern void load_platform_drivers(void);
extern int plat_cpu_poweron(struct cpu *cp);	/* power on CPU */
extern int plat_cpu_poweroff(struct cpu *cp);	/* power off CPU */
extern void plat_freelist_process(int mnode);

/*
 * Data structures expected of the platform modules.
 */
extern char *platform_pm_module_list[];

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_PLATFORM_MODULE_H */
