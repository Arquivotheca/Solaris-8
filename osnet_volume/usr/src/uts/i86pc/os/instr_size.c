/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any 	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)instr_size.c	1.3	97/06/27 SMI"

#include <sys/proc.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/regset.h>
#include <sys/archsystm.h>
#include <sys/copyops.h>
#include <vm/seg_enum.h>

/*
 * instr_size() is the only external object in this file.
 * It is called from trap() to determine the operand size
 * of an instruction that might be incurring a watchpoint trap.
 */

/*
 * This is the structure used for storing all the op code information.
 */
struct instable {
	const struct instable *const indirect;
	const u_char adr_mode;
	const u_char size;	/* operand size, if fixed */
};

/*
 * These are the instruction formats (adr_mode values).
 */
#define	UNKNOWN	0
#define	MRw	2
#define	IMlw	3
#define	IMw	4
#define	IR	5
#define	OA	6
#define	AO	7
#define	MS	8
#define	SM	9
#define	Mv	10
#define	Mw	11
#define	M	12
#define	R	13
#define	RA	14
#define	SEG	15
#define	MR	16
#define	IA	17
#define	MA	18
#define	SD	19
#define	AD	20
#define	SA	21
#define	D	22
#define	INM	23
#define	SO	24
#define	BD	25
#define	I	26
#define	P	27
#define	V	28
#define	DSHIFT	29	/* for double shift that has an 8-bit immediate */
#define	U	30
#define	OVERRIDE 31
#define	GO_ON	32
#define	O	33	/* for call */
#define	JTAB	34	/* jump table */
#define	IMUL	35	/* for 186 iimul instr */
#define	CBW	36	/* so data16 can be evaluated for cbw and variants */
#define	MvI	37	/* for 186 logicals */
#define	ENTER	38	/* for 186 enter instr */
#define	RMw	39	/* for 286 arpl instr */
#define	Ib	40	/* for push immediate byte */
#define	F	41	/* for 287 instructions */
#define	FF	42	/* for 287 instructions */
#define	DM	43	/* 16-bit data */
#define	AM	44	/* 16-bit addr */
#define	LSEG	45	/* for 3-bit seg reg encoding */
#define	MIb	46	/* for 386 logicals */
#define	SREG	47	/* for 386 special registers */
#define	PREFIX	48	/* an instruction prefix like REP, LOCK */
#define	INT3	49	/* The int 3 instruction, which has a fake operand */
#define	DSHIFTcl 50	/* for double shift that implicitly uses %cl */
#define	CWD	51	/* so data16 can be evaluated for cwd and variants */
#define	RET	52	/* single immediate 16-bit operand */
#define	MOVZ	53	/* for movs and movz, with different size operands */

/*
 * Register numbers for the i386
 */
#define	ESP_REGNO	4
#define	EBP_REGNO	5

#define	INVALID		{NULL, UNKNOWN, 1}

/*
 *	In 16-bit mode:
 *	This initialized array will be indexed by the 'r/m' and 'mod'
 *	fields, to determine the size of the displacement in each mode.
 */
static const char dispsize16[8][4] = {
/* mod		00	01	10	11 */
/* r/m */
/* 000 */	0,	1,	2,	0,
/* 001 */	0,	1,	2,	0,
/* 010 */	0,	1,	2,	0,
/* 011 */	0,	1,	2,	0,
/* 100 */	0,	1,	2,	0,
/* 101 */	0,	1,	2,	0,
/* 110 */	2,	1,	2,	0,
/* 111 */	0,	1,	2,	0
};


/*
 *	In 32-bit mode:
 *	This initialized array will be indexed by the 'r/m' and 'mod'
 *	fields, to determine the size of the displacement in this mode.
 */
static const char dispsize32[8][4] = {
/* mod		00	01	10	11 */
/* r/m */
/* 000 */	0,	1,	4,	0,
/* 001 */	0,	1,	4,	0,
/* 010 */	0,	1,	4,	0,
/* 011 */	0,	1,	4,	0,
/* 100 */	0,	1,	4,	0,
/* 101 */	4,	1,	4,	0,
/* 110 */	0,	1,	4,	0,
/* 111 */	0,	1,	4,	0
};


/*
 *	Decode table for 0x0F00 opcodes
 */
static const struct instable op0F00[8] = {
/* [0] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
/* [4] */	{NULL, M, 0},		{NULL, M, 0},
		INVALID,		INVALID,
};


/*
 *	Decode table for 0x0F01 opcodes
 */
static const struct instable op0F01[8] = {
/* [0] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
/* [4] */	{NULL, M, 0},		INVALID,
		{NULL, M, 0},		{NULL, M, 0},
};


/*
 *	Decode table for 0x0FBA opcodes
 */
static const struct instable op0FBA[8] = {
/* [0] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [4] */	{NULL, MIb, 0},		{NULL, MIb, 0},
		{NULL, MIb, 0},		{NULL, MIb, 0},
};

/*
 *	Decode table for 0x0FC8 opcode -- 486 bswap instruction
 *
 * bit pattern: 0000 1111 1100 1reg
 */
static const struct instable op0FC8[4] = {
/* [0] */	{NULL, R, 0},		INVALID,
		INVALID,		INVALID,
};

/*
 *	Decode table for 0x0F opcodes
 */
static const struct instable op0F[13][16] = {
{
/* [00] */	{op0F00, 0, 0},		{op0F01, 0, 0},
		{NULL, MR, 0},		{NULL, MR, 0},
/* [04] */	INVALID,		INVALID,
		{NULL, GO_ON, 0},	INVALID,
/* [08] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		INVALID,		INVALID,
/* [0C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [10] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [14] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [18] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [1C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [20] */	{NULL, SREG, 0},	{NULL, SREG, 0},
		{NULL, SREG, 0},	{NULL, SREG, 0},
/* [24] */	{NULL, SREG, 0},	INVALID,
		{NULL, SREG, 0},	INVALID,
/* [28] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [2C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [30] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [34] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [38] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [3C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [40] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [44] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [48] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [4C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [50] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [54] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [58] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [5C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [60] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [64] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [68] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [6C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [70] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [74] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [78] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [7C] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [80] */	{NULL, D, 0},		{NULL, D, 0},
		{NULL, D, 0},		{NULL, D, 0},
/* [84] */	{NULL, D, 0},		{NULL, D, 0},
		{NULL, D, 0},		{NULL, D, 0},
/* [88] */	{NULL, D, 0},		{NULL, D, 0},
		{NULL, D, 0},		{NULL, D, 0},
/* [8C] */	{NULL, D, 0},		{NULL, D, 0},
		{NULL, D, 0},		{NULL, D, 0},
}, {
/* [90] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
/* [94] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
/* [98] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
/* [9C] */	{NULL, M, 0},		{NULL, M, 0},
		{NULL, M, 0},		{NULL, M, 0},
}, {
/* [A0] */	{NULL, LSEG, 0},	{NULL, LSEG, 0},
		INVALID,		{NULL, RMw, 0},
/* [A4] */	{NULL, DSHIFT, 0},	{NULL, DSHIFTcl, 0},
		INVALID,		INVALID,
/* [A8] */	{NULL, LSEG, 0},	{NULL, LSEG, 0},
		INVALID,		{NULL, RMw, 0},
/* [AC] */	{NULL, DSHIFT, 0},	{NULL, DSHIFTcl, 0},
		INVALID,		{NULL, MRw, 0},
}, {
/* [B0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MR, 0},		{NULL, RMw, 0},
/* [B4] */	{NULL, MR, 0},		{NULL, MR, 0},
		{NULL, MOVZ, 0},	{NULL, MOVZ, 0},
/* [B8] */	INVALID,		INVALID,
		{op0FBA, 0, 0},		{NULL, RMw, 0},
/* [BC] */	{NULL, MRw, 0},		{NULL, MRw, 0},
		{NULL, MOVZ, 0},	{NULL, MOVZ, 0},
}, {
/* [C0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		INVALID,		INVALID,
/* [C4] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [C8] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [CC] */	INVALID,		INVALID,
		INVALID,		INVALID
} };


/*
 *	Decode table for 0x80 opcodes
 */
static const struct instable op80[8] = {
/* [0] */	{NULL, IMlw, 0},	{NULL, IMw, 0},
		{NULL, IMlw, 0},	{NULL, IMlw, 0},
/* [4] */	{NULL, IMw, 0},		{NULL, IMlw, 0},
		{NULL, IMw, 0},		{NULL, IMlw, 0},
};


/*
 *	Decode table for 0x81 opcodes.
 */
static const struct instable op81[8] = {
/* [0] */	{NULL, IMlw, 0},	{NULL, IMw, 0},
		{NULL, IMlw, 0},	{NULL, IMlw, 0},
/* [4] */	{NULL, IMw, 0},		{NULL, IMlw, 0},
		{NULL, IMw, 0},		{NULL, IMlw, 0},
};


/*
 *	Decode table for 0x82 opcodes.
 */
static const struct instable op82[8] = {
/* [0] */	{NULL, IMlw, 0},	INVALID,
		{NULL, IMlw, 0},	{NULL, IMlw, 0},
/* [4] */	INVALID,		{NULL, IMlw, 0},
		INVALID,		{NULL, IMlw, 0},
};

/*
 *	Decode table for 0x83 opcodes.
 */
static const struct instable op83[8] = {
/* [0] */	{NULL, IMlw, 0},	INVALID,
		{NULL, IMlw, 0},	{NULL, IMlw, 0},
/* [4] */	INVALID,		{NULL, IMlw, 0},
		INVALID,		{NULL, IMlw, 0},
};

/*
 *	Decode table for 0xC0 opcodes.
 */
static const struct instable opC0[8] = {
/* [0] */	{NULL, MvI, 0},		{NULL, MvI, 0},
		{NULL, MvI, 0},		{NULL, MvI, 0},
/* [4] */	{NULL, MvI, 0},		{NULL, MvI, 0},
		INVALID,		{NULL, MvI, 0},
};

/*
 *	Decode table for 0xD0 opcodes.
 */
static const struct instable opD0[8] = {
/* [0] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		{NULL, Mv, 0},		{NULL, Mv, 0},
/* [4] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		INVALID,		{NULL, Mv, 0},
};

/*
 *	Decode table for 0xC1 opcodes.
 *	186 instruction set
 */
static const struct instable opC1[8] = {
/* [0] */	{NULL, MvI, 0},		{NULL, MvI, 0},
		{NULL, MvI, 0},		{NULL, MvI, 0},
/* [4] */	{NULL, MvI, 0},		{NULL, MvI, 0},
		INVALID,		{NULL, MvI, 0},
};

/*
 *	Decode table for 0xD1 opcodes.
 */
static const struct instable opD1[8] = {
/* [0] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		{NULL, Mv, 0},		{NULL, Mv, 0},
/* [4] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		INVALID,		{NULL, Mv, 0},
};


/*
 *	Decode table for 0xD2 opcodes.
 */
static const struct instable opD2[8] = {
/* [0] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		{NULL, Mv, 0},		{NULL, Mv, 0},
/* [4] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		INVALID,		{NULL, Mv, 0},
};

/*
 *	Decode table for 0xD3 opcodes.
 */
static const struct instable opD3[8] = {
/* [0] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		{NULL, Mv, 0},		{NULL, Mv, 0},
/* [4] */	{NULL, Mv, 0},		{NULL, Mv, 0},
		INVALID,		{NULL, Mv, 0},
};

/*
 *	Decode table for 0xF6 opcodes.
 */
static const struct instable opF6[8] = {
/* [0] */	{NULL, IMw, 0},		INVALID,
		{NULL, Mw, 0},		{NULL, Mw, 0},
/* [4] */	{NULL, MA, 0},		{NULL, MA, 0},
		{NULL, MA, 0},		{NULL, MA, 0},
};

/*
 *	Decode table for 0xF7 opcodes.
 */
static const struct instable opF7[8] = {
/* [0] */	{NULL, IMw, 0},		INVALID,
		{NULL, Mw, 0},		{NULL, Mw, 0},
/* [4] */	{NULL, MA, 0},		{NULL, MA, 0},
		{NULL, MA, 0},		{NULL, MA, 0},
};

/*
 *	Decode table for 0xFE opcodes.
 */
static const struct instable opFE[8] = {
/* [0] */	{NULL, Mw, 0},		{NULL, Mw, 0},
		INVALID,		INVALID,
/* [4] */	INVALID,		INVALID,
		INVALID,		INVALID,
};

/*
 *	Decode table for 0xFF opcodes.
 */
static const struct instable opFF[8] = {
/* [0] */	{NULL, Mw, 0},		{NULL, Mw, 0},
		{NULL, INM, 0},		{NULL, INM, 0},
/* [4] */	{NULL, INM, 0},		{NULL, INM, 0},
		{NULL, M, 0},		INVALID,
};

/*
 *	Decode tables for 287 instructions, which are a mess to decode
 */
static const struct instable opFP1n2[8][8] = {
/* bit pattern:	1101 1xxx MODxx xR/M */
{
/* [0, 0] */	{NULL, M, 4},		{NULL, M, 4},
		{NULL, M, 4},		{NULL, M, 4},
/* [0, 4] */	{NULL, M, 4},		{NULL, M, 4},
		{NULL, M, 4},		{NULL, M, 4},
}, {
/* [1, 0] */	{NULL, M, 4},		INVALID,
		{NULL, M, 4},		{NULL, M, 4},
/* [1, 4] */	{NULL, M, 28},		{NULL, M, 2},
		{NULL, M, 28},		{NULL, M, 2},
}, {
/* [2, 0] */	{NULL, M, 4},		{NULL, M, 4},
		{NULL, M, 4},		{NULL, M, 4},
/* [2, 4] */	{NULL, M, 4},		{NULL, M, 4},
		{NULL, M, 4},		{NULL, M, 4},
}, {
/* [3, 0] */	{NULL, M, 4},		INVALID,
		{NULL, M, 4},		{NULL, M, 4},
/* [3, 4] */	INVALID,		{NULL, M, 10},
		INVALID,		{NULL, M, 10},
}, {
/* [4, 0] */	{NULL, M, 8},		{NULL, M, 8},
		{NULL, M, 8},		{NULL, M, 8},
/* [4, 4] */	{NULL, M, 8},		{NULL, M, 8},
		{NULL, M, 8},		{NULL, M, 8},
}, {
/* [5, 0] */	{NULL, M, 8},		INVALID,
		{NULL, M, 8},		{NULL, M, 8},
/* [5, 4] */	{NULL, M, 108},		INVALID,
		{NULL, M, 108},		{NULL, M, 2},
}, {
/* [6, 0] */	{NULL, M, 2},		{NULL, M, 2},
		{NULL, M, 2},		{NULL, M, 2},
/* [6, 4] */	{NULL, M, 2},		{NULL, M, 2},
		{NULL, M, 2},		{NULL, M, 2},
}, {
/* [7, 0] */	{NULL, M, 2},		INVALID,
		{NULL, M, 2},		{NULL, M, 2},
/* [7, 4] */	{NULL, M, 10},		{NULL, M, 8},
		{NULL, M, 10},		{NULL, M, 8},
} };

static const struct instable opFP3[8][8] = {
/* bit pattern:	1101 1xxx 11xx xREG */
{
/* [0, 0] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, F, 4},		{NULL, F, 4},
/* [0, 4] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, FF, 4},		{NULL, FF, 4},
}, {
/* [1, 0] */	{NULL, F, 4},		{NULL, F, 4},
		{NULL, GO_ON, 0},	{NULL, F, 4},
/* [1, 4] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [2, 0] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [2, 4] */	INVALID,		{NULL, GO_ON, 0},
		INVALID,		INVALID,
}, {
/* [3, 0] */	INVALID,		INVALID,
		INVALID,		INVALID,
/* [3, 4] */	INVALID,		INVALID,
		INVALID,		INVALID,
}, {
/* [4, 0] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, F, 4},		{NULL, F, 4},
/* [4, 4] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, FF, 4},		{NULL, FF, 4},
}, {
/* [5, 0] */	{NULL, F, 4},		{NULL, F, 4},
		{NULL, F, 4},		{NULL, F, 4},
/* [5, 4] */	{NULL, F, 4},		{NULL, F, 4},
		INVALID,		INVALID,
}, {
/* [6, 0] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, F, 4},		{NULL, GO_ON, 0},
/* [6, 4] */	{NULL, FF, 4},		{NULL, FF, 4},
		{NULL, FF, 4},		{NULL, FF, 4},
}, {
/* [7, 0] */	{NULL, F, 4},		{NULL, F, 4},
		{NULL, F, 4},		{NULL, F, 4},
/* [7, 4] */	{NULL, M, 0},		INVALID,
		INVALID,		INVALID,
} };

static const struct instable opFP4[4][8] = {
/* bit pattern:	1101 1001 111x xxxx */
{
/* [0, 0] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		INVALID,		INVALID,
/* [0, 4] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		INVALID,		INVALID,
}, {
/* [1, 0] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/* [1, 4] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	INVALID,
}, {
/* [2, 0] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/* [2, 4] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
}, {
/* [3, 0] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/* [3, 4] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
} };

static const struct instable opFP5[8] = {
/* bit pattern:	1101 1011 1110 0xxx */
/* [0] */	INVALID,		INVALID,
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/* [4] */	{NULL, GO_ON, 0},	INVALID,
		INVALID,		INVALID,
};

/*
 *	Main decode table for the op codes.  The first two nibbles
 *	will be used as an index into the table.  If there is a
 *	a need to further decode an instruction, the array to be
 *	referenced is indicated with the other two entries being
 *	empty.
 */

static const struct instable distable[16][16] = {
{
/* [0, 0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [0, 4] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, SEG, 0},		{NULL, SEG, 0},
/* [0, 8] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [0, C] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, SEG, 0},		{op0F[0], 0, 0},
}, {
/* [1, 0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [1, 4] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, SEG, 0},		{NULL, SEG, 0},
/* [1, 8] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [1, C] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, SEG, 0},		{NULL, SEG, 0},
}, {
/* [2, 0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [2, 4] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, OVERRIDE, 1},	{NULL, GO_ON, 0},
/* [2, 8] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [2, C] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, OVERRIDE, 1},	{NULL, GO_ON, 0},
}, {
/* [3, 0] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [3, 4] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, OVERRIDE, 1},	{NULL, GO_ON, 0},
/* [3, 8] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [3, C] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, OVERRIDE, 1},	{NULL, GO_ON, 0},
}, {
/* [4, 0] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [4, 4] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [4, 8] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [4, C] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
}, {
/* [5, 0] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [5, 4] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [5, 8] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
/* [5, C] */	{NULL, R, 0},		{NULL, R, 0},
		{NULL, R, 0},		{NULL, R, 0},
}, {
/* [6, 0] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, MR, 0},		{NULL, RMw, 0},
/* [6, 4] */	{NULL, OVERRIDE, 1},	{NULL, OVERRIDE, 1},
		{NULL, DM, 1},		{NULL, AM, 1},
/* [6, 8] */	{NULL, I, 0},		{NULL, IMUL, 0},
		{NULL, Ib, 0},		{NULL, IMUL, 0},
/* [6, C] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
}, {
/* [7, 0] */	{NULL, BD, 1},		{NULL, BD, 1},
		{NULL, BD, 1},		{NULL, BD, 1},
/* [7, 4] */	{NULL, BD, 1},		{NULL, BD, 1},
		{NULL, BD, 1},		{NULL, BD, 1},
/* [7, 8] */	{NULL, BD, 1},		{NULL, BD, 1},
		{NULL, BD, 1},		{NULL, BD, 1},
/* [7, C] */	{NULL, BD, 1},		{NULL, BD, 1},
		{NULL, BD, 1},		{NULL, BD, 1},
}, {
/* [8, 0] */	{op80, 0, 0},		{op81, 0, 0},
		{op82, 0, 0},		{op83, 0, 0},
/* [8, 4] */	{NULL, MRw, 0},		{NULL, MRw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [8, 8] */	{NULL, RMw, 0},		{NULL, RMw, 0},
		{NULL, MRw, 0},		{NULL, MRw, 0},
/* [8, C] */	{NULL, SM, 0},		{NULL, MR, 0},
		{NULL, MS, 0},		{NULL, M, 0},
}, {
/* [9, 0] */	{NULL, GO_ON, 0},	{NULL, RA, 0},
		{NULL, RA, 0},		{NULL, RA, 0},
/* [9, 4] */	{NULL, RA, 0},		{NULL, RA, 0},
		{NULL, RA, 0},		{NULL, RA, 0},
/* [9, 8] */	{NULL, CBW, 0},		{NULL, CWD, 0},
		{NULL, SO, 0},		{NULL, GO_ON, 0},
/* [9, C] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
}, {
/* [A, 0] */	{NULL, OA, 0},		{NULL, OA, 0},
		{NULL, AO, 0},		{NULL, AO, 0},
/* [A, 4] */	{NULL, SD, 1},		{NULL, SD, 0},
		{NULL, SD, 1},		{NULL, SD, 0},
/* [A, 8] */	{NULL, IA, 0},		{NULL, IA, 0},
		{NULL, AD, 0},		{NULL, AD, 0},
/* [A, C] */	{NULL, SA, 0},		{NULL, SA, 0},
		{NULL, AD, 0},		{NULL, AD, 0},
}, {
/* [B, 0] */	{NULL, IR, 0},		{NULL, IR, 0},
		{NULL, IR, 0},		{NULL, IR, 0},
/* [B, 4] */	{NULL, IR, 0},		{NULL, IR, 0},
		{NULL, IR, 0},		{NULL, IR, 0},
/* [B, 8] */	{NULL, IR, 0},		{NULL, IR, 0},
		{NULL, IR, 0},		{NULL, IR, 0},
/* [B, C] */	{NULL, IR, 0},		{NULL, IR, 0},
		{NULL, IR, 0},		{NULL, IR, 0},
}, {
/* [C, 0] */	{opC0, 0, 0},		{opC1, 0, 0},
		{NULL, RET, 0},		{NULL, GO_ON, 0},
/* [C, 4] */	{NULL, MR, 0},		{NULL, MR, 0},
		{NULL, IMw, 0},		{NULL, IMw, 0},
/* [C, 8] */	{NULL, ENTER, 0},	{NULL, GO_ON, 0},
		{NULL, RET, 0},		{NULL, GO_ON, 0},
/* [C, C] */	{NULL, INT3, 1},	{NULL, Ib, 1},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
}, {
/* [D, 0] */	{opD0, 0, 0},		{opD1, 0, 0},
		{opD2, 0, 0},		{opD3, 0, 0},
/* [D, 4] */	{NULL, U, 1},		{NULL, U, 1},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/*
 * 287 instructions.  Note that although the indirect field
 * indicates opFP1n2 for further decoding, this is not necessarily
 * the case since the opFP arrays are not partitioned according to key1
 * and key2.  opFP1n2 is given only to indicate that we haven't
 * finished decoding the instruction.
 */
/* [D, 8] */	{opFP1n2[0], 0, 0},	{opFP1n2[0], 0, 0},
		{opFP1n2[0], 0, 0},	{opFP1n2[0], 0, 0},
/* [D, C] */	{opFP1n2[0], 0, 0},	{opFP1n2[0], 0, 0},
		{opFP1n2[0], 0, 0},	{opFP1n2[0], 0, 0},
}, {
/* [E, 0] */	{NULL, BD, 1},		{NULL, BD, 1},
		{NULL, BD, 1},		{NULL, BD, 1},
/* [E, 4] */	{NULL, P, 1},		{NULL, P, 0},
		{NULL, P, 1},		{NULL, P, 0},
/* [E, 8] */	{NULL, D, 0},		{NULL, D, 0},
		{NULL, SO, 0},		{NULL, BD, 1},
/* [E, C] */	{NULL, V, 1},		{NULL, V, 0},
		{NULL, V, 1},		{NULL, V, 0},
}, {
/* [F, 0] */	{NULL, PREFIX, 1},	{NULL, JTAB, 1},
		{NULL, PREFIX, 1},	{NULL, PREFIX, 1},
/* [F, 4] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{opF6, 0, 0},		{opF7, 0, 0},
/* [F, 8] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
/* [F, C] */	{NULL, GO_ON, 0},	{NULL, GO_ON, 0},
		{opFE, 0, 0},		{opFF, 0, 0},
} };


#define	WBIT(x)		(x & 0x1)		/* to get w bit */
#define	OPSIZE(data16, wbit)	((wbit)? ((data16) ? 2 : 4) : 1)

#define	REG_ONLY	3	/* mode indicates a single register with */
				/* no displacement is an operand */
#define	LONGOPERAND	1	/* value of the w-bit indicating a long */
				/* operand (2-bytes or 4-bytes) */


/*
 * getbyte() fetched the next byte from the instruction.
 * getbytes(n) reads and discards n from the instruction.
 * displacement(n) reads an n-byte displacement from the instruction.
 * imm_data(n) reads n bytes of immediate data from the instruction.
 */
#define	getbyte()	((u_int)(*ip++))
#define	getbytes(n)	((void)(ip += (n)))
#define	displacement(n)	getbytes(n)
#define	imm_data(n)	getbytes(n)

/*
 *	void get_modrm_byte (byte, mode, reg, r_m)
 *
 *	Get the byte following the op code and separate it into the
 *	mode, register, and r/m fields.
 * Scale-Index-Bytes have a similar format.
 */
static void
get_modrm_byte(u_short modrmbyte, u_int *mode, u_int *reg, u_int *r_m)
{
	*r_m = modrmbyte & 0x7;
	*reg = (modrmbyte >> 3) & 0x7;
	*mode = (modrmbyte >> 6) & 0x3;
}


static void
get_operand(u_char **ipp, u_int mode, u_int r_m, int addr16)
{
	u_char *ip = *ipp;
	int dispsize;	/* size of displacement in bytes */
	int s_i_b = 0;	/* flag presence of scale-index-byte */
	u_int ss;	/* scale-factor from opcode */
	u_int index;	/* index register number */
	u_int base;	/* base register number */

	/* check for the presence of the s-i-b byte */
	if (r_m == ESP_REGNO && mode != REG_ONLY && !addr16) {
		s_i_b = 1;
		get_modrm_byte(getbyte(), &ss, &index, &base);
	}

	if (addr16)
		dispsize = dispsize16[r_m][mode];
	else
		dispsize = dispsize32[r_m][mode];

	if (s_i_b && mode == 0 && base == EBP_REGNO)
		dispsize = 4;

	if (dispsize != 0)
		displacement(dispsize);

	*ipp = ip;
}



/*
 * get_opcode (opcode, high, low)
 * Separate the op code into the high and low nibbles.
 */
static void
get_opcode(u_short opcode, u_int *high, u_int *low)
{
	*low = opcode & 0xf;
	*high = (opcode >> 4) & 0xf;
}


/*
 * instr_size() is the only external object in this file.
 *
 * Return the operand size of an instruction (1, 2, 4, 8, ...).
 * If rw == S_EXEC we must disassemble completely
 * to determine the size of the instruction.
 * Return 0 on failure.
 */
/* ARGSUSED */
int
instr_size(struct regs *rp, caddr_t *addrp, enum seg_rw rw)
{
	int sz;
	const struct instable *dp;
	int wbit = 0;
	u_int mode, reg, r_m;
	/* nibbles of the opcode */
	u_int opcode1, opcode2, opcode3, opcode4, opcode5;
	int got_modrm_byte;
	/* number of bytes of opcode - used to get wbit */
	int opcode_bytes = 0;
	int data16 = 0;		/* 16- or 32-bit data */
	int addr16 = 0;		/* 16- or 32-bit addressing */
	u_char instr[32];	/* maximum size instruction */
	u_char *ip = instr;
	caddr_t pc = (caddr_t)rp->r_eip;
	int mapped = 0;

	if (curproc->p_warea)
		mapped = pr_mappage(pc, sizeof (instr), S_READ, 1);
	(void) default_copyin(pc, (caddr_t)instr, sizeof (instr));
	if (mapped)
		pr_unmappage(pc, sizeof (instr), S_READ, 1);

	/*
	 * As long as there is a prefix, the default segment register,
	 * addressing-mode, or data-mode in the instruction will be overridden.
	 * This may be more general than the chip actually is.
	 */
	for (;;) {
		get_opcode(getbyte(), &opcode1, &opcode2);
		dp = &distable[opcode1][opcode2];

		switch (dp->adr_mode) {
		case PREFIX:
		case OVERRIDE:
			continue;
		case AM:
			addr16 = !addr16;
			continue;
		case DM:
			data16 = !data16;
			continue;
		}
		break;
	}

	/*
	 * Some 386 instructions have 2 bytes of opcode before the
	 * mod_r/m byte so we need to perform a table indirection.
	 */
	if (dp->indirect == op0F[0]) {
		get_opcode(getbyte(), &opcode4, &opcode5);
		if (opcode4 > 12)	/* maximum valid opcode */
			goto out;
		if (opcode4 == 0xc && opcode5 >= 0x8)
			dp = &op0FC8[0];
		else
			dp = &op0F[opcode4][opcode5];
		opcode_bytes = 2;
	}

	got_modrm_byte = 0;
	if (dp->indirect != NULL) {
		/*
		 * This must have been an opcode for which several
		 * instructions exist.  The opcode3 field further decodes
		 * the instruction.
		 */
		got_modrm_byte = 1;
		get_modrm_byte(getbyte(), &mode, &opcode3, &r_m);

		/*
		 * decode 287 instructions (D8-DF) from opcodeN
		 */
		if (opcode1 == 0xD && opcode2 >= 0x8) {
			/* instruction form 5 */
			if (opcode2 == 0xB && mode == 0x3 && opcode3 == 4)
				dp = &opFP5[r_m];
			else if (opcode2 == 0xB && mode == 0x3 && opcode3 > 4)
				goto out;
			/* instruction form 4 */
			else if (opcode2 == 0x9 && mode == 0x3 && opcode3 >= 4)
				dp = &opFP4[opcode3-4][r_m];
			/* instruction form 3 */
			else if (mode == 0x3)
				dp = &opFP3[opcode2-8][opcode3];
			/* instruction form 1 and 2 */
			else
				dp = &opFP1n2[opcode2-8][opcode3];
		} else {
			dp = dp->indirect + opcode3;
		}
		/* now dp points the proper subdecode table entry */
	}

	if (dp->indirect != NULL) {
		cmn_err(CE_WARN,
		    "instr_size(): instruction table indirect entry not NULL");
		goto out;
	}

	switch (dp->adr_mode) {

	/*
	 * movsbl movsbw (0x0FBE) or movswl (0x0FBF)
	 * movzbl movzbw (0x0FB6) or movzwl (0x0FB7)
	 * wbit lives in 2nd byte, note that operands are different sized
	 */
	case MOVZ:
		/* Get second operand first so data16 can be destroyed */
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		wbit = WBIT(opcode5);
		data16 = 1;
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* imul instruction, with either 8-bit or longer immediate */
	case IMUL:
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		/*
		 * opcode 0x6B for byte, sign-extended displacement,
		 * 0x69 for word(s)
		 */
		imm_data(OPSIZE(data16, opcode2 == 0x9));
		wbit = LONGOPERAND;
		break;

	/* memory or register operand to register, with 'w' bit */
	case MRw:
		wbit = WBIT(opcode2);
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* register to memory or register operand, with 'w' bit */
	/* arpl happens to fit here also because it is odd */
	case RMw:
		if (opcode_bytes == 2)
			wbit = WBIT(opcode5);
		else
			wbit = WBIT(opcode2);
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* Double shift. Has immediate operand specifying the shift. */
	case DSHIFT:
		get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		imm_data(1);
		wbit = LONGOPERAND;
		break;

	/* Double shift. With no immediate operand, specifies using %cl. */
	case DSHIFTcl:
		get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	/* immediate to memory or register operand */
	case IMlw:
		wbit = WBIT(opcode2);
		get_operand(&ip, mode, r_m, addr16);
		/*
		 * A long immediate is expected for opcode 0x81,
		 * not 0x80 nor 0x83
		 */
		imm_data(OPSIZE(data16, opcode2 == 1));
		break;

	/* immediate to memory or register operand with the */
	/* 'w' bit present */
	case IMw:
		wbit = WBIT(opcode2);
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		imm_data(OPSIZE(data16, wbit));
		break;

	/* immediate to register with register in low 3 bits */
	/* of op code */
	case IR:
		/* w-bit here (with regs) is bit 3 */
		wbit = (opcode2 >> 3) & 0x1;
		imm_data(OPSIZE(data16, wbit));
		break;

	/* memory operand to accumulator */
	case OA:
		wbit = WBIT(opcode2);
		displacement(OPSIZE(addr16, LONGOPERAND));
		break;

	/* accumulator to memory operand */
	case AO:
		wbit = WBIT(opcode2);
		displacement(OPSIZE(addr16, LONGOPERAND));
		break;

	/* memory or register operand to segment register */
	case MS:
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	/* segment register to memory or register operand */
	case SM:
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	/* rotate or shift instrutions, which may shift by 1 or */
	/* consult the cl register, depending on the 'v' bit */
	case Mv:
		wbit = WBIT(opcode2);
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* immediate rotate or shift instrutions, which may or */
	/* may not consult the cl register, depending on the 'v' bit */
	case MvI:
		wbit = WBIT(opcode2);
		get_operand(&ip, mode, r_m, addr16);
		imm_data(1);
		break;

	case MIb:
		get_operand(&ip, mode, r_m, addr16);
		imm_data(1);
		wbit = LONGOPERAND;
		break;

	/* single memory or register operand with 'w' bit present */
	case Mw:
		wbit = WBIT(opcode2);
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* single memory or register operand */
	case M:
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	case SREG: /* special register */
		get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		wbit = LONGOPERAND;
		break;

	/* single reg operand with reg in the low 3 bits of op code */
	case R:
	/* reg to accumulator with reg in the low 3 bits of op code */
	/* xchg instructions */
	case RA:
	/* single segment reg operand, with reg in bits 3-4 of op code */
	case SEG:
	/* single segment reg operand, with reg in bits 3-5 of op code */
	case LSEG:
		wbit = LONGOPERAND;
		break;

	/* memory or register operand to register */
	case MR:
		if (!got_modrm_byte)
			get_modrm_byte(getbyte(), &mode, &reg, &r_m);
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	/* immediate operand to accumulator */
	case IA:
		wbit = WBIT(opcode2);
		imm_data(OPSIZE(data16, wbit));
		break;

	/* memory or register operand to accumulator */
	case MA:
		wbit = WBIT(opcode2);
		get_operand(&ip, mode, r_m, addr16);
		break;

	/* si register to di register */
	case SD:
		wbit = LONGOPERAND;
		break;

	/* accumulator to di register */
	case AD:
	/* si register to accumulator */
	case SA:
		wbit = WBIT(opcode2);
		break;

	/* single operand, a 16/32 bit displacement */
	/* added to current offset by 'compoff' */
	case D:
		displacement(OPSIZE(data16, LONGOPERAND));
		wbit = LONGOPERAND;
		break;

	/* indirect to memory or register operand */
	case INM:
		get_operand(&ip, mode, r_m, addr16);
		wbit = LONGOPERAND;
		break;

	/*
	 * for long jumps and long calls -- a new code segment
	 * register and an offset in IP -- stored in object
	 * code in reverse order
	 */
	case SO:
		displacement(OPSIZE(addr16, LONGOPERAND));
		/* will now get segment operand */
		displacement(2);
		wbit = LONGOPERAND;
		break;

	/* jmp/call. single operand, 8 bit displacement. */
	/* added to current EIP in 'compoff' */
	case BD:
		displacement(1);
		break;

	/* single 32/16 bit immediate operand */
	case I:
		imm_data(OPSIZE(data16, LONGOPERAND));
		wbit = LONGOPERAND;
		break;

	/* single 8 bit immediate operand */
	case Ib:
		imm_data(1);
		wbit = LONGOPERAND;
		break;

	case ENTER:
		imm_data(2);
		imm_data(1);
		wbit = LONGOPERAND;
		break;

	/* 16-bit immediate operand */
	case RET:
		imm_data(2);
		wbit = LONGOPERAND;
		break;

	/* single 8 bit port operand */
	case P:
		imm_data(1);
		wbit = LONGOPERAND;
		break;

	/* single operand, dx register (variable port instruction) */
	case V:
		wbit = LONGOPERAND;
		break;

	/* The int instruction, which has two forms: int 3 (breakpoint) or */
	/* int n, where n is indicated in the subsequent byte (format Ib). */
	/* The int 3 instruction (opcode 0xCC), where, although the 3 looks */
	/* like an operand, it is implied by the opcode. It must be converted */
	/* to the correct base and output. */
	case INT3:
		break;

	/* an unused byte must be discarded */
	case U:
		(void) getbyte();
		break;

	case CBW:
	case CWD:
	case GO_ON:
		wbit = LONGOPERAND;
		break;

	/* Special byte indicating a the beginning of a */
	/* jump table has been seen. We should never see this. */
	case JTAB:
		break;

	/* float reg */
	case F:
	/* float reg to float reg, with ret bit present */
	case FF:
		break;

	/* an invalid op code */
	case AM:
	case DM:
	case OVERRIDE:
	case PREFIX:
	case UNKNOWN:
		break;

	default:
		cmn_err(CE_WARN,
			"instr_size(): case from instruction table not found");
		break;

	}

out:
	if (rw == S_EXEC)		/* length of the instruction inself */
		sz = ip - instr;
	else if ((sz = dp->size) == 0)		/* operand size not known? */
		sz = OPSIZE(data16, wbit);		/* compute it */

	return (sz);
}
