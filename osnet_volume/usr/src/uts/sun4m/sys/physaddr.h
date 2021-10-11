/*
 * Copyright (c) 1991-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PHYSADDR_H
#define	_SYS_PHYSADDR_H

#pragma ident	"@(#)physaddr.h	1.13	98/01/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Generic Sun-4M physical addresses
 *
 * Don't access physical addresses directly. Either create a well known
 * virtual address and map to it or create a vector for the routine
 * (see subr.s)
 */
#define	PA_DIAGMESG	0x00001000	/* asi=2F: diagnostic message */
#define	PA_MBUS_ARBEN	0xE0001008	/* asi=2F: mbus arbiter enable */
#define	PA_DIAGLED	0xF1600000	/* asi=2F: diagnostic LED */
#define	PA_SYSCTL	0xF1F00000	/* asi=2F: system control/status */

#define	PA_MID		0xE0002000	/* asi=2F: module identification */
#define	PA_INTPEND	0xF1400000	/* asi=2F: get soft/hard int bits */
#define	PA_INTCLR	0xF1400004	/* asi=2F: clear soft/hard int bits */
#define	PA_INTSET	0xF1400008	/* asi=2F: set soft/hard int bits */
#define	PA_SYSINTPEND	0xF1410000	/* asi=2F: get sys ints pending */
#define	PA_SYSINTMASK	0xF1410004	/* asi=2F: get sys int mask */
#define	PA_SYSINTMCLR	0xF1410008	/* asi=2F: clr sys int mask bits */
#define	PA_SYSINTMSET	0xF141000C	/* asi=2F: set sys int mask bits */
#define	PA_INTTARGET	0xF1410010	/* asi=2F: system interrupt target  */
#define	PA_DIAGREG	0xF1600000	/* asi=2F: diagnostic LED */
#define	PA_AUXIO	0xF1800000	/* asi=2F: Aux IO and LED */

#if defined(_KERNEL) && !defined(_ASM)

extern int ldphys(int);
extern int asm_ldphys(int);
extern void stphys(int, int);
extern void scrubphys(int);

#endif /* _KERNEL && !defined(_ASM) */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PHYSADDR_H */
