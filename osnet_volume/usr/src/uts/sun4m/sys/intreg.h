/*
 * Copyright (c) 1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INTREG_H
#define	_SYS_INTREG_H

#pragma ident	"@(#)intreg.h	1.17	98/01/24 SMI"

#include <sys/devaddr.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM

struct cpu_intreg {
	uint_t	pend;
	uint_t	clr_pend;
	uint_t	set_pend;
	uchar_t	filler[0x1000 - 0xc];
};

struct sys_intreg {
	uint_t	sys_pend;
	uint_t	sys_m;
	uint_t	sys_mclear;
	uint_t	sys_mset;
	uint_t	itr;
};

extern struct cpu_intreg *v_interrupt_addr[];
extern struct sys_intreg *v_sipr_addr;

#endif  /* _ASM */

/*
 * Bits of the interrupt registers.
 */
#define	IR_SOFT_SHIFT	0x10		/* soft interrupt in set reg */
#define	IR_CPU_CLEAR	0x4		/* clear pending register for cpu */
#define	IR_CPU_SOFTINT	0x8		/* set soft interrupt for cpu */
#define	IR_MASK_OFFSET	0x4
#define	IR_CLEAR_OFFSET	0x8
#define	IR_SET_OFFSET	0xC
#define	IR_SET_ITR	0x10

#define	SIR_MASKALL	0x80000000	/* mask all interrupts */
#define	SIR_MODERROR	0x40000000	/* module error */
#define	SIR_M2SWRITE	0x20000000	/* m-to-s write buffer error */
#define	SIR_ECCERROR	0x10000000	/* ecc memory error */
#define	SIR_FLOPPY	0x00400000	/* floppy disk */
#define	SIR_MODULEINT	0x00200000	/* module interrupt */
#define	SIR_VIDEO	0x00100000	/* onboard video */
#define	SIR_REALTIME	0x00080000	/* system timer */
#define	SIR_SCSI	0x00040000	/* onboard scsi */
#define	SIR_AUDIO	0x00020000	/* audio/isdn */
#define	SIR_ETHERNET	0x00010000	/* onboard ethernet */
#define	SIR_SERIAL	0x00008000	/* serial ports */
#define	SIR_KBDMS	0x00004000	/* keyboard/mouse */
#define	SIR_SBUSBITS	0x00003F80	/* sbus int bits */
/* asynchronous fault */
#define	SIR_ASYNCFLT	(SIR_ECCERROR+SIR_M2SWRITE+SIR_MODERROR)
#define	SIR_SBUSINT(n)	(0x00000001 << (n+6))
#define	SIR_L1		0
#define	SIR_L2		(SIR_SBUSINT(1))
#define	SIR_L3		(SIR_SBUSINT(2))
#define	SIR_L4		(SIR_SCSI)
#define	SIR_L5		(SIR_SBUSINT(3))
#define	SIR_L6		(SIR_ETHERNET)
#define	SIR_L7		(SIR_SBUSINT(4))
#define	SIR_L8		(SIR_VIDEO)
#define	SIR_L9		(SIR_SBUSINT(5)+SIR_MODULEINT)
#define	SIR_L10		(SIR_REALTIME)
#define	SIR_L11		(SIR_SBUSINT(6)+SIR_FLOPPY)
#define	SIR_L12		(SIR_KBDMS+SIR_SERIAL)
#define	SIR_L13		(SIR_AUDIO)
#define	SIR_L14		0
#define	SIR_L15		(SIR_ASYNCFLT)

#define	IR_ENA_CLK14	0x80		/* r/w - clock level 14 interrupt */
#define	IR_ENA_CLK10	0x80000		/* r/w - clock level 10 interrupt */
#define	IR_ENA_INT	0x80000000	/* r/w - enable (all) interrupts */

#define	IR_HARD_INT(n)	(0x000000001 << (n))
#define	IR_SOFT_INT(n)	(0x000010000 << (n))

#define	IR_SOFT_INT6	IR_SOFT_INT(6)	/* r/w - software level 6 interrupt */
#define	IR_SOFT_INT4	IR_SOFT_INT(4)	/* r/w - software level 4 interrupt */
#define	IR_SOFT_INT1	IR_SOFT_INT(1)	/* r/w - software level 1 interrupt */
#define	IR_HARD_INT15	IR_HARD_INT(15)	/* r/w - hardware level 15 interrupt */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_INTREG_H */
