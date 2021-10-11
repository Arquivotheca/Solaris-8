/*
 * Copyright (c) 1991-1993,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_BT_H
#define	_SYS_BT_H

#pragma ident	"@(#)bt.h	1.4	98/01/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These values are decoded from physical addresses and other information,
 * and represent real and pseudo bus types.
 * They are only used as tokens--they do not relate numerically to any
 * physical reality.
 */

#define	BT_UNKNOWN	1	/* not a defined map area */
#define	BT_DRAM		2	/* system memory as DRAM */
#define	BT_NVRAM	3	/* system memory as NVRAM */
#define	BT_OBIO		4	/* on-board devices */
#define	BT_VIDEO	5	/* onboard video */
#define	BT_SBUS		6	/* S-Bus */

#if defined(_KERNEL) && !defined(_ASM)

extern int impl_bustype(uint_t);
extern int small_sun4m_impl_bustype(uint_t);
extern int sun4m_impl_bustype(uint_t);

#endif /* _KERNEL && !ASM */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_BT_H */
