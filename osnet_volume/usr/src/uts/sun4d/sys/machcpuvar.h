/*
 * Copyright (c) 1990,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHCPUVAR_H
#define	_SYS_MACHCPUVAR_H

#pragma ident	"@(#)machcpuvar.h	1.18	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine specific fields of the cpu struct
 * defined in common/sys/cpuvar.h.
 */
struct	machcpu {
	struct machpcb	*mpcb;
	uint_t	syncflt_status;
	uint_t	syncflt_addr;
};

#define	CPU_IN_OBP	0x01

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHCPUVAR_H */
