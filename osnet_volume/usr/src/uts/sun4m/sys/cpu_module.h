
/*
 * Copyright (c) 1987-1995 by Sun Microsystems, Inc.
 */

#ifndef _SYS_CPU_MODULE_H
#define	_SYS_CPU_MODULE_H

#pragma ident	"@(#)cpu_module.h	1.1	96/05/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef _KERNEL

/*
 * A cpu module needs to provide a module_info struct
 * and its size.
 */

struct module_linkage module_info[];

int module_info_size;

#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CPU_MODULE_H */
