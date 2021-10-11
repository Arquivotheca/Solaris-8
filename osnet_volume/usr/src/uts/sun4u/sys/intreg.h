/*
 * Copyright (c) 1994,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_INTREG_H
#define	_SYS_INTREG_H

#pragma ident	"@(#)intreg.h	1.15	99/07/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Interrupt Receive Data Registers
 *	ASI_SDB_INTR_R or ASI_INTR_RECEIVE; ASI 0x7F; VA 0x40, 0x50, 0x60
 */
#define	IRDR_0		0x40
#define	IRDR_1		0x50
#define	IRDR_2		0x60

/*
 * Interrupt Receive Status Register
 *	ASI_INTR_RECEIVE_STATUS; ASI 0x49; VA 0x0
 *
 *	|---------------------------------------------------|
 *	|    RESERVED (Read as 0)        | BUSY |   PORTID  |
 *	|--------------------------------|------|-----------|
 *	 63                             6    5   4         0
 *
 */
#define	IRSR_BUSY	0x20	/* set when there's a vector received */
#define	IRSR_PID_MASK	0x1F	/* PORTID bit mask <4:0> */

/*
 * Interrupt Dispatch Data Register
 *	ASI_SDB_INTR_W or ASI_INTR_DISPATCH; ASI 0x77; VA 0x40, 0x50, 0x60
 */
#define	IDDR_0		0x40
#define	IDDR_1		0x50
#define	IDDR_2		0x60

/*
 * Interrupt Dispatch Command Register
 *	ASI_INTR_DISPATCH or ASI_SDB_INTR_W; ASI 0x77; VA = PORTID<<14|0x70
 *
 *	|---------------------------------------------------|
 *	|          0           | PORTID     |  0x70         |
 *	|----------------------|------------|---------------|
 *	 63                     18        14  13           0
 */
#define	IDCR_OFFSET	0x70		/* IDCR VA<13:0> */
#define	IDCR_PID_MASK	0x7C000		/* IDCR VA<18:14> */
#define	IDCR_PID_SHIFT	14

/*
 * Interrupt Dispatch Status Register
 *	ASI_INTR_DISPATCH_STATUS; ASI 0x48; VA 0x0
 *
 *	|---------------------------------------------------|
 *	|     RESERVED (Read as 0)          | NACK  | BUSY  |
 *	|-----------------------------------|-------|-------|
 *	 63                               2    1        0   |
 */
#define	IDSR_NACK	0x2		/* set if interrupt dispatch failed */
#define	IDSR_BUSY	0x1		/* set when there's a dispatch */

/*
 * Interrupt Number Register
 *	Every interrupt source has a register associated with it
 *
 *	|---------------------------------------------------|
 *	|INT_EN |  PORTID  |RESERVED (Read as 0)| INT_NUMBER|
 *	|       |          |                    | IGN | INO |
 *	|-------|----------|--------------------|-----|-----|
 *	|  31    30      26 25                11 10  6 5   0
 */
#define	INR_EN_SHIFT	31
#define	INR_PID_SHIFT	26
#define	INR_PID_MASK	(IRSR_PID_MASK << (INR_PID_SHIFT))
#ifdef	_STARFIRE
/*
 * Starfire interrupt group number is 7 bits
 * Starfire's IGN (inter group #) is not the same as upaid
 */
#define	IGN_SIZE	7		/* Interrupt Group Number bit size */
#define	UPAID_TO_IGN(upaid) ((((upaid & 0x3C) >> 1) | (upaid & 0x1)) |	\
				(((upaid & 0x2) << 4) |			\
				((upaid & 0x40) ^ 0x40)))
#else
#define	IGN_SIZE	5		/* Interrupt Group Number bit size */
#define	UPAID_TO_IGN(upaid) (upaid)
#endif	/* _STARFIRE */
#define	INO_SIZE	6		/* Interrupt Number Offset bit size */
#define	INR_SIZE	(IGN_SIZE + INO_SIZE)	/* Interrupt Number bit size */
#define	MAX_IGN		(1 << IGN_SIZE) /* max Interrupt Group Number size */
#define	MAX_INO		(1 << INO_SIZE) /* max Interrupt Number per group */
#define	MAX_SOFT_INO	256

#define	SOFTIVNUM	(MAX_IGN * MAX_INO)
#define	MAXIVNUM	(MAX_IGN * MAX_INO + MAX_SOFT_INO)

/*
 * Interrupt State Machine
 *	Each interrupt source has a 2-bit state machine which ensures that
 *	software sees exactly one interrupt packet per assertion of the
 *	interrupt signal.
 */
#define	ISM_IDLE	0x0	/* not asserted or pending */
#define	ISM_TRANSMIT	0x1	/* asserted but is not dispatched */
#define	ISM_PENDING	0x2	/* dispatched to a processor or is in transit */

/*
 * Per-Processor Soft Interrupt Register
 * XXX use %asr when the new assembler supports them
 */
#define	SET_SOFTINT	%asr20		/* ASR 0x14 */
#define	CLEAR_SOFTINT	%asr21		/* ASR 0x15 */
#define	SOFTINT		%asr22		/* ASR 0x16 */
#define	SOFTINT_MASK	0xFFFE		/* <15:1> */
#define	TICK_INT_MASK	0x1		/* <0> */
#define	STICK_INT_MASK	0x10000		/* <0> */

/*
 * Per-Processor TICK Register and TICK_Compare registers
 *
 */
#define	TICK_COMPARE	%asr23		/* ASR 0x17 */
#define	STICK		%asr24		/* ASR 0x18 */
#define	STICK_COMPARE	%asr25		/* ASR 0x19 */
#define	TICKINT_DIS_SHFT	0x3f

#ifndef _ASM

/*
 * Interrupt Packet (mondo)
 */
struct	intr_packet {
	u_longlong_t intr_data0; /* can be an interrupt number or a pc */
	u_longlong_t intr_data1;
	u_longlong_t intr_data2;
};

/*
 * Leftover bogus stuff; removed them later
 */
struct cpu_intreg {
	uint_t	pend;
	uint_t   clr_pend;
	uint_t   set_pend;
	uchar_t	filler[0x1000 - 0xc];
};

struct sys_intreg {
	uint_t	sys_pend;
	uint_t	sys_m;
	uint_t	sys_mclear;
	uint_t	sys_mset;
	uint_t	itr;
};

#endif  /* _ASM */

#define	IR_CPU_CLEAR	0x4		/* clear pending register for cpu */
#define	IR_MASK_OFFSET	0x4
#define	IR_SET_ITR	0x10
#define	IR_SOFT_INT(n)	(0x000010000 << (n))
#define	IR_SOFT_INT4	IR_SOFT_INT(4)	/* r/w - software level 4 interrupt */
#define	IR_CPU_SOFTINT	0x8		/* set soft interrupt for cpu */
#define	IR_CLEAR_OFFSET	0x8

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_INTREG_H */
