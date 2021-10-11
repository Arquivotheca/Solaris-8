/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DR_KERNEL_H
#define	_DR_KERNEL_H

#pragma ident	"@(#)dr_kernel.h	1.7	98/08/19 SMI"

/*
 * This header file defines macros/included files/etc. for dr_kernel.c which,
 * in turn, implements the interface between the DR daemon and the DR driver.
 */

#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <unistd.h>
#include <sys/processor.h>
#include <sys/obpdefs.h>	/* for OBP_NONODE */
#include <sys/dditypes.h>	/* for dev_info_t */
#include <sys/ddipropdefs.h>	/* for DDI_DEV_T_NONE */
#include <utime.h>		/* for utime routines/structs */
#include <dr_subr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The DR_IOCTARG_* are possible targets for a DR ioctl.  Either the whole
 * board, just its memory, just its cpus, or just its i/o boards.
 */
#define	DR_IOCTARG_NONE			(0)
#define	DR_IOCTARG_BRD			(1)
#define	DR_IOCTARG_MEM			(2)
#define	DR_IOCTARG_CPUS			(3)
#define	DR_IOCTARG_IO			(4)
#define	DR_IOC_NTARGS			(5)

/*
 * This macro tests whether or not a given target is valid
 */
#define	TARGET_IN_RANGE(target)		(target > 0 && target < DR_IOC_NTARGS)

#define	DR_DRV_BASE	"/devices/pseudo/dr@0:slot"

#ifdef	__cplusplus
}
#endif

#endif	/* _DR_KERNEL_H */
