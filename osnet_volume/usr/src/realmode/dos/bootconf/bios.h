/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * bios.h - bios definitions
 */

#ifndef	_BIOS_H
#define	_BIOS_H

#ident "@(#)bios.h   1.7   98/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public pnpbios function prototypes
 */
void enumerator_bios();
int init_bioscmos(void);

#define	BIOS_ID 0xaa55
#define	BIOS_MIN 0xc0000000
#define	BIOS_MAX 0xf0000000
#define	BIOS_ALIGN 0x800000 /* 2KB boundaries - in far pointer format */

#define	MK_PHYS(faddr) ((u_long)(((u_long)faddr >> 12) & 0xffff0) + \
	((u_long)faddr & 0xffff))

#define	MK_PTR(seg, off)	((void *)((u_long)(seg) << 16 + (ushort)(off)))

#ifdef	__cplusplus
}
#endif

#endif	/* _BIOS_H */
