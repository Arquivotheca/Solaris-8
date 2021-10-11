/*
 * Copyright (c) 1991-1993, by Sun Microsystems, Inc.
 */

#ifndef _SYS_ENABLE_H
#define	_SYS_ENABLE_H

#pragma ident	"@(#)enable.h	1.9	93/05/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The System Enable register controls overall
 * operation of the system.  When the system is
 * reset, the Enable register is cleared.  The
 * enable register is addressed as a byte in
 * ASI_CTL space.
 */

/*
 * Bits of the Enable Register
 */
#define	ENA_SWRESET	0x04		/* r/w - software reset */
#define	ENA_CACHE	0x10		/* r/w - enable external cache */
#define	ENA_SDVMA	0x20		/* r/w - enable system DVMA */
#define	ENA_NOTBOOT	0x80		/* r/w - non-boot state, 1 = normal */

#define	ENABLEREG	0x40000000	/* addr in ASI_CTL space */

#if defined(_KERNEL) && !defined(_ASM)

extern void on_enablereg(unsigned char);
extern void off_enablereg(unsigned char);
extern unsigned char get_enablereg(void);

#endif /* _KERNEL && !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ENABLE_H */
