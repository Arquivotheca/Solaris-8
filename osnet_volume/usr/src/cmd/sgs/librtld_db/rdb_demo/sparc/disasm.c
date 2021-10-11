
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided this notice is preserved and that due credit is given
 * to Sun Microsystems, Inc.  The name of Sun Microsystems, Inc. may
 * not be used to endorse or promote products derived from this
 * software without specific prior written permission.  This software
 * is provided ``as is'' without express or implied warranty.
 */
#pragma ident	"@(#)disasm.c	1.5	98/03/18 SMI"

/*
 * This code has been hijaced from usr/src/cmd/sgs/dis/sparc/*.c
 * and hibridized so that we can build a libdis.so which is callable
 * from other applications.
 */



#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "disasm.h"

/* SPARC disassembler */

/*
 * The programming interface is via dsmInst, which prints a single disassembled
 * instruction, and dsmNbytes, which will tell you how big an instruction is.
 *
 * ADDRESS PRINTING ROUTINE
 * Many of the operands to assembly language instructions are addresses.  In
 * order to allow the ability to print these symbolically, the call to dsmInst
 * has, as a parameter, the address of a routine to call to print an address.
 * When dsmLib needs to print an address as part of an operand field, it will
 * call the supplied routine to do the actual printing.  The routine should be
 * declared as:
 *
 *	prtAddress(buf, address)
 *	char *buf;
 * 	unsigned int address;	/ * address to print * /
 *
 * When called, the routine should print the address on standard out (with
 * printf or whatever) in whatever form, numeric or symbolic, that it desires.
 *
 * If the prtAddress argument to dsmPrint is NULL, a default print routine
 * is used, which just prints the address as a hex number.
 *
 * DEFICIENCIES
 * SETHI now has the pretty lookahead that gives it accurate values for %hi()
 * in most cases; however, an occasional case will probably confuse it.
 * The same applies to the corresponding %lo lookback.
 */


#define	OP(x)		((unsigned)(0x3 & x) << 30)	/* general opcode */
#define	OPMSK		0xC0000000
#define	OP2(x)		((0x7 & x) << 22)	/* op2 opcode */
#define	OP2MSK		0x01C00000
#define	OP3(x)		((0x3f & x) << 19)	/* op3 opcode */
#define	OP3MSK		0x01F80000
#define	OPFC(x)		((0x1ff & x) << 5)	/* FP or CP opcode */
#define	OPFCMSK		0x00003FE0
#define	RD(x)		((0x1f & x) << 25) 	/* destination register */
#define	RDMSK		0x3E000000
#define	RS1(x)		((0x1f & x) << 14)	/* source register 1 */
#define	RS1MSK		0x0007C000
#define	RS2(x)		(0x1f & x)		/* source register 2 */
#define	RS2MSK		0x0000001F
#define	A(x)		((0x1 & x) << 29)	/* annul bit */
#define	AMSK		0x20000000
#define	COND(x)		((0xf & x) << 25)	/* test condition */
#define	CONDMSK		0x1E000000
#define	I(x)		((0x1 & x) << 13)	/* type of second ALU operand */
#define	IMSK		0x00002000
#define	ASI(x)		((0x8 & x) << 5) 	/* address space indentifier */
#define	ASIMSK		0x00001FE0
#define	SHCNT(x)	(0x1F & x)		/* shift count */
#define	SHCNTMSK	0x0000001F
#define	DISP30		0x3FFFFFFF /* 30-bit sign-extended word displacement */
#define	DISP22		0x003FFFFF /* 22-bit sign-extended word displacement */
#define	DISP22_SIGN	0x00200000 /* 22-bit sign */
#define	CONST22		0x003FFFFF /* 22-bit constant value (structure size) */
#define	IMM22		0x003FFFFF /* 22-bit immediate value (result of %hi) */
#define	SIMM13		0x00001FFF /* 13-bit sign-extended immediate value */
#define	SIMM13_SIGN	0x00001000 /* 13-bit sign */
#define	LOBITS		0x000003FF /* 10-bit immediate value (result of %lo) */
#define	TRAP_NUMBER	0x0000007F /* trap number */
#define	DISP22_SIGN_EXTEND 0xFFC00000 /* 22-bit sign-extended word disp */
#define	SIMM13_SIGN_EXTEND 0xFFFFE000 /* 13-bit sign-extended immediate value */
/* new for V9 SPARC */
#define	P(x)		((0x1 & x) << 19)	/* predict bit */
#define	PMSK		0x00080000
#define	CC01_2(x)	((0x3 & x) << 20)	/* fmt 2 cc1, cc0 bits */
#define	CC01_2MSK	0x00300000
#define	CC01_3(x)	((0x3 & x) << 25)	/* fmt 3 cc1, cc0 bits */
#define	CC01_3MSK	0x06000000
#define	CC01_4(x)	((0x3 & x) << 11)	/* fmt 4 cc1, cc0 bits */
#define	CC01_4MSK	0x00001800
#define	CC2_4(x)	((0x1 & x) << 18)	/* fmt 4 cc2 bit */
#define	CC2_4MSK	0x00040000
#define	IXCC_4(x)	((0x1 & x) << 13)	/* another fmt 4 cc2 bit */
#define	IXCC_4MSK	0x00002000
#define	OPF_CC(x)	((0x7 & x) << 11)	/* fmt 4 opf_cc field */
#define	OPF_CCMSK	0x00003800
#define	CC(x)		(0x3 & x)		/* shifted cc1, cc0 bits */
#define	CCMSK		0x00000003
#define	CMSK(x)		((0x7 & x) << 4)	/* constraint mask */
#define	CMSKMSK		0x00000070
#define	MMSK(x)		(0xF & x)		/* memory reference type(s) */
#define	MMSKMSK		0x0000000F
#define	RCOND2(x)	((0x7 & x) << 25)	/* test condition */
#define	RCOND2MSK	0x0E000000
#define	RCOND3(x)	((0x7 & x) << 10)	/* test condition */
#define	RCOND3MSK	0x00001C00
#define	COND4(x)	((0xF & x) << 14)	/* test condition */
#define	COND4MSK	0x0003C000
#define	IMM_ASI(x)	((0xFF & x) << 5)	/* imm_asi field */
#define	IMM_ASIMSK	0x00001FE0
#define	TRAP(x)		(0x7F & x)		/* software_trap# */
#define	TRAPMSK		0x0000007F
#define	XX(x)		((0x1 & x) << 12)	/* x bit */
#define	XXMSK		0x00001000
#define	SHCNT64MSK	0x0000003F
#define	DBLMSK		0x0000003E
#define	QUADMSK		0x0000003C
#define	SIMM10		0x000003FF /* 10-bit sign-extended immediate value */
#define	SIMM10_SIGN	0x00000200 /* 10-bit sign */
#define	SIMM10_SIGN_EXTEND 0xFFFFFC00 /* 10-bit sign-extended immediate value */
#define	SIMM11		0x000007FF /* 11-bit sign-extended immediate value */
#define	SIMM11_SIGN	0x00000400 /* 11-bit sign */
#define	SIMM11_SIGN_EXTEND 0xFFFFF800 /* 11-bit sign-extended immediate value */
#define	DISP19		0x0007FFFF /* 19-bit sign-extended word displacement */
#define	DISP19_SIGN	0x00040000 /* 19-bit sign */
#define	DISP19_SIGN_EXTEND 0xFFF80000 /* 19-bit sign-extended word disp */
#define	D16HIMSK	0x00300000
#define	D16LOMSK	0x00003FFF
#define	DISP16		0x0000FFFF /* 16-bit sign-extended word displacement */
#define	DISP16_SIGN	0x00008000 /* 16-bit sign */
#define	DISP16_SIGN_EXTEND 0xFFFF0000 /* 16-bit sign-extended word disp */

#define	BRMSK		(OPMSK + CONDMSK + OP2MSK)
#define	OPOP3MSK	(OPMSK + OP3MSK)
#define	FOPMSK		(OPMSK + OP3MSK + OPFCMSK)
#define	TRMSK		(OPMSK + CONDMSK + OP3MSK)
/* new for V9 SPARC */
#define	BPRMSK		(OPMSK + RCOND2MSK + OP2MSK)
#define	SHIFTMSK	(OPMSK + OP3MSK + XXMSK)
#define	MOVRMSK		(OPMSK + OP3MSK + RCOND3MSK)

/*
 * opcode, register, constant, and miscellaneous shift counts
 */
#define	DISP30_SHIFT_CT	 2	/* 30-bit sign-extended word displacement */
#define	DISP22_SHIFT_CT	 2	/* 22-bit sign-extended word displacement */
#define	IMM22_SHIFT_CT	10	/* 22-bit immediate value */
/* new for V9 SPARC */
#define	DISP19_SHIFT_CT	 2	/* 19-bit sign-extended word displacement */
#define	DISP16_SHIFT_CT	 2	/* 16-bit sign-extended word displacement */

#define	OP_SHIFT_CT	30	/* general opcode */
#define	A_SHIFT_CT	29	/* annul bit */
#define	RD_SHIFT_CT	25	/* destination register */
#define	COND_SHIFT_CT	25	/* test condition */
#define	OP2_SHIFT_CT	22	/* op2 opcode */
#define	OP3_SHIFT_CT	19	/* op3 opcode */
#define	RS1_SHIFT_CT	14	/* source register 1 */
#define	I_SHIFT_CT	13	/* I field */
#define	OPFC_SHIFT_CT	 5	/* FP or CP opcode */
#define	ASI_SHIFT_CT	 5	/* alternate address space indicator */
/* new for V9 SPARC */
#define	D16HI_SHIFT_CT	 6	/* hi bits of disp16 */
#define	CC01_2SHIFT_CT	20	/* condition code bits for format 2 */
#define	CC01_3SHIFT_CT	25	/* condition code bits for format 3 */
#define	CC01_4SHIFT_CT	11	/* condition code bits for format 4 */

#define	ASI_SUPER_D	0x00000160	/* asi: supervisor data */
#define	ASI_USER_D	0x00000140	/* asi: user data */
#define	ASI_SUPER_I	0x00000120	/* asi: supervisor instruction */
#define	ASI_USER_I	0x00000100	/* asi: user instruction */

/* instruction types */

/*	Listed mostly in alphabetical order
 *	Note that assembly language formats are the overriding concern here.
 *	Sometimes an instruction and it's assebly language format map
 *	one-to-one, but generally, instructions are grouped by the way they
 *	print.
 *
 *	Values must be chosen with care because of the interaction between
 *	the integer version of the instruction type in the INST table
 *	and the byte version in the format table.
 *
 *	itXxxx values should be 1 - 255.
 *	0 is reserved to mark the end of the format table.
 *	values outside the 1 - 255 range (e.g. -1) are reserved for
 *	spotting difficult instructions.  These special values can
 *	not appear in the format table.
 */

	/*
	 * This is where special cases go..  We catch them right away in
	 * dsmPrint and handle them separately
	 */

	/*
	 * Notice that there is no entry for zero.  That's so we know when
	 * we've hit the end of the format table
	 */
#define	itArgAddress		0x01
#define	itArgMaybeRs1Rd		0x02
#define	itArgMaybeRs2Rd		0x03
#define	itArgRd			0x04
#define	itArgRegOrUimm		0x05
#define	itArgRegOrUimmRd	0x06
#define	itArgToAddress		0x07
#define	itBranch		0x08
#define	itBtst			0x09
#define	itCall			0x0A
#define	itCmp			0x0B
#define	itCpop1			0x0C
#define	itCpop2			0x0D
#define	itFP2op			0x0E
#define	itFP3op			0x0F
#define	itFPCmp			0x10
#define	itFpop1			0x11
#define	itFpop2			0x12
#define	itInc			0x13
#define	itIU3op			0x14
#define	itIU3opSimm		0x15
#define	itJmpl			0x16
#define	itLd			0x17
#define	itLdAsi			0x18
#define	itLdCreg		0x19
#define	itLdCspec		0x1A
#define	itLdFreg		0x1B
#define	itLdFspec		0x1C
#define	itMovSpec		0x1D
#define	itNoArg			0x1E
#define	itRdSpec		0x1F
#define	itSethi			0x20
#define	itShift			0x21
#define	itSt			0x22
#define	itStAsi			0x23
#define	itStCreg		0x24
#define	itStCspec		0x25
#define	itStFreg		0x26
#define	itStFspec		0x27
#define	itTrap			0x28
#define	itTst			0x29
#define	itUnimp			0x2A
#define	itWrSpec		0x2B
/* V9 instruction types */
#define	itArgFregRd		0x2C
#define	itArgDregRd		0x2D
#define	itBPcc			0x2E
#define	itBPr			0x2F
#define	itCas			0x30
#define	itCasa			0x31
#define	itFPCmpcc		0x32
#define	itDPCmpcc		0x33
#define	itQPCmpcc		0x34
#define	itFPMvcc		0x35
#define	itDPMvcc		0x36
#define	itQPMvcc		0x37
#define	itFPMvr			0x38
#define	itDPMvr			0x39
#define	itQPMvr			0x3A
#define	itFP1op			0x3B
#define	itDP1op			0x3C
#define	itFP2DP			0x3D
#define	itFP2QP			0x3E
#define	itDP2op			0x3F
#define	itDP2FP			0x40
#define	itDP2QP			0x41
#define	itDP3op			0x42
#define	itQP2op			0x43
#define	itQP2DP			0x44
#define	itQP2FP			0x45
#define	itQP3op			0x46
#define	itDP2Rd			0x47
#define	itIU2opSimm		0x48
#define	itLdAsiFP		0x49
#define	itLdAsiDP		0x4A
#define	itLdAsiQP		0x4B
#define	itLdImmAsi		0x4C
#define	itLdDreg		0x4D
#define	itLdQreg		0x4E
#define	itMembar		0x4F
#define	itMovcc			0x50
#define	itMovr			0x51
#define	itMovV9Spec		0x52
#define	itIPref			0x53
#define	itPref			0x54
#define	itPrefAsi		0x55
#define	itRdsr			0x56
#define	itShift64		0x57
#define	itStAsiFP		0x58
#define	itStAsiDP		0x59
#define	itStAsiQP		0x5A
#define	itStDPAsiPst		0x5B
#define	itStImmAsi		0x5C
#define	itStDreg		0x5D
#define	itStQreg		0x5E
#define	itTcc			0x5F
#define	itTstRs2		0x60
#define	itWrgsr			0x61
#define	itWrsr			0x62
#define	itFP3DP			0x63
#define	itDP3QP			0x64
#define itMvgsr                 0x65
#define	itMvSpec		0x66
#define	itMvrsr			0x67
#define	itIU3Logop		0x68
#define	itMvySpec		0x69

typedef struct {
	char *name;
	unsigned long op;
	unsigned long mask;
	int type;
	unsigned long vermask;
} INST;

extern INST inst[];

/*
 * Instruction Format tokens
 */
#define	atEnd		0x0	/* MUST be zero */
#define	atOp		0x01
#define	atTab		0x02
#define	atComma		0x03
#define	atRs1		0x04
#define	atRegOrUimm	0x05
#define	atRegOrSimm	0x06
#define	atRd		0x07
#define	atAnnul		0x08
#define	atAddress	0x09
#define	atAsi		0x0A
#define	atConst22	0x0B
#define	atCregRd	0x0C
#define	atCregRs1	0x0D
#define	atCregRs2	0x0E
#define	atCspec		0x0F
#define	atDisp22	0x10
#define	atImm22		0x11
#define	atOpfc		0x12
#define	atDisp30	0x13
#define	atFregRd	0x14
#define	atFregRs1	0x15
#define	atFregRs2	0x16
#define	atFspec		0x17
#define	atShift		0x18
#define	atSpec		0x19
#define	atToAddress	0x1A
#define	atToRegAddr	0x1B
#define	atTrap		0x1C
#define	atSimmInc	0x1D
#define	atMaybeRs1	0x1E
#define	atMaybeRs2	0x1F
/* V9 instruction tokens */
#define	atAddressOrImm	0x20
#define	atAppendOp	0x21
#define	atCc		0x22
#define	atCmask		0x23
#define	atCond4		0x24
#define	atFcc		0x25
#define	atFcn		0x26
#define	atDisp16	0x27
#define	atDisp19	0x28
#define	atImmOrAsi	0x29
#define	atMmask		0x2A
#define	atPredict	0x2B
#define	atRegRs1	0x2C
#define	atRegOrSimm10	0x2D
#define	atRegOrSimm11	0x2E
#define	atRegOrShcnt64	0x2F
#define	atRegOrTrap	0x30
#define	atRCond3	0x31
#define	atRs2		0x32
#define	atSpace		0x33
#define	atV9Spec	0x34
#define	atDregRd	0x35
#define	atDregRs1	0x36
#define	atDregRs2	0x37
#define	atQregRd	0x38
#define	atQregRs1	0x39
#define	atQregRs2	0x3A
#define	atLogOp		0x3B
#define	atCom		0x3C

typedef unsigned char FORMAT;

#define	NULL	0

typedef unsigned long  Instruction;


static int	nextword;
static int	prevword;
static int	sparcver;

char	combuf[30];
int	combuf_flag;

/* declarations of commonly used strings, to save space. */
static char commaStr[]  = ", ";
static char plusStr[]   = " + ";
static char spaceStr[]  = " ";

extern char *(iureg[]);

/* Forward declarations */
INST	*dsmFind();
FORMAT	*findFmt();
static char *dis_print();
static char *dsmPrint();
static char *prtFmt();
static char *prtLo();
static char *prtReg();
static char *prtFreg();
static char *prtCreg();
static char *prtSpec();
static char *prtOpfc();
static char *prtAsi();
static char *prtDisp30();
static char *prtConst22();
static char *prtDisp22();
static char *prtImm22();
static char *prtSimm13();
static char *prtEffAddr();
static char *prtRegAddr();
static char *prtRegOrImm();
static char *prtRegOrUimm();
static char *prtRegOrSimm();
static char *nPrtAddress();
/* V9-only Forward declarations */
static char *prtCond4();
static char *prtCc();
static char *prtixcc();
static char *prtfcc();
static char *prtCc();
static char *prtDreg();
static char *prtQreg();
static char *prtICond4();
static char *prtFCond4();
static char *prtRFCond3();
static char *prtCmask();
static char *prtMmask();
static char *prtV9Spec();
static char *prtSimm10();
static char *prtSimm11();
static char *prtDisp16();
static char *prtDisp19();
static char *prtShcnt64();
static char *prtTrap();

/*
 * This table is ordered numerically, starting from word 0 and going up.
 * However, instructions with the same underlying function but which can go
 * by different names are grouped by the number of bits in an instruction's
 * mask, beginning with the greatest number of bits in a mask.  The term
 * that's been chosen to use for these instructions are "spoofs" (called
 * pseudo-instructions, in the Sun-4 Ass'y Language manual).
 *
 * Following certain instructions are literal synonyms (particularly prevalent
 * in the itBranch group) which have been termed "aliases".  Note that these
 * instructions will never be found by dsmFind; they are provided both for
 * completeness and for the usage of code wishing to provide incremental
 * assembly.
 *
 * Spoofs and aliases CANNOT BE MOVED WITH RESPECT TO THEIR CANONICAL
 * INSTRUCTIONS.  The only exception to this is RD, which has been
 * "pre-alias"ed as MOV.
 *
 * All SPARC instructions are the same length (32 bits).
 */

/* declarations of commonly used strings, to save space. */
static char movStr[]    = "mov";

/*
 * SPARC instructions. 
 */

INST inst[] = {
{"unimp", OP(0) + OP2(0), OPMSK + OP2MSK, itUnimp, V8_MODE},
{"illtrap", OP(0) + OP2(0), OPMSK + OP2MSK, itUnimp, V9_MODE},
	/* V9 BPcc instructions, spoof BN */
{"iprefetch", OP(0)+COND(0)+OP2(1), BRMSK,  itIPref, V9_MODE},
{"bn",   OP(0) + COND(0) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"be",   OP(0) + COND(1) + OP2(1), BRMSK,  itBPcc, V9_MODE},
	/* bz is alias for BE */
{"bz",   OP(0) + COND(1) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"ble",  OP(0) + COND(2) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bl",   OP(0) + COND(3) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bleu", OP(0) + COND(4) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bcs",  OP(0) + COND(5) + OP2(1), BRMSK,  itBPcc, V9_MODE},
	/* blu is alias for BCS */
{"blu",  OP(0) + COND(5) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bneg", OP(0) + COND(6) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bvs",  OP(0) + COND(7) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"ba",   OP(0) + COND(8) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bne",  OP(0) + COND(9) + OP2(1), BRMSK,  itBPcc, V9_MODE},
	/* bnz is alias for BNE */
{"bnz",  OP(0) + COND(9) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bg",   OP(0) + COND(0xA) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bge",  OP(0) + COND(0xB) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bgu",  OP(0) + COND(0xC) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bcc",  OP(0) + COND(0xD) + OP2(1), BRMSK,  itBPcc, V9_MODE},
	/* bgeu is alias for BCC */
{"bgeu", OP(0) + COND(0xD) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bpos", OP(0) + COND(0xE) + OP2(1), BRMSK,  itBPcc, V9_MODE},
{"bvc",  OP(0) + COND(0xF) + OP2(1), BRMSK,  itBPcc, V9_MODE},
	/* Bicc instructions */
{"bn",   OP(0) + COND(0) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"be",   OP(0) + COND(1) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
	/* bz is alias for BE */
{"bz",   OP(0) + COND(1) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"ble",  OP(0) + COND(2) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bl",   OP(0) + COND(3) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bleu", OP(0) + COND(4) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
	/* blu is alias for BCS */
{"blu",  OP(0) + COND(5) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bcs",  OP(0) + COND(5) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bneg", OP(0) + COND(6) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bvs",  OP(0) + COND(7) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"ba",   OP(0) + COND(8) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bne",  OP(0) + COND(9) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
	/* bnz is alias for BNE */
{"bnz",  OP(0) + COND(9) + OP2(2), BRMSK,    itBranch, V8_MODE | V9_MODE},
{"bg",   OP(0) + COND(0xA) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bge",  OP(0) + COND(0xB) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bgu",  OP(0) + COND(0xC) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bgeu", OP(0) + COND(0xD) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
	/* bgeu is alias for BCC */
{"bcc",  OP(0) + COND(0xD) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bpos", OP(0) + COND(0xE) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},
{"bvc",  OP(0) + COND(0xF) + OP2(2), BRMSK,  itBranch, V8_MODE | V9_MODE},

{"brz",  OP(0) + RCOND2(1) + OP2(3), BPRMSK,  itBPr, V9_MODE},
{"brlez",OP(0) + RCOND2(2) + OP2(3), BPRMSK,  itBPr, V9_MODE},
{"brlz", OP(0) + RCOND2(3) + OP2(3), BPRMSK,  itBPr, V9_MODE},
{"brnz", OP(0) + RCOND2(5) + OP2(3), BPRMSK,  itBPr, V9_MODE},
{"brgz", OP(0) + RCOND2(6) + OP2(3), BPRMSK,  itBPr, V9_MODE},
{"brgez",OP(0) + RCOND2(7) + OP2(3), BPRMSK,  itBPr, V9_MODE},

{"nop",  OP(0) + RD(0) + OP2(4), OPMSK + RDMSK + OP2MSK + CONST22, itNoArg, V8_MODE | V9_MODE},

/*
 * Note: to change the next entry, if ever needed, also change the
 * look-backwards code at checkLo().  It saves an unneccesary call
 * to dsmFind to do this.
 */
{"sethi",OP(0) + OP2(4), OPMSK + OP2MSK,   itSethi, V8_MODE | V9_MODE},

{"fbn",  OP(0) + COND(0) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbne", OP(0) + COND(1) + OP2(5), BRMSK,  itBPcc, V9_MODE},
	/* fbnz is alias for FBNE */
{"fbnz", OP(0) + COND(1) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fblg", OP(0) + COND(2) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbul", OP(0) + COND(3) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbl",  OP(0) + COND(4) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbug", OP(0) + COND(5) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbg",  OP(0) + COND(6) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbu",  OP(0) + COND(7) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fba",  OP(0) + COND(8) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbe",  OP(0) + COND(9) + OP2(5), BRMSK,  itBPcc, V9_MODE},
	/* fbz is alias for FBE */
{"fbz",  OP(0) + COND(9) + OP2(5), BRMSK,  itBPcc, V9_MODE},
{"fbue", OP(0) + COND(0xA) + OP2(5), BRMSK, itBPcc, V9_MODE},
{"fbge", OP(0) + COND(0xB) + OP2(5), BRMSK, itBPcc, V9_MODE},
{"fbuge",OP(0) + COND(0xC) + OP2(5), BRMSK, itBPcc, V9_MODE},
{"fble", OP(0) + COND(0xD) + OP2(5), BRMSK, itBPcc, V9_MODE},
{"fbule",OP(0) + COND(0xE) + OP2(5), BRMSK, itBPcc, V9_MODE},
{"fbo",  OP(0) + COND(0xF) + OP2(5), BRMSK, itBPcc, V9_MODE},

{"fbn",  OP(0) + COND(0) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbne", OP(0) + COND(1) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
	/* fbnz is alias for FBNE */
{"fbnz", OP(0) + COND(1) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fblg", OP(0) + COND(2) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbul", OP(0) + COND(3) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbl",  OP(0) + COND(4) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbug", OP(0) + COND(5) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbg",  OP(0) + COND(6) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbu",  OP(0) + COND(7) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fba",  OP(0) + COND(8) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbe",  OP(0) + COND(9) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
	/* fbz is alias for FBE */
{"fbz",  OP(0) + COND(9) + OP2(6), BRMSK,   itBranch, V8_MODE | V9_MODE},
{"fbue", OP(0) + COND(0xA) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},
{"fbge", OP(0) + COND(0xB) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},
{"fbuge",OP(0) + COND(0xC) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},
{"fble", OP(0) + COND(0xD) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},
{"fbule",OP(0) + COND(0xE) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},
{"fbo",  OP(0) + COND(0xF) + OP2(6), BRMSK, itBranch, V8_MODE | V9_MODE},

{"cbn", OP(0) + COND(0) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb123", OP(0) + COND(1) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb12", OP(0) + COND(2) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb13", OP(0) + COND(3) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb1", OP(0) + COND(4) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb23", OP(0) + COND(5) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb2", OP(0) + COND(6) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb3", OP(0) + COND(7) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cba", OP(0) + COND(8) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb0", OP(0) + COND(9) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb03", OP(0) + COND(0xA) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb02", OP(0) + COND(0xB) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb023", OP(0) + COND(0xC) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb01", OP(0) + COND(0xD) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb013", OP(0) + COND(0xE) + OP2(7), BRMSK, itBranch, V8_MODE},
{"cb012", OP(0) + COND(0xF) + OP2(7), BRMSK, itBranch, V8_MODE},

{"call", OP(1),	OPMSK, itCall, V8_MODE | V9_MODE},

{"add",  OP(2) + OP3(0), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
	 /* add rd, simm13, rd */
{"inc",  OP(2) + OP3(0), OPOP3MSK, itInc, V8_MODE | V9_MODE},
{"and",  OP(2) + OP3(1), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	 /* spoof OR */
{"clr",  OP(2) + OP3(2) + RS1(0) + I(0) + RS2(0),
	 OPMSK + OP3MSK + RS1MSK + IMSK + RS2MSK, itArgRd, V8_MODE | V9_MODE},
{"clr",  OP(2) + OP3(2) + RS1(0) + I(1) + 0,
	 OPMSK + OP3MSK + RS1MSK + IMSK + SIMM13, itArgRd, V8_MODE | V9_MODE},
	 /* spoof OR */
{movStr, OP(2) + OP3(2) + RS1(0), OPMSK + OP3MSK + RS1MSK, itArgRegOrUimmRd, V8_MODE | V9_MODE},
{"or",   OP(2) + OP3(2) + I(0), OPOP3MSK + IMSK, itIU3Logop, V8_MODE | V9_MODE},
{"or",   OP(2) + OP3(2), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"bset", OP(2) + OP3(2), OPOP3MSK, itArgRegOrUimmRd, V8_MODE | V9_MODE},
{"xor",  OP(2) + OP3(3), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	 /* xor rd, reg_or_imm, rd */
{"btog", OP(2) + OP3(3), OPOP3MSK, itArgRegOrUimmRd, V8_MODE | V9_MODE},
	 /* spoof SUB */
{"sub",  OP(2) + OP3(4) + RS1(0) + I(0),
	 OPMSK + OP3MSK + RS1MSK + IMSK, itIU3opSimm, V8_MODE | V9_MODE},
{"sub",  OP(2) + OP3(4), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"dec",  OP(2) + OP3(4), OPOP3MSK, itInc, V8_MODE},
{"andn", OP(2) + OP3(5), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	 /* andn rd, reg_or_imm, rd */
{"bclr", OP(2) + OP3(5), OPOP3MSK, itArgRegOrUimmRd, V8_MODE | V9_MODE},
{"orn",  OP(2) + OP3(6), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	 /* spoof XNOR */
{"xnor",  OP(2) + OP3(7) + I(0) + RS2(0),
	 OPMSK + OP3MSK + IMSK + RS2MSK, itIU3op, V8_MODE | V9_MODE},
{"xnor", OP(2) + OP3(7), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"addx", OP(2) + OP3(8), OPOP3MSK, itIU3opSimm, V8_MODE},
{"addc", OP(2) + OP3(8), OPOP3MSK, itIU3opSimm, V9_MODE},
{"mulx", OP(2) + OP3(9),   OPOP3MSK, itIU3opSimm, V9_MODE},
{"umul", OP(2) + OP3(0xA), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"smul", OP(2) + OP3(0xB), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"subx", OP(2) + OP3(0xC), OPOP3MSK, itIU3opSimm, V8_MODE},
{"subc", OP(2) + OP3(0xC), OPOP3MSK, itIU3opSimm, V9_MODE},
{"udivx",OP(2) + OP3(0xD), OPOP3MSK, itIU3op, V9_MODE},
{"udiv", OP(2) + OP3(0xE), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"sdiv", OP(2) + OP3(0xF), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},

{"addcc",  OP(2) + OP3(0x10), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
	   /* addcc rd, simm13, rd */
{"inccc",  OP(2) + OP3(0x10), OPOP3MSK, itInc, V8_MODE | V9_MODE},
	   /* spoof ANDCC */
{"btst",   OP(2) + RD(0) + OP3(0x11), OPMSK + RDMSK + OP3MSK, itBtst, V9_MODE},
{"andcc",  OP(2) + OP3(0x11), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	   /* spoof ORCC */
{"tst",    OP(2) + RD(0) + OP3(12) + I(0) + RS2(0),
           OPMSK + RDMSK + OP3MSK + IMSK + RS2MSK, itTst, V8_MODE},
{"tst",    OP(2) + RD(0) + OP3(0x12) + RS1(0) + I(0),
	   OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK, itTstRs2, V9_MODE},
{"orcc",   OP(2) + OP3(0x12), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"xorcc",  OP(2) + OP3(0x13), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
	   /* spoof SUBCC */
{"tst",    OP(2) + RD(0) + OP3(0x14) + RS2(0), OPMSK + RDMSK + OP3MSK + RS2MSK,
itTst, V8_MODE},
{"cmp",    OP(2) + RD(0) + OP3(0x14), OPMSK + RDMSK + OP3MSK, itCmp, V8_MODE | V9_MODE},
{"subcc",  OP(2) + OP3(0x14), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
	   /* subcc rd, simm13, rd */
{"deccc",  OP(2) + OP3(0x14), OPOP3MSK, itInc, V8_MODE | V9_MODE},
{"andncc", OP(2) + OP3(0x15), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"orncc",  OP(2) + OP3(0x16), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"xnorcc", OP(2) + OP3(0x17), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"addxcc", OP(2) + OP3(0x18), OPOP3MSK, itIU3opSimm, V8_MODE},
{"addccc", OP(2) + OP3(0x18), OPOP3MSK, itIU3opSimm, V9_MODE},

{"umulcc", OP(2) + OP3(0x1A), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"smulcc", OP(2) + OP3(0x1B), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"subxcc", OP(2) + OP3(0x1C), OPOP3MSK, itIU3opSimm, V8_MODE},
{"subccc", OP(2) + OP3(0x1C), OPOP3MSK, itIU3opSimm, V9_MODE},

{"udivcc", OP(2) + OP3(0x1E), OPOP3MSK, itIU3op, V8_MODE | V9_MODE},
{"sdivcc", OP(2) + OP3(0x1F), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"taddcc", OP(2) + OP3(0x20), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"tsubcc", OP(2) + OP3(0x21), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"taddcctv",OP(2) + OP3(0x22), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"tsubcctv",OP(2) + OP3(0x23), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"mulscc", OP(2) + OP3(0x24), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"sll",    OP(2) + OP3(0x25) + XX(0), SHIFTMSK, itShift, V8_MODE | V9_MODE},
{"sllx",   OP(2) + OP3(0x25) + XX(1), SHIFTMSK, itShift64, V8_MODE | V9_MODE},
{"srl",    OP(2) + OP3(0x26) + XX(0), SHIFTMSK, itShift, V8_MODE | V9_MODE},
{"srlx",   OP(2) + OP3(0x26) + XX(1), SHIFTMSK, itShift64, V8_MODE | V9_MODE},
{"sra",    OP(2) + OP3(0x27) + XX(0), SHIFTMSK, itShift, V8_MODE | V9_MODE},
{"srax",   OP(2) + OP3(0x27) + XX(1), SHIFTMSK, itShift64, V8_MODE | V9_MODE},
	   /* stbar alias */
{"stbar",  OP(2) + OP3(0x28) + RS1(0xf) + I(0) + RD(0),
	   OPOP3MSK + RS1MSK + IMSK + RDMSK, itNoArg, V8_MODE | V9_MODE},
{"membar", OP(2) + OP3(0x28) + RS1(0xf) + I(1) + RD(0),
	   OPOP3MSK + RS1MSK + IMSK + RDMSK, itMembar, V9_MODE},
	    /* from %y */
{"rd",     OP(2) + OP3(0x28) + RS1(0), OPMSK + OP3MSK + RS1MSK, itRdSpec, V8_MODE | V9_MODE},
	   /* from %asr[rs1] */
{"rd",     OP(2) + OP3(0x28), OPOP3MSK, itRdSpec, V8_MODE},
{"rd",     OP(2) + OP3(0x28) + I(0), OPOP3MSK + IMSK, itRdsr, V9_MODE},
{"rd",     OP(2) + OP3(0x28) + RS1(0x13) + I(0), OPOP3MSK + RS1MSK + IMSK, itRdsr, V9_SGI_MODE},
        /* from %psr */
{"rd", OP(2) + OP3(0x29), OPOP3MSK, itRdSpec, V8_MODE},
        /* from %wim */
{"rd", OP(2) + OP3(0x2A), OPOP3MSK, itRdSpec, V8_MODE},
        /* from %tbr */
{"rd", OP(2) + OP3(0x2B), OPOP3MSK, itRdSpec, V8_MODE},
 

{"rdpr",   OP(2) + OP3(0x2A), OPOP3MSK, itRdsr, V9_MODE},
{"flushw", OP(2) + OP3(0x2B), OPOP3MSK, itNoArg, V9_MODE},
	/* V9 MOVcc instructions */
{"mov",    OP(2) + OP3(0x2C), OPOP3MSK,  itMovcc, V9_MODE},

{"sdivx",  OP(2) + OP3(0x2D), OPOP3MSK, itIU3opSimm, V9_MODE},
{"popc",   OP(2) + OP3(0x2E) + RS1(0), OPOP3MSK + RS1MSK, itIU2opSimm, V9_MODE},

{"movre",  OP(2) + OP3(0x2F) + RCOND3(1), MOVRMSK,  itMovr, V9_MODE},
	/* movrz is alias for movre */
{"movrz",  OP(2) + OP3(0x2F) + RCOND3(1), MOVRMSK,  itMovr, V9_MODE},
{"movrlez",OP(2) + OP3(0x2F) + RCOND3(2), MOVRMSK,  itMovr, V9_MODE},
{"movrlz", OP(2) + OP3(0x2F) + RCOND3(3), MOVRMSK,  itMovr, V9_MODE},
{"movrne", OP(2) + OP3(0x2F) + RCOND3(5), MOVRMSK,  itMovr, V9_MODE},
	/* movrnz is alias for movrne */
{"movrnz", OP(2) + OP3(0x2F) + RCOND3(5), MOVRMSK,  itMovr, V9_MODE},
{"movrgz", OP(2) + OP3(0x2F) + RCOND3(6), MOVRMSK,  itMovr, V9_MODE},
{"movrgez",OP(2) + OP3(0x2F) + RCOND3(7), MOVRMSK,  itMovr, V9_MODE},

{"sir",    OP(2) + OP3(0x30) + RD(0xf) + RS1(0) + I(1),
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK, itArgAddress, V9_MODE},
	    /* to %gsr */
{"wr",      OP(2) + RD(0x13) + OP3(0x30), OPMSK + RDMSK + OP3MSK, itWrgsr, V9_SGI_MODE},
	    /* to %y */
{"mov",      OP(2) + RD(0) + OP3(0x30) + RS1(0), OPMSK + RDMSK + OP3MSK + RS1MSK, itMvySpec, V8_MODE},
{"wr",      OP(2) + RD(0) + OP3(0x30), OPMSK + RDMSK + OP3MSK, itWrSpec, V8_MODE},
{"wr",      OP(2) + RD(0) + OP3(0x30), OPMSK + RDMSK + OP3MSK, itWrsr, V9_MODE},
	    /* to %asr[rd] */
{"mov",     OP(2) + OP3(0x30) +I(0) + RS2(0), OPOP3MSK + IMSK + RS2MSK, itMvSpec, V8_MODE},
{"mov",     OP(2) + OP3(0x30) + RS1(0), OPOP3MSK + RS1MSK, itMvySpec, V8_MODE},
{"wr",      OP(2) + OP3(0x30), OPOP3MSK, itWrSpec, V8_MODE},
{"mov",     OP(2) + OP3(0x30) + I(0) + RS2(0), OPOP3MSK + IMSK + RS2MSK, itMvrsr, V9_MODE},
{"wr",      OP(2) + OP3(0x30), OPOP3MSK, itWrsr, V9_MODE},

        /* to %psr */
{"mov",      OP(2) + OP3(0x31) + RS1(0), OPOP3MSK + RS1MSK, itMvySpec, V8_MODE},
{"wr",      OP(2) + OP3(0x31), OPOP3MSK, itWrSpec, V8_MODE},
        /* to %wim */
{"mov",      OP(2) + OP3(0x32) + RS1(0), OPOP3MSK + RS1MSK, itMvySpec, V8_MODE},
{"wr",      OP(2) + OP3(0x32), OPOP3MSK, itWrSpec, V8_MODE},
        /* to %tbr */
{"mov",      OP(2) + OP3(0x33) + RS1(0), OPOP3MSK + RS1MSK, itMvySpec, V8_MODE},
{"wr",      OP(2) + OP3(0x33), OPOP3MSK, itWrSpec, V8_MODE},
{"saved",   OP(2) + OP3(0x31) + RD(0), OPOP3MSK + RDMSK, itNoArg, V9_MODE},
{"restored",OP(2) + OP3(0x31) + RD(1), OPOP3MSK + RDMSK, itNoArg, V9_MODE},
{"wrpr",    OP(2) + OP3(0x32), OPOP3MSK, itWrsr, V9_MODE},

{"fmovs", OP(2) + OP3(0x34) + OPFC(1), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fmovd", OP(2) + OP3(0x34) + OPFC(2), FOPMSK, itDP2op, V9_MODE},
{"fmovq", OP(2) + OP3(0x34) + OPFC(3), FOPMSK, itQP2op, V9_MODE},

{"fnegs", OP(2) + OP3(0x34) + OPFC(5), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fnegd", OP(2) + OP3(0x34) + OPFC(6), FOPMSK, itDP2op, V9_MODE},
{"fnegq", OP(2) + OP3(0x34) + OPFC(7), FOPMSK, itQP2op, V9_MODE},

{"fabss", OP(2) + OP3(0x34) + OPFC(9), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fabsd", OP(2) + OP3(0x34) + OPFC(0xA), FOPMSK, itDP2op, V9_MODE},
{"fabsq", OP(2) + OP3(0x34) + OPFC(0xB), FOPMSK, itQP2op, V9_MODE},

{"fsqrts", OP(2) + OP3(0x34) + OPFC(0x29), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fsqrtd", OP(2) + OP3(0x34) + OPFC(0x2a), FOPMSK, itDP2op, V8_MODE | V9_MODE},
{"fsqrtq", OP(2) + OP3(0x34) + OPFC(0x2b), FOPMSK, itQP2op, V8_MODE | V9_MODE},

{"fadds", OP(2) + OP3(0x34) + OPFC(0x41), FOPMSK, itFP3op, V8_MODE | V9_MODE},
{"faddd", OP(2) + OP3(0x34) + OPFC(0x42), FOPMSK, itDP3op, V8_MODE | V9_MODE},
{"faddq", OP(2) + OP3(0x34) + OPFC(0x43), FOPMSK, itQP3op, V8_MODE | V9_MODE},

{"fsubs", OP(2) + OP3(0x34) + OPFC(0x45), FOPMSK, itFP3op, V8_MODE | V9_MODE},
{"fsubd", OP(2) + OP3(0x34) + OPFC(0x46), FOPMSK, itDP3op, V8_MODE | V9_MODE},
{"fsubq", OP(2) + OP3(0x34) + OPFC(0x47), FOPMSK, itQP3op, V8_MODE | V9_MODE},

{"fmuls", OP(2) + OP3(0x34) + OPFC(0x49), FOPMSK, itFP3op, V8_MODE | V9_MODE},
{"fmuld", OP(2) + OP3(0x34) + OPFC(0x4a), FOPMSK, itDP3op, V8_MODE | V9_MODE},
{"fmulq", OP(2) + OP3(0x34) + OPFC(0x4b), FOPMSK, itQP3op, V8_MODE | V9_MODE},

{"fdivs", OP(2) + OP3(0x34) + OPFC(0x4d), FOPMSK, itFP3op, V8_MODE | V9_MODE},
{"fdivd", OP(2) + OP3(0x34) + OPFC(0x4e), FOPMSK, itDP3op, V8_MODE | V9_MODE},
{"fdivq", OP(2) + OP3(0x34) + OPFC(0x4f), FOPMSK, itQP3op, V8_MODE | V9_MODE},

{"fsmuld", OP(2) + OP3(0x34) + OPFC(0x69), FOPMSK, itFP3DP, V8_MODE | V9_MODE},
{"fdmulq", OP(2) + OP3(0x34) + OPFC(0x6e), FOPMSK, itDP3QP, V8_MODE | V9_MODE},

{"fstox", OP(2) + OP3(0x34) + OPFC(0x81), FOPMSK, itFP2op, V9_MODE},
{"fdtox", OP(2) + OP3(0x34) + OPFC(0x82), FOPMSK, itDP2op, V9_MODE},
{"fqtox", OP(2) + OP3(0x34) + OPFC(0x83), FOPMSK, itQP2op, V9_MODE},
{"fxtos", OP(2) + OP3(0x34) + OPFC(0x84), FOPMSK, itFP2op, V9_MODE},
{"fxtod", OP(2) + OP3(0x34) + OPFC(0x88), FOPMSK, itDP2op, V9_MODE},
{"fxtoq", OP(2) + OP3(0x34) + OPFC(0x8C), FOPMSK, itQP2op, V9_MODE},

{"fitos", OP(2) + OP3(0x34) + OPFC(0xC4), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fdtos", OP(2) + OP3(0x34) + OPFC(0xC6), FOPMSK, itDP2FP, V8_MODE | V9_MODE},
{"fqtos", OP(2) + OP3(0x34) + OPFC(0xC7), FOPMSK, itQP2FP, V8_MODE | V9_MODE},
{"fitod", OP(2) + OP3(0x34) + OPFC(0xC8), FOPMSK, itDP2op, V8_MODE | V9_MODE},
{"fstod", OP(2) + OP3(0x34) + OPFC(0xC9), FOPMSK, itFP2DP, V8_MODE | V9_MODE},
{"fqtod", OP(2) + OP3(0x34) + OPFC(0xCB), FOPMSK, itQP2DP, V8_MODE | V9_MODE},
{"fitoq", OP(2) + OP3(0x34) + OPFC(0xCC), FOPMSK, itQP2op, V8_MODE | V9_MODE},
{"fstoq", OP(2) + OP3(0x34) + OPFC(0xCD), FOPMSK, itFP2QP, V8_MODE | V9_MODE},
{"fdtoq", OP(2) + OP3(0x34) + OPFC(0xCE), FOPMSK, itDP2QP, V8_MODE | V9_MODE},
{"fstoi", OP(2) + OP3(0x34) + OPFC(0xD1), FOPMSK, itFP2op, V8_MODE | V9_MODE},
{"fdtoi", OP(2) + OP3(0x34) + OPFC(0xD2), FOPMSK, itDP2op, V8_MODE | V9_MODE},
{"fqtoi", OP(2) + OP3(0x34) + OPFC(0xD3), FOPMSK, itQP2op, V8_MODE | V9_MODE},
	/* any other extra fpop1's */
{"fpop1", OP(2) + OP3(0x34), OPOP3MSK, itFpop1, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(1), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(2), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(3), FOPMSK, itQPMvcc, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0x25), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0x26), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0x27), FOPMSK, itQPMvr, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(0x41), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(0x42), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(0x43), FOPMSK, itQPMvcc, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0x45), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0x46), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0x47), FOPMSK, itQPMvr, V9_MODE},

{"fcmps", OP(2) + OP3(0x35) + OPFC(0x51), FOPMSK, itFPCmp, V8_MODE},
{"fcmpd", OP(2) + OP3(0x35) + OPFC(0x52), FOPMSK, itFPCmp, V8_MODE},
{"fcmpq", OP(2) + OP3(0x35) + OPFC(0x53), FOPMSK, itFPCmp, V8_MODE},

{"fcmps", OP(2) + OP3(0x35) + OPFC(0x51), FOPMSK, itFPCmpcc, V9_MODE},
{"fcmpd", OP(2) + OP3(0x35) + OPFC(0x52), FOPMSK, itDPCmpcc, V9_MODE},
{"fcmpq", OP(2) + OP3(0x35) + OPFC(0x53), FOPMSK, itQPCmpcc, V9_MODE},


{"fcmpes", OP(2) + OP3(0x35) + OPFC(0x55), FOPMSK, itFPCmp, V8_MODE},
{"fcmped", OP(2) + OP3(0x35) + OPFC(0x56), FOPMSK, itFPCmp, V8_MODE},
{"fcmpeq", OP(2) + OP3(0x35) + OPFC(0x57), FOPMSK, itFPCmp, V8_MODE},

{"fcmpes", OP(2) + OP3(0x35) + OPFC(0x55), FOPMSK, itFPCmpcc, V9_MODE},
{"fcmped", OP(2) + OP3(0x35) + OPFC(0x56), FOPMSK, itDPCmpcc, V9_MODE},
{"fcmpeq", OP(2) + OP3(0x35) + OPFC(0x57), FOPMSK, itQPCmpcc, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0x65), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0x66), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0x67), FOPMSK, itQPMvr, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(0x81), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(0x82), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(0x83), FOPMSK, itQPMvcc, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0xA5), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0xA6), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0xA7), FOPMSK, itQPMvr, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(0xC1), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(0xC2), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(0xC3), FOPMSK, itQPMvcc, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0xC5), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0xC6), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0xC7), FOPMSK, itQPMvr, V9_MODE},

{"fmovrs", OP(2) + OP3(0x35) + OPFC(0xE5), FOPMSK, itFPMvr, V9_MODE},
{"fmovrd", OP(2) + OP3(0x35) + OPFC(0xE6), FOPMSK, itDPMvr, V9_MODE},
{"fmovrq", OP(2) + OP3(0x35) + OPFC(0xE7), FOPMSK, itQPMvr, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(0x101), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(0x102), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(0x103), FOPMSK, itQPMvcc, V9_MODE},

{"fmovs", OP(2) + OP3(0x35) + OPFC(0x181), FOPMSK, itFPMvcc, V9_MODE},
{"fmovd", OP(2) + OP3(0x35) + OPFC(0x182), FOPMSK, itDPMvcc, V9_MODE},
{"fmovq", OP(2) + OP3(0x35) + OPFC(0x183), FOPMSK, itQPMvcc, V9_MODE},
	/* any other extra fpop2's */
{"fpop2", OP(2) + OP3(0x35), OPOP3MSK, itFpop2, V9_MODE},
	/* IMPDEP1 */
{"edge8",      OP(2) + OP3(0x36) + OPFC(0x0), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"edge8l",     OP(2) + OP3(0x36) + OPFC(0x2), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"edge16",     OP(2) + OP3(0x36) + OPFC(0x4), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"edge16l",    OP(2) + OP3(0x36) + OPFC(0x6), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"edge32",     OP(2) + OP3(0x36) + OPFC(0x8), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"edge32l",    OP(2) + OP3(0x36) + OPFC(0xA), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},

{"array8",     OP(2) + OP3(0x36) + OPFC(0x10), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"array16",    OP(2) + OP3(0x36) + OPFC(0x12), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"array32",    OP(2) + OP3(0x36) + OPFC(0x14), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"alignaddr",  OP(2) + OP3(0x36) + OPFC(0x18), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},
{"alignaddrl", OP(2) + OP3(0x36) + OPFC(0x1A), OPOP3MSK + OPFCMSK, itIU3op, V9_SGI_MODE},

{"fcmple16",   OP(2) + OP3(0x36) + OPFC(0x20), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpne16",   OP(2) + OP3(0x36) + OPFC(0x22), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmple32",   OP(2) + OP3(0x36) + OPFC(0x24), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpne32",   OP(2) + OP3(0x36) + OPFC(0x26), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpgt16",   OP(2) + OP3(0x36) + OPFC(0x28), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpeq16",   OP(2) + OP3(0x36) + OPFC(0x2A), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpgt32",   OP(2) + OP3(0x36) + OPFC(0x2C), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},
{"fcmpeq32",   OP(2) + OP3(0x36) + OPFC(0x2E), OPOP3MSK + OPFCMSK, itDP2Rd, V9_SGI_MODE},

{"fmul8x16",   OP(2) + OP3(0x36) + OPFC(0x31), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmul8x16au", OP(2) + OP3(0x36) + OPFC(0x33), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmul8x16al", OP(2) + OP3(0x36) + OPFC(0x35), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmul8sux16", OP(2) + OP3(0x36) + OPFC(0x36), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmul8ulx16", OP(2) + OP3(0x36) + OPFC(0x37), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmuld8sux16",OP(2) + OP3(0x36) + OPFC(0x38), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"fmuld8ulx16",OP(2) + OP3(0x36) + OPFC(0x39), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},
{"pdist",       OP(2) + OP3(0x36) + OPFC(0x3E), OPOP3MSK + OPFCMSK, itFP3DP, V9_SGI_MODE},

{"fpack32",   OP(2) + OP3(0x36) + OPFC(0x3A), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpack16",   OP(2) + OP3(0x36) + OPFC(0x3B), OPOP3MSK + OPFCMSK, itFP2op, V9_SGI_MODE},
{"fpackfix",  OP(2) + OP3(0x36) + OPFC(0x3D), OPOP3MSK + OPFCMSK, itFP2op, V9_SGI_MODE},
{"faligndata",OP(2) + OP3(0x36) + OPFC(0x48), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fpmerge",   OP(2) + OP3(0x36) + OPFC(0x4B), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fexpand",   OP(2) + OP3(0x36) + OPFC(0x4D), OPOP3MSK + OPFCMSK, itFP2op, V9_SGI_MODE},

{"fpadd16", OP(2) + OP3(0x36) + OPFC(0x50), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpadd16s",OP(2) + OP3(0x36) + OPFC(0x51), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpadd32", OP(2) + OP3(0x36) + OPFC(0x52), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpadd32s",OP(2) + OP3(0x36) + OPFC(0x53), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpsub16", OP(2) + OP3(0x36) + OPFC(0x54), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpsub16s",OP(2) + OP3(0x36) + OPFC(0x55), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpsub32", OP(2) + OP3(0x36) + OPFC(0x56), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fpsub32s",OP(2) + OP3(0x36) + OPFC(0x57), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},

{"fzero",   OP(2) + OP3(0x36) + OPFC(0x60), OPOP3MSK + OPFCMSK, itArgDregRd, V9_SGI_MODE},
{"fzeros",  OP(2) + OP3(0x36) + OPFC(0x61), OPOP3MSK + OPFCMSK, itArgFregRd, V9_SGI_MODE},
{"fnor",    OP(2) + OP3(0x36) + OPFC(0x62), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fnors",   OP(2) + OP3(0x36) + OPFC(0x63), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fandnot2",OP(2) + OP3(0x36) + OPFC(0x64), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fandnot2s",OP(2) + OP3(0x36) + OPFC(0x65), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fnot2",   OP(2) + OP3(0x36) + OPFC(0x66), OPOP3MSK + OPFCMSK, itDP2op, V9_SGI_MODE},
{"fnot2s",  OP(2) + OP3(0x36) + OPFC(0x67), OPOP3MSK + OPFCMSK, itFP2op, V9_SGI_MODE},
{"fandnot1",OP(2) + OP3(0x36) + OPFC(0x68), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fandnot1s",OP(2) + OP3(0x36) + OPFC(0x69), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fnot1",   OP(2) + OP3(0x36) + OPFC(0x6A), OPOP3MSK + OPFCMSK, itDP1op, V9_SGI_MODE},
{"fnot1s",  OP(2) + OP3(0x36) + OPFC(0x6B), OPOP3MSK + OPFCMSK, itFP1op, V9_SGI_MODE},
{"fxor",    OP(2) + OP3(0x36) + OPFC(0x6C), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fxors",   OP(2) + OP3(0x36) + OPFC(0x6D), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fnand",   OP(2) + OP3(0x36) + OPFC(0x6E), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fnands",  OP(2) + OP3(0x36) + OPFC(0x6F), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},

{"fand",    OP(2) + OP3(0x36) + OPFC(0x70), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fands",   OP(2) + OP3(0x36) + OPFC(0x71), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fxnor",   OP(2) + OP3(0x36) + OPFC(0x72), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fxnors",  OP(2) + OP3(0x36) + OPFC(0x73), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fsrc1",   OP(2) + OP3(0x36) + OPFC(0x74), OPOP3MSK + OPFCMSK, itDP1op, V9_SGI_MODE},
{"fsrc1s",  OP(2) + OP3(0x36) + OPFC(0x75), OPOP3MSK + OPFCMSK, itFP1op, V9_SGI_MODE},
{"fornot2", OP(2) + OP3(0x36) + OPFC(0x76), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fornot2s",OP(2) + OP3(0x36) + OPFC(0x77), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fsrc2",   OP(2) + OP3(0x36) + OPFC(0x78), OPOP3MSK + OPFCMSK, itDP2op, V9_SGI_MODE},
{"fsrc2s",  OP(2) + OP3(0x36) + OPFC(0x79), OPOP3MSK + OPFCMSK, itFP2op, V9_SGI_MODE},
{"fornot1", OP(2) + OP3(0x36) + OPFC(0x7A), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fornot1s",OP(2) + OP3(0x36) + OPFC(0x7B), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"for",     OP(2) + OP3(0x36) + OPFC(0x7C), OPOP3MSK + OPFCMSK, itDP3op, V9_SGI_MODE},
{"fors",    OP(2) + OP3(0x36) + OPFC(0x7D), OPOP3MSK + OPFCMSK, itFP3op, V9_SGI_MODE},
{"fone",    OP(2) + OP3(0x36) + OPFC(0x7E), OPOP3MSK + OPFCMSK, itArgDregRd, V9_SGI_MODE},
{"fones",   OP(2) + OP3(0x36) + OPFC(0x7F), OPOP3MSK + OPFCMSK, itArgFregRd, V9_SGI_MODE},

{"shutdown",OP(2) + OP3(0x36) + OPFC(0x80), OPOP3MSK + OPFCMSK, itNoArg, V9_SGI_MODE},

	    /* spoof JMPL */
{"ret",     OP(2) + RD(0) + OP3(0x38) + RS1(0x1f) + I(1) + 8,
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + SIMM13, itNoArg, V8_MODE | V9_MODE},
	    /* spoof JMPL */
{"jmp",    OP(2) + RD(0) + OP3(0x38) + RS1(0xf) + I(1) + 8,
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + SIMM13, itArgAddress, V8_MODE},
{"retl",    OP(2) + RD(0) + OP3(0x38) + RS1(0xf) + I(1) + 8,
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + SIMM13, itNoArg, V9_MODE},
	    /* spoof JMPL */
{"jmp",     OP(2) + RD(0) + OP3(0x38), OPMSK + RDMSK + OP3MSK, itArgAddress, V8_MODE | V9_MODE},
	    /* spoof JMPL */
{"jmpl",    OP(2) + RD(0xf) + OP3(0x38), OPMSK + RDMSK + OP3MSK, itJmpl, V8_MODE},
{"call",    OP(2) + RD(0xf) + OP3(0x38), OPMSK + RDMSK + OP3MSK, itArgAddress, V8_MODE},
						    /* (CALL reg_or_imm) */
{"jmpl",    OP(2) + OP3(0x38), OPOP3MSK, itJmpl, V8_MODE | V9_MODE},
{"rett", OP(2) + OP3(0x39), OPOP3MSK, itArgAddress, V8_MODE},
{"return",  OP(2) + OP3(0x39), OPOP3MSK, itArgAddress, V9_MODE},

	    /* V9 Tcc instructions */
{"tn",      OP(2) + COND(0) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tn",      OP(2) + COND(0) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"te",      OP(2) + COND(1) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"te",      OP(2) + COND(1) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
	    /* tz is alias for TE */
{"tz",      OP(2) + COND(1) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tz",      OP(2) + COND(1) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tle",     OP(2) + COND(2) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tle",     OP(2) + COND(2) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tl",      OP(2) + COND(3) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tl",      OP(2) + COND(3) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tleu",     OP(2) + COND(4) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tleu",    OP(2) + COND(4) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tcs",     OP(2) + COND(5) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
	    /* tlu is alias for TCS */
{"tlu",     OP(2) + COND(5) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tlu",     OP(2) + COND(5) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tneg",     OP(2) + COND(6) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tneg",    OP(2) + COND(6) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tvs",     OP(2) + COND(7) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tvs",     OP(2) + COND(7) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"ta",      OP(2) + COND(8) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"ta",      OP(2) + COND(8) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"t",      OP(2) + COND(8) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tne",     OP(2) + COND(9) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tne",     OP(2) + COND(9) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
	    /* tnz is alias for TNE */
{"tnz",     OP(2) + COND(9) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tnz",     OP(2) + COND(9) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tg",      OP(2) + COND(0xA) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tg",      OP(2) + COND(0xA) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tge",     OP(2) + COND(0xB) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tge",     OP(2) + COND(0xB) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tgu",     OP(2) + COND(0xC) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tgu",     OP(2) + COND(0xC) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tcc",     OP(2) + COND(0xD) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
	    /* tgeu is alias for TCC */
{"tgeu",     OP(2) + COND(0xD) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tgeu",    OP(2) + COND(0xD) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tpos",     OP(2) + COND(0xE) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tpos",    OP(2) + COND(0xE) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},
{"tvc",     OP(2) + COND(0xF) + OP3(0x3A), TRMSK,  itTrap, V8_MODE},
{"tvc",     OP(2) + COND(0xF) + OP3(0x3A), OPMSK + CONDMSK + OP3MSK,  itTcc, V9_MODE},

{"iflush",   OP(2) + OP3(0x3B), OPOP3MSK, itArgAddress, V8_MODE},
{"flush",   OP(2) + OP3(0x3B), OPOP3MSK, itArgAddress, V9_MODE},
{"save",     OP(2) + RD(0) + OP3(0x3C) + RS1(0) + I(0) + RS2(0),
        OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + RS2MSK, itNoArg, V8_MODE},
{"save",    OP(2) + RD(0) + OP3(0x3C) + RS1(0) + I(0) + RS2(0),
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + RS2MSK, itIU3opSimm, V9_MODE},
	    /* spoof SAVE */
{"save",    OP(2) + OP3(0x3C), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"restore", OP(2) + RD(0) + OP3(0x3D) + RS1(0) + I(0) + RS2(0),
	    OPMSK + RDMSK + OP3MSK + RS1MSK + IMSK + RS2MSK, itNoArg, V8_MODE | V9_MODE},
	    /* spoof RESTORE */
{"restore", OP(2) + OP3(0x3D), OPOP3MSK, itIU3opSimm, V8_MODE | V9_MODE},
{"done",    OP(2) + OP3(0x3E) + RD(0), OPOP3MSK + RDMSK, itNoArg, V9_MODE},
{"retry",   OP(2) + OP3(0x3E) + RD(1), OPOP3MSK + RDMSK, itNoArg, V9_MODE},

	    /* spoof LDUW */
{"ld",      OP(3) + OP3(0x00), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"lduw",    OP(3) + OP3(0x00), OPOP3MSK, itLd, V9_MODE},
{"ldub",    OP(3) + OP3(0x01), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"lduh",    OP(3) + OP3(0x02), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"ldd",     OP(3) + OP3(0x03), OPOP3MSK, itLd, V8_MODE | V9_MODE},
	    /* spoof ST */
	    /* spoof STW */
{"st",      OP(3) + OP3(0x04), OPOP3MSK, itSt, V8_MODE | V9_MODE},
{"clr",     OP(3) + OP3(0x04) + RD(0), OPMSK + OP3MSK + RDMSK, itArgToAddress, V8_MODE | V9_MODE},
{"stw",     OP(3) + OP3(0x04), OPOP3MSK, itSt, V9_MODE},
	    /* spoof STB */
{"stb",     OP(3) + OP3(0x05), OPOP3MSK, itSt, V8_MODE},
{"clrb",    OP(3) + OP3(0x05) + RD(0), OPMSK + OP3MSK + RDMSK, itArgToAddress, V8_MODE | V9_MODE},
{"stb",     OP(3) + OP3(0x05), OPOP3MSK, itSt, V9_MODE},
        /* alias STB */
{"stsb",  OP(3) + OP3(0x05), OPOP3MSK, itSt, V8_MODE},
        /* alias STB */
{"stub",  OP(3) + OP3(0x05), OPOP3MSK, itSt, V8_MODE},
	    /* spoof STH */
{"clrh",    OP(3) + OP3(0x06) + RD(0), OPMSK + OP3MSK + RDMSK, itArgToAddress, V8_MODE | V9_MODE},
{"sth",     OP(3) + OP3(0x06), OPOP3MSK, itSt, V8_MODE | V9_MODE},
{"std",     OP(3) + OP3(0x07), OPOP3MSK, itSt, V8_MODE | V9_MODE},
{"ldsw",    OP(3) + OP3(0x08), OPOP3MSK, itLd, V9_MODE},
{"ldsb",    OP(3) + OP3(0x09), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"ldsh",    OP(3) + OP3(0x0A), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"ldx",     OP(3) + OP3(0x0B), OPOP3MSK, itLd, V9_MODE},

{"ldstub",  OP(3) + OP3(0x0D), OPOP3MSK, itLd, V8_MODE | V9_MODE},
{"stx",     OP(3) + OP3(0x0E), OPOP3MSK, itSt, V9_MODE},
{"swap",    OP(3) + OP3(0x0F), OPOP3MSK, itLd, V8_MODE | V9_MODE},
	    /* do not spoof LDUWA */
{"lda",   OP(3) + OP3(0x10), OPOP3MSK, itLdImmAsi, V8_MODE},
{"lduwa",   OP(3) + OP3(0x10), OPOP3MSK, itLdImmAsi, V9_MODE},
{"lduba",   OP(3) + OP3(0x11), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},
{"lduha",   OP(3) + OP3(0x12), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},
{"ldda",    OP(3) + OP3(0x13), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE | V9_SGI_MODE},
	    /* do not spoof STWA */
{"sta",   OP(3) + OP3(0x14), OPOP3MSK, itStImmAsi, V8_MODE},
{"stwa",    OP(3) + OP3(0x14), OPOP3MSK, itStImmAsi, V9_MODE},
{"stba",    OP(3) + OP3(0x15), OPOP3MSK, itStImmAsi, V8_MODE | V9_MODE},
{"stsba",  OP(3) + OP3(0x15), OPOP3MSK, itStAsi, V8_MODE}, /* alias STBA */
{"stuba",  OP(3) + OP3(0x15), OPOP3MSK, itStAsi, V8_MODE}, /* alias STBA */
{"stha",    OP(3) + OP3(0x16), OPOP3MSK, itStImmAsi, V8_MODE | V9_MODE},
{"stsha",  OP(3) + OP3(0x16), OPOP3MSK, itStAsi, V8_MODE}, /* alias STHA */
{"stuha",  OP(3) + OP3(0x16), OPOP3MSK, itStAsi, V8_MODE}, /* alias STHA */
{"stda",    OP(3) + OP3(0x17), OPOP3MSK, itStImmAsi, V8_MODE | V9_MODE},
{"ldswa",   OP(3) + OP3(0x18), OPOP3MSK, itLdImmAsi, V9_MODE},
{"ldsba",   OP(3) + OP3(0x19), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},
{"ldsha",   OP(3) + OP3(0x1A), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},
{"ldxa",    OP(3) + OP3(0x1B), OPOP3MSK, itLdImmAsi, V9_MODE},

{"ldstuba", OP(3) + OP3(0x1D), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},
{"stxa",    OP(3) + OP3(0x1E), OPOP3MSK, itStImmAsi, V9_MODE},
{"swapa",   OP(3) + OP3(0x1F), OPOP3MSK, itLdImmAsi, V8_MODE | V9_MODE},

{"ld",      OP(3) + OP3(0x20), OPOP3MSK, itLdFreg, V8_MODE | V9_MODE}, /* to freg[rd] */
{"ld",      OP(3) + OP3(0x21), OPOP3MSK, itLdFspec, V8_MODE}, /* to %fsr */
{"ld",      OP(3) + OP3(0x21) + RD(0),
	    OPMSK + RDMSK + OP3MSK, itLdFspec, V9_MODE}, /* to %fsr */
{"ldx",     OP(3) + OP3(0x21) + RD(1),
	    OPMSK + RDMSK + OP3MSK, itLdFspec, V9_MODE}, /* to %fsr */
{"ldq",     OP(3) + OP3(0x22), OPOP3MSK, itLdQreg, V9_MODE}, /* to freg[rd] */
{"ldd",     OP(3) + OP3(0x23), OPOP3MSK, itLdDreg, V8_MODE | V9_MODE}, /* to freg[rd] */
{"st",      OP(3) + OP3(0x24), OPOP3MSK, itStFreg, V8_MODE | V9_MODE}, /* from freg[rd] */
{"st",      OP(3) + OP3(0x25), OPOP3MSK, itStFspec, V8_MODE}, /* from %fsr */
{"st",      OP(3) + OP3(0x25) + RD(0),
	    OPMSK + RDMSK + OP3MSK, itStFspec, V9_MODE}, /* from %fsr */
{"stx",     OP(3) + OP3(0x25) + RD(1),
	    OPMSK + RDMSK + OP3MSK, itStFspec, V9_MODE}, /* from %fsr */
{"std",     OP(3) + OP3(0x26), OPOP3MSK, itStFspec, V8_MODE}, /* from %fq */
{"stq",     OP(3) + OP3(0x26), OPOP3MSK, itStQreg, V9_MODE}, /* from freg[rd] */
{"std",     OP(3) + OP3(0x27), OPOP3MSK, itStDreg, V8_MODE | V9_MODE}, /* from freg[rd] */

{"prefetch", OP(3) + OP3(0x2D), OPOP3MSK, itPref, V9_MODE},

{"ld",      OP(3) + OP3(0x30), OPOP3MSK, itLdCreg, V8_MODE}, /* to creg[rd] */
{"lda",     OP(3) + OP3(0x30), OPOP3MSK, itLdAsiFP, V9_MODE},  /* to freg[rd] */
{"ld",      OP(3) + OP3(0x31), OPOP3MSK, itLdCspec, V8_MODE}, /* to %csr */
{"ldqa",    OP(3) + OP3(0x32), OPOP3MSK, itLdAsiQP, V9_MODE},  /* to freg[rd] */
{"ldd",     OP(3) + OP3(0x33), OPOP3MSK, itLdCreg, V8_MODE}, /* to creg[rd] */
{"ldda",    OP(3) + OP3(0x33), OPOP3MSK, itLdAsiDP, V9_MODE | V9_SGI_MODE},  /* to freg[rd] */
{"st",      OP(3) + OP3(0x34), OPOP3MSK, itStCreg, V8_MODE}, /* from creg[rd] */
{"sta",     OP(3) + OP3(0x34), OPOP3MSK, itStAsiFP, V9_MODE},  /* from freg[rd] */

{"st",      OP(3) + OP3(0x35), OPOP3MSK, itStCspec, V8_MODE}, /* from %csr */
{"std",     OP(3) + OP3(0x36), OPOP3MSK, itStCspec, V8_MODE}, /* from %cq */
{"stqa",    OP(3) + OP3(0x36), OPOP3MSK, itStAsiQP, V9_MODE},  /* from freg[rd] */
{"std",     OP(3) + OP3(0x37), OPOP3MSK, itStCreg, V8_MODE}, /* from creg[rd] */
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC0),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},  /* groan */
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC1),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC2),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC3),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC4),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC5),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC8),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xC9),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xCA),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xCB),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xCC),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37) + I(0) + IMM_ASI(0xCD),
		OPOP3MSK + IMSK + IMM_ASIMSK, itStDPAsiPst, V9_SGI_MODE},
{"stda",    OP(3) + OP3(0x37), OPOP3MSK, itStAsiDP, V9_MODE | V9_SGI_MODE},

	    /* spoof CAS */
{"cas",     OP(3) + OP3(0x3C) + IMM_ASI(0x80), OPOP3MSK + IMM_ASIMSK, itCas, V9_MODE},
{"casa",    OP(3) + OP3(0x3C), OPOP3MSK, itCasa, V9_MODE},
{"prefetcha", OP(3) + OP3(0x3D), OPOP3MSK, itPrefAsi, V9_MODE},
	    /* spoof CAS */
{"casx",    OP(3) + OP3(0x3E) + IMM_ASI(0x80), OPOP3MSK + IMM_ASIMSK, itCas, V9_MODE},
{"casxa",   OP(3) + OP3(0x3E), OPOP3MSK, itCasa, V9_MODE},

{"", NULL, 	NULL, 0}
};


/*
 *	This table tells you how to parse the various instructions.
 *	Each entry is a variable length sequence of one-byte values.
 *	atEnd is zero, so you can tell when you get to the end of
 *	an entry.
 *	The first value in each entry is the instruction type which
 *	it corresponds to. Note that itXxxx is a 32-bit integer above
 *	and a byte down here. Choose values with care. Negative
 *	values provide a handy way of skipping out for those
 *	problem pseudo-ops. They can't appear in the table.
 */
FORMAT format[] = {
	itArgAddress, atOp, atAddress, atEnd,
	itArgMaybeRs1Rd, atOp, atMaybeRs1, atRd, atEnd,
	itArgMaybeRs2Rd, atOp, atMaybeRs2, atRd, atEnd,
	itArgRd, atOp, atRd, atEnd,
	itArgRegOrUimm, atOp, atRegOrUimm, atEnd,
	itArgRegOrUimmRd, atOp, atRegOrUimm, atComma, atRd, atEnd,
	itArgToAddress, atOp, atToAddress, atEnd,
	itBranch, atOp, atAnnul, atDisp22, atEnd,
	itBtst, atOp, atRegOrUimm, atComma, atRs1, atEnd,
	itCall, atOp, atDisp30, atEnd,
	itCmp, atOp, atRs1, atComma, atRegOrSimm, atEnd,
	itCpop1, atOp, atOpfc, atComma, atCregRs1, atComma, atCregRs2,
						atComma, atCregRd, atEnd,
	itCpop2, atOp, atOpfc, atComma, atCregRs1, atComma, atCregRs2,
						atComma, atCregRd, atEnd,
	itFP2op, atOp, atFregRs2, atComma, atFregRd, atEnd,
	itFP2DP, atOp, atFregRs2, atComma, atDregRd, atEnd,
	itFP2QP, atOp, atFregRs2, atComma, atQregRd, atEnd,
	itFP3op, atOp, atFregRs1, atComma, atFregRs2, atComma, atFregRd,
									atEnd,
	itDP2op, atOp, atDregRs2, atComma, atDregRd, atEnd,
	itDP2FP, atOp, atDregRs2, atComma, atFregRd, atEnd,
	itDP2QP, atOp, atDregRs2, atComma, atQregRd, atEnd,
	itDP3op, atOp, atDregRs1, atComma, atDregRs2, atComma, atDregRd,
									atEnd,
	itQP2op, atOp, atQregRs2, atComma, atQregRd, atEnd,
	itQP2DP, atOp, atQregRs2, atComma, atDregRd, atEnd,
	itQP2FP, atOp, atQregRs2, atComma, atFregRd, atEnd,
	itQP3op, atOp, atQregRs1, atComma, atQregRs2, atComma, atQregRd,
									atEnd,
	itFPCmp, atOp, atFregRs1, atComma, atFregRs2, atEnd,
	itFpop1, atOp, atOpfc, atComma, atFregRs1, atComma, atFregRs2,
						atComma, atFregRd, atEnd,
	itFpop2, atOp, atOpfc, atComma, atFregRs1, atComma, atFregRs2,
						atComma, atFregRd, atEnd,
	itInc, atOp, atSimmInc, atRd, atEnd,
	itIU3op, atOp, atRs1, atComma, atRegOrUimm, atComma, atRd, atCom, atEnd,
	itIU3opSimm, atOp, atRs1, atComma, atRegOrSimm, atComma, atRd, atEnd,
	itJmpl, atOp, atAddress, atComma, atRd, atEnd,
	itLd, atOp, atToAddress, atComma, atRd, atEnd,
	itLdAsi, atOp, atToRegAddr, atAsi, atComma, atRd, atEnd,
	itLdCreg, atOp, atToAddress, atComma, atCregRd, atEnd,
	itLdCspec, atOp, atToAddress, atComma, atCspec, atEnd,
	itLdFreg, atOp, atToAddress, atComma, atFregRd, atEnd,
	itLdDreg, atOp, atToAddress, atComma, atDregRd, atEnd,
	itLdQreg, atOp, atToAddress, atComma, atQregRd, atEnd,
	itLdFspec, atOp, atToAddress, atComma, atFspec, atEnd,
	itMovSpec, atOp, atRegOrUimm, atComma, atSpec, atEnd,
	itNoArg, atOp, atEnd,
	itRdSpec, atOp, atSpec, atComma, atRd, atEnd,
	itSethi, atOp, atImm22, atComma, atRd, atEnd,
	itShift, atOp, atRs1, atComma, atRegOrUimm, atComma, atRd, atEnd,
	itSt, atOp, atRd, atComma, atToAddress, atEnd,
	itStAsi, atOp, atRd, atComma, atToRegAddr, atAsi, atEnd,
	itStCreg, atOp, atCregRd, atComma, atToAddress, atEnd,
	itStCspec, atOp, atCspec, atComma, atToAddress, atEnd,
	itStFreg, atOp, atFregRd, atComma, atToAddress, atEnd,
	itStDreg, atOp, atDregRd, atComma, atToAddress, atEnd,
	itStQreg, atOp, atQregRd, atComma, atToAddress, atEnd,
	itStFspec, atOp, atFspec, atComma, atToAddress, atEnd,
	itTrap, atOp, atTrap, atEnd,
	itTst, atOp, atRs1, atEnd,
	itUnimp, atOp, atConst22, atEnd,
	itWrSpec, atOp, atRs1, atComma, atRegOrUimm, atComma, atSpec, atEnd,
		/* new for V9 SPARC */
	itArgFregRd, atOp, atFregRd, atEnd,
	itArgDregRd, atOp, atDregRd, atEnd,
	itBPcc, atAppendOp, atAnnul, atPredict, atCc, atDisp19, atEnd,
	itBPr, atAppendOp, atAnnul, atPredict, atRs1, atComma, atDisp16, atEnd,
	itCas, atOp, atRegRs1, atComma, atRs2, atComma, atRd, atEnd,
	itCasa, atOp, atRegRs1, atImmOrAsi, atComma, atRs2, atComma,
							atRd, atEnd,
	itFPCmpcc, atOp, atFcc, atFregRs1, atComma, atFregRs2, atEnd,
	itDPCmpcc, atOp, atFcc, atDregRs1, atComma, atDregRs2, atEnd,
	itQPCmpcc, atOp, atFcc, atQregRs1, atComma, atQregRs2, atEnd,
	itFPMvcc, atAppendOp, atCond4, atCc, atFregRs2, atComma,
						  atFregRd, atEnd,
	itDPMvcc, atAppendOp, atCond4, atCc, atDregRs2, atComma,
						  atDregRd, atEnd,
	itQPMvcc, atAppendOp, atCond4, atCc, atQregRs2, atComma,
						  atQregRd, atEnd,
	itFPMvr, atAppendOp, atRCond3, atRs1, atComma, atFregRs2, atComma,
						  atFregRd, atEnd,
	itDPMvr, atAppendOp, atRCond3, atRs1, atComma, atDregRs2, atComma,
						  atDregRd, atEnd,
	itQPMvr, atAppendOp, atRCond3, atRs1, atComma, atDregRs2, atComma,
						  atDregRd, atEnd,
	itFP1op, atOp, atFregRs1, atComma, atFregRd, atEnd,
	itDP1op, atOp, atDregRs1, atComma, atDregRd, atEnd,
	itDP2Rd, atOp, atFregRs1, atComma, atFregRs2, atComma, atRd, atEnd,
	itLdAsiFP, atOp, atAddressOrImm, atImmOrAsi, atComma, atFregRd, atEnd,
	itLdAsiDP, atOp, atAddressOrImm, atImmOrAsi, atComma, atDregRd, atEnd,
	itLdAsiQP, atOp, atAddressOrImm, atImmOrAsi, atComma, atQregRd, atEnd,
	itLdImmAsi, atOp, atAddressOrImm, atImmOrAsi, atComma, atRd, atEnd,
	itMembar, atOp, atCmask, atMmask, atEnd,
	itRdsr, atOp, atV9Spec, atComma, atRd, atEnd,
	itMovcc, atAppendOp, atCond4, atCc, atRegOrSimm11, atComma, atRd, atEnd,
	itIU2opSimm, atOp, atRegOrSimm, atComma, atRd, atEnd,
	itMovr, atOp, atRs1, atComma, atRegOrSimm10, atComma, atRd, atEnd,
	itMovV9Spec, atOp, atRegOrUimm, atComma, atV9Spec, atEnd,
	itIPref, atAppendOp, atSpace, atDisp19, atEnd,
	itPref, atOp, atAddressOrImm, atComma, atFcn, atEnd,
	itPrefAsi, atOp, atAddressOrImm, atImmOrAsi, atComma, atFcn, atEnd,
	itShift64, atOp, atRs1, atComma, atRegOrShcnt64, atComma, atRd, atEnd,
	itStImmAsi, atOp, atRd, atComma, atAddressOrImm, atImmOrAsi, atEnd,
	itStAsiFP, atOp, atFregRd, atComma, atAddressOrImm, atImmOrAsi, atEnd,
	itStAsiDP, atOp, atDregRd, atComma, atAddressOrImm, atImmOrAsi, atEnd,
	itStAsiQP, atOp, atQregRd, atComma, atAddressOrImm, atImmOrAsi, atEnd,
	itStDPAsiPst, atOp, atDregRd, atComma, atRegRs1, atRs2, atComma,
							atImmOrAsi, atEnd,
	itTstRs2, atOp, atRs2, atEnd,
	itTcc, atOp, atCc, atRegOrTrap, atEnd,
	itWrgsr, atOp, atRd, atComma, atRs1, atComma, atV9Spec, atEnd,
	itWrsr, atOp, atRs1, atComma, atRegOrSimm, atComma, atV9Spec, atEnd,
	itFP3DP, atOp, atFregRs1, atComma, atFregRs2, atComma, atDregRd,
									atEnd,
	itDP3QP, atOp, atDregRs1, atComma, atDregRs2, atComma, atQregRd,
									atEnd,
	itMvgsr, atOp, atRs1, atComma, atV9Spec, atEnd,
	itMvSpec, atOp, atRs1, atComma, atSpec, atEnd,
	itMvrsr, atOp, atRs1, atComma, atV9Spec, atEnd,
        itIU3Logop, atLogOp, atOp, atRs1, atComma, atRegOrUimm, atComma, atRd, 									atEnd,
	itMvySpec, atOp, atRegOrSimm, atComma, atSpec, atEnd,
	atEnd, atEnd
};

/*
 * The following table is the ascii for use in printing the Integer
 * Unit registers.
 */
char *(iureg[]) =
{
	"%g0",		/* register %g0 use: 0 */
	"%g1",		/* register %g1 use: global 1 */
	"%g2",		/* register %g2 use: global 2 */
	"%g3",		/* register %g3 use: global 3 */
	"%g4",		/* register %g4 use: global 4 */
	"%g5",		/* register %g5 use: global 5 */
	"%g6",		/* register %g6 use: global 6 */
	"%g7",		/* register %g7 use: global 6 */
	"%o0",		/* register %o0 use: outgoing parameter 0 */
	"%o1",		/* register %o1 use: outgoing parameter 1 */
	"%o2",		/* register %o2 use: outgoing parameter 2 */
	"%o3",		/* register %o3 use: outgoing parameter 3 */
	"%o4",		/* register %o4 use: outgoing parameter 4 */
	"%o5",		/* register %o5 use: outgoing parameter 5 */
	"%sp",		/* register %o6 use: stack pointer, alias: %o6 */
	"%o7",		/* register %o7 use: temp */
	"%l0",		/* register %l0 use: local 0 */
	"%l1",		/* register %l1 use: local 1 */
	"%l2",		/* register %l2 use: local 2 */
	"%l3",		/* register %l3 use: local 3 */
	"%l4",		/* register %l4 use: local 4 */
	"%l5",		/* register %l5 use: local 5 */
	"%l6",		/* register %l6 use: local 6 */
	"%l7",		/* register %l7 use: local 7 */
	"%i0",		/* register %i0 use: incoming parameter 0 */
	"%i1",		/* register %i1 use: incoming parameter 1 */
	"%i2",		/* register %i2 use: incoming parameter 2 */
	"%i3",		/* register %i3 use: incoming parameter 3 */
	"%i4",		/* register %i4 use: incoming parameter 4 */
	"%i5",		/* register %i5 use: incoming parameter 5 */
	"%fp",		/* register %i6 use: frame pointer, alias: %i6 */
	"%i7"		/* register %i7 use: return address */
};

#define	sign_extend13(x) \
	((x & SIMM13_SIGN) ? (x | SIMM13_SIGN_EXTEND) : (x & SIMM13))
/* for V9 SPARC */
#define	sign_extend10(x) \
	((x & SIMM10_SIGN) ? (x | SIMM10_SIGN_EXTEND) : (x & SIMM10))
#define	sign_extend11(x) \
	((x & SIMM11_SIGN) ? (x | SIMM11_SIGN_EXTEND) : (x & SIMM11))

static char dis_buf[256];

/*
char *toSymbolic(addr)
	unsigned int addr;
{
	static char buf[64];
	char *bufptr = buf;

	extsympr(addr, &bufptr);
	return (buf);
}
*/

char *
disassemble(unsigned int inst, unsigned long pc,
	FUNCPTR prtAddress, unsigned int next, unsigned int prev,
	int vers)
{
	register INST *iPtr;

	nextword = next;
	prevword = prev;
	sparcver = vers;
	iPtr = dsmFind(inst);
	(void) dsmPrint(dis_buf, inst, iPtr, pc, pc, prtAddress);
	return (dis_buf);
}


void
dsmError()
{
	printf("dis: Internal Error\n");
}

/*
 * dsmFind - disassemble one instruction
 *
 * This routine figures out which instruction is in bits,
 * and returns a pointer to the INST which describes it. If no INST is
 * found, returns NULL.
 */
INST *
dsmFind(bits)
	register Instruction bits;
{
	register INST *iPtr;

	/* Find out which instruction it is */
	for (iPtr = &inst[0]; iPtr->mask != NULL; iPtr++) {
		if (((bits & iPtr->mask) == iPtr->op) && 
			(iPtr->vermask & sparcver)){
				return (iPtr);
		}
	}
	return (NULL);
}


/*
 * dsmPrint - print a disassembled instruction
 *
 * This routine prints an instruction in disassembled form. It takes
 * as input a pointer to the instruction, a pointer to the INST
 * that describes it (as found by dsmFind), and an address with which to
 * prepend the instruction.
 */
static char *
dsmPrint(cp, bits, iPtr, InstPtr, address, prtAddress)
	char *cp;			/* output string buffer */
	register Instruction bits;	/* bits of current instruction */
	register INST	*iPtr;		/* ptr to INST returned by dsmFind */
	Instruction	*InstPtr;	/* pointer to instruction */
	unsigned long int address;	/* address to prepend to instruction */
	FUNCPTR		prtAddress;	/* address printing function */
{
	register FORMAT	*fmtptr;	/* pointer to format entry */

	if (iPtr == NULL) {
		return (dis_print(cp, "%-3s", "???"));
	}

	/* Print the instruction mnemonic and the arguments */

	if ((iPtr->type) < 0) {
		/* special case for hard instruction types */

		fmtptr = (FORMAT *) 0;

		/*
		 * Don't need any special cases right now, but this is where
		 * they would go.
		 */

	} else {
		/* normal instruction types */
		fmtptr = findFmt(iPtr->type);
	}
	if (fmtptr == (FORMAT *) 0) {
		dsmError();
		*cp = NULL;
		return (0);
	}
	return (prtFmt(cp, bits, iPtr, fmtptr, InstPtr, address, prtAddress));
}

/*
 * findFmt - Find the format string corresponding to an instruction type
 */
FORMAT *
findFmt(iType)
	register int iType;	/* Instruction type from INST table entry */
{
	register FORMAT	*p;

	if ((iType <= 0) || (iType > 255))
		return ((FORMAT *) 0);

	p = format;

	while (((int) *p) != iType) {
		for (p; *p != atEnd; p++);

		p++;	/* skip to beginning of next entry */

		if (((int) *p) == atEnd) return ((FORMAT *) 0);
	}
	return (p);
}


/*
 * print a string into a buffer using one optional arg
 * returns the position after last inserted character in buffer
 * Note: print assumes only one arg
 */
static char *
dis_print(cp, fmt, arg)
	char 	*cp;		/* buffer pointer */
	char	*fmt;		/* printf format string */
	int	arg;		/* only one in format string */
{
	sprintf(cp, fmt, arg);
	cp += strlen(cp);
	return (cp);
}

/*
 * prtFmt - Print the arguments for an instruction
 */
static char *
prtFmt(cp, bits, iPtr, fmtptr, InstPtr, address, prtAddress)
	register char *cp;
	register Instruction bits;		/* The binary instruction */
	INST	*iPtr;		/* Pointer to INST describing bits */
	register FORMAT	*fmtptr;	/* Pointer to format entry */
	Instruction	*InstPtr;	/* pointer to instruction */
	unsigned long int address;	/* Address of the instruction */
	FUNCPTR		prtAddress;	/* Routine to print addresses */
{
	register FORMAT	fmt;
	Instruction	pBits;		/* The previous instruction */
	Instruction	nBits;		/* The next instruction */
	u_int cc;
	int rs1, rs2, rd;
	int len;

	/*
	 * For now, just skip over the instruction type.
	 * There may be some reasonmable checking we could add here,
	 * but we don't want to just check versus iPtr->type, since
	 * we may get here after one of the spoofs.
	 */
	fmtptr++;

	for (; *fmtptr != atEnd; fmtptr++) {
		fmt = *fmtptr;
		switch (fmt) {

		case atOp:
			if ((*(fmtptr+1) == atAnnul) &&
			    ((bits & AMSK) == A(1))) {
				register int i, n;
				cp = dis_print(cp, "%s", iPtr->name);
				n = strlen(iPtr->name);
				cp = dis_print(cp, "%s", ",a");
				n = 8 - (n + 2);
				for (i = 0; i < n; i++)
					cp[i] = ' ';
				cp += n;
				fmtptr++;
			} else {
				int n;
				n = strlen(iPtr->name);

				cp = dis_print(cp, "%-8s\t", iPtr->name);
                                /*
                                 * This is a hack to keep the interface
                                 * same as that of previous dis.
                                 */
                                if ((strcmp(iPtr->name,"taddcctv") == 0) ||
                                    (strcmp(iPtr->name, "tsubcctv") == 0))
                                        cp = dis_print(cp, "\t");
				else if (n >= 8)
					cp = dis_print(cp, "%s", " ");

			}
			break;

		case atLogOp:
				/* Hack Alert */
			rs1 = (bits >> RS1_SHIFT_CT) & RS2MSK;
			rs2 = (bits >> RD_SHIFT_CT) & RS2MSK;
			rd = (bits) & RS2MSK;
			if ((rs1 == rs2) &&  (rs2 == rd)) {
					cp = dis_print(cp, "nop");
					return (cp);
			}
			break;
		case atAnnul:
			if ((bits & AMSK) == A(1)) {
			    if (*(fmtptr+1) != atPredict)
				cp = dis_print(cp, ",a ");
			    else
				cp = dis_print(cp, ",a");
			}
			break;

		case atPredict:
			if ((bits & PMSK) == P(1))
				cp = dis_print(cp, ",pt ");
			else
				cp = dis_print(cp, ",pn ");
			break;

		case atComma:
			cp = dis_print(cp, commaStr);
			break;

		case atRs1:
			cp = prtReg(cp, bits >> RS1_SHIFT_CT);
			break;

		case atRd:
			cp = prtReg(cp, bits >> RD_SHIFT_CT);
			break;

		case atCregRs1:
			cp = prtCreg(cp, bits >> RS1_SHIFT_CT);
			break;

		case atCregRs2:
			cp = prtCreg(cp, bits);
			break;

		case atCregRd:
			cp = prtCreg(cp, bits >> RD_SHIFT_CT);
			break;

		case atFregRs1:
			cp = prtFreg(cp, bits >> RS1_SHIFT_CT);
			break;

		case atFregRs2:
			cp = prtFreg(cp, bits);
			break;

		case atFregRd:
			cp = prtFreg(cp, bits >> RD_SHIFT_CT);
			break;

		case atDregRs1:
			cp = prtDreg(cp, bits >> RS1_SHIFT_CT);
			break;

		case atDregRs2:
			cp = prtDreg(cp, bits);
			break;

		case atDregRd:
			cp = prtDreg(cp, bits >> RD_SHIFT_CT);
			break;

		case atQregRs1:
			cp = prtQreg(cp, bits >> RS1_SHIFT_CT);
			break;

		case atQregRs2:
			cp = prtQreg(cp, bits);
			break;

		case atQregRd:
			cp = prtQreg(cp, bits >> RD_SHIFT_CT);
			break;

		case atCspec:
		case atFspec:
		case atSpec:
			cp = prtSpec(cp, bits);
			break;

		case atToRegAddr:
			cp = dis_print(cp, "[");
			cp = prtRegAddr(cp, bits);
			cp = dis_print(cp, "]");
			break;

		case atAsi:
			cp = prtAsi(cp, bits);
			break;

		case atConst22:
			cp = prtConst22(cp, bits);
			break;

		case atDisp22:
			cp = prtDisp22(cp, address, bits, prtAddress);
			break;

		case atTrap:
		case atAddress:
			if (((int)InstPtr != 0) &&
                                ((Instruction)InstPtr != bits))
				pBits = prevword;
			cp = prtEffAddr(cp, fmt, pBits, bits, prtAddress);
			break;

		case atToAddress:
			if (((int)InstPtr != 0) &&
                                ((Instruction)InstPtr != bits))
				pBits = prevword;
			cp = dis_print(cp, "[");
			cp = prtEffAddr(cp, fmt, pBits, bits, prtAddress);
			cp = dis_print(cp, "]");
			break;

		case atRegOrUimm:
		case atRegOrSimm:
		case atRegOrSimm10:
		case atRegOrSimm11:
			if (((int)InstPtr != 0) &&
                                ((Instruction)InstPtr != bits))
				pBits = prevword;
			cp = prtRegOrImm(cp, fmt, pBits, bits, prtAddress);
			break;

		case atImm22:
			if (((int)InstPtr != 0) &&
                                ((Instruction)InstPtr != bits)) {
				if ((nBits = nextword) == 0)
					return (cp);
			}
			cp = dis_print(cp, "%%hi(");
			cp = prtImm22(cp, bits, nBits, prtAddress);
			cp = dis_print(cp, ")");
			break;

		case atOpfc:
			cp = prtOpfc(cp, bits);
			break;

		case atDisp30:
			cp = prtDisp30(cp, address, bits, prtAddress);
			break;

		case atSimmInc:
			if ((bits & SIMM13) != 1) {
				cp = prtSimm13(cp, bits);
				cp = dis_print(cp, commaStr);
			}
			break;

		case atMaybeRs1:
			if (((bits & RS1MSK) >> RS1_SHIFT_CT) !=
					((bits & RDMSK) >> RD_SHIFT_CT)) {
				cp = prtReg(cp, bits >> RS1_SHIFT_CT);
				cp = dis_print(cp, commaStr);
			}
			break;

		case atMaybeRs2:
			if ((bits & RS2MSK) !=
					((bits & RDMSK) >> RD_SHIFT_CT)) {
				cp = prtReg(cp, bits);
				cp = dis_print(cp, commaStr);
			}
			break;

		case atAppendOp:
			len = strlen(iPtr->name);
                        cp = dis_print(cp, "%s", iPtr->name);
			break;

		case atAddressOrImm:
			if ((bits & IMSK) == I(1)) {
			    if (((int)InstPtr != 0) &&
                                ((Instruction)InstPtr != bits))
				pBits = prevword;
			    cp = dis_print(cp, "[");
			    cp = prtReg(cp, bits >> RS1_SHIFT_CT);
			    cp = dis_print(cp, plusStr);
			    cp = prtRegOrImm(cp, fmt, pBits, bits, prtAddress);
			    cp = dis_print(cp, "] ");
			} else {
			    cp = dis_print(cp, "[");
			    cp = prtRegAddr(cp, bits);
			    cp = dis_print(cp, "] ");
			}
			break;

		case atCc:
			cp = prtCc(cp, bits);
			break;

		case atFcc:
			cc = (bits & CC01_3MSK) >> CC01_3SHIFT_CT;
			cp = prtfcc(cp, cc);
			break;

		case atCond4:
			cp = prtCond4(cp, bits);
			break;

		case atCmask:
			cp = prtCmask(cp, bits);
			break;

		case atMmask:
			cp = prtMmask(cp, bits);
			break;

		case atFcn:
			cp = dis_print(cp, "%d", (bits >> RD_SHIFT_CT) & 0x1F);
			break;

		case atImmOrAsi:
			if ((bits & IMSK) == I(1))
				cp = dis_print(cp, "%%asi");
			else
				cp = prtAsi(cp, bits);
			break;

		case atRCond3:
			cp = prtRFCond3(cp, bits);
			break;

		case atRegOrShcnt64:
			if ((bits & IMSK) == I(1))
				cp = prtShcnt64(cp, bits);
			else 				/* I = 0: r[rs2] */
				cp = prtReg(cp, bits);
			break;

		case atRegOrTrap:
			if ((bits & IMSK) == I(1)) {
				cp = prtReg(cp, bits >> RS1_SHIFT_CT);
				cp = dis_print(cp, plusStr);
				cp = prtTrap(cp, bits);
			} else {
				cp = prtRegAddr(cp, bits);
			}
			break;

		case atRegRs1:
			cp = dis_print(cp, "[");
			cp = prtReg(cp, bits >> RS1_SHIFT_CT);
			cp = dis_print(cp, "] ");
			break;

		case atRs2:
			cp = prtReg(cp, bits);
			break;

		case atSpace:
			cp = dis_print(cp, spaceStr);
			break;

		case atV9Spec:
			cp = prtV9Spec(cp, bits);
			break;

		case atDisp16:
			cp = prtDisp16(cp, address, bits, prtAddress);
			break;

		case atDisp19:
			cp = prtDisp19(cp, address, bits, prtAddress);
			break;

		case atCom:
			cp = dis_print(cp, combuf);
			combuf[0] = '\0';
			break;
		default:
			dsmError();
		}
	}
	return (cp);
}


/*
 * prtImm22 - print the Imm22 field, using heuristics to spot
 *		sethi	%hi(value), %rx
 *		op	%rx, %lo(value), %rd
 *	pairs.
 */
static char *
prtImm22(cp, bits, nBits, prtAddress)
	register char *cp;
	register Instruction	bits;	/* Bits of current instruction */
	register Instruction	nBits;	/* Bits of next instruction */
	FUNCPTR			prtAddress; /* Routine to print addresses */
{
	char buf[15];
	int v;

	/*
	 * First, pretend we know the instructions will pair up. That way, we
	 * can eliminate many cases on the easy tests before we have to figure
	 * out what the next instruction is. To do this we pass a fake format
	 * token.
	 */
	combuf_flag = 1;
	if (checkLo(atAddress, bits, nBits) != 1) {
                v = (bits & IMM22) << IMM22_SHIFT_CT;
                sprintf(buf, "0x%x", v );
                cp = dis_print(cp, "%s", buf);
	} else {
		/* switch on next instruction type */
		switch (dsmFind(nBits)->type) {

		case itArgAddress:
		case itArgToAddress:
		case itJmpl:
		case itLd:
		case itLdCreg:
		case itLdCspec:
		case itLdFreg:
		case itLdDreg:
		case itLdQreg:
		case itLdFspec:
		case itSt:
		case itStCreg:
		case itStCspec:
		case itStFreg:
		case itStDreg:
		case itStQreg:
		case itStFspec:
			combuf_flag = 0;
			break;

		case itIU3op:
		case itIU3opSimm:
			/* if it's "add" or "or", it counts */
			if (((nBits & (OPOP3MSK)) == (OP(2) + OP3(0))) ||
				((nBits & (OPOP3MSK)) == (OP(2) + OP3(0x2)))) {
				break;
			}
			/* otherwise fall through */

		default:
			cp = dis_print(cp, "%s", prtAddress(
					(bits & IMM22) << IMM22_SHIFT_CT));
			return (cp);
		}
                /* For v8 compatibility
                 * cp = dis_print(cp, "%s", prtAddress(
                 *                      ((bits & IMM22) << IMM22_SHIFT_CT) |
                 *                                      (nBits & LOBITS)));
                 */
                 cp = dis_print(cp, "%s", prtAddress(
                                        ((bits & IMM22) << IMM22_SHIFT_CT)));
	}
	return (cp);
}


/*
 * prtEffAddr -
 *	print effective address of a register-or-immediate instruction.
 *	This can take one of the following forms:
 *		%rs1
 *		%rs1 + %rs2
 *		%rs1 + simm13
 *		%rs1 - simm13
 *		simm13
 *		%rs1 + %lo(value)
 */
static char *
prtEffAddr(cp, fmt, pBits, bits, prtAddress)
	register char	*cp;			/* string buffer pointer */
	register FORMAT		fmt;	/* format token that got us here */
	register Instruction	pBits;	/* Bits of previous instruction */
	register Instruction	bits;	/* Bits of current instruction */
	FUNCPTR		prtAddress;	/* Routine to print addresses */
{
	if (checkLo(fmt, pBits, bits) == 1) {
		cp = prtReg(cp, bits >> RS1_SHIFT_CT);
		cp = dis_print(cp, plusStr);
		cp = prtLo(cp, pBits, bits, prtAddress);
	} else if ((bits & IMSK) == I(1)) {
		/*
		 * I = 1: "[%rs1] <+|-> sign_extend(simm13)"
		 * or
		 *	 "%rs1 + %lo(value)"
		 */
		if ((bits & RS1MSK) == RS1(0)) {
			cp = dis_print(cp, "0x%x", sign_extend13(bits));
		} else {
			cp = prtReg(cp, bits >> RS1_SHIFT_CT);
			if ((bits & SIMM13) != 0) {
				if ((bits & SIMM13_SIGN) == SIMM13_SIGN) {
					cp = dis_print(cp, " - ");
					cp = prtSimm13(cp,
						-(sign_extend13(bits)));
				} else {
					cp = dis_print(cp, plusStr);
					cp = prtSimm13(cp, bits);
				}
			}
		}
	} else { /* I = 0: "%rs1[ + %rs2]" */
		cp = prtRegAddr(cp, bits);
	}
	return (cp);
}


/*
 * prtRegOrImm - print reg-or-imm field of a register-or-immediate instruction.
 */
static char *
prtRegOrImm(cp, fmt, pBits, bits, prtAddress)
	register char *cp;
	register FORMAT		fmt;	/* format token that got us here */
	register Instruction	pBits;	/* Bits of previous instruction */
	register Instruction	bits;	/* Bits of current instruction */
	FUNCPTR prtAddress;		/* Routine to print addresses */
{
	if (checkLo(fmt, pBits, bits) == 1) {
		cp = prtLo(cp, pBits, bits, prtAddress);
	} else if ((bits & IMSK) == I(1)) {
		/*
		 * I = 1: sign_extend(simm13) or sign_extend(simm11)
		 *	 or
		 *	 %lo(value)
		 */
		if (fmt == atRegOrSimm10)
			cp = prtSimm10(cp, bits);
		else if (fmt == atRegOrSimm11)
			cp = prtSimm11(cp, bits);
		else
			cp = prtSimm13(cp, bits);
	} else {
		/* I = 0: r[rs2] */
		cp = prtReg(cp, bits);
	}
	return (cp);
}


/*
 * checkLo -
 *	use heuristics to check whether to print the immediate field out as
 *	%lo(value).
 */
static
checkLo(fmt, pBits, bits)
	register FORMAT		fmt;	/* format token that got us here */
	register Instruction	pBits;	/* Bits of previous instruction */
	register Instruction	bits;	/* Bits of current instruction */
{
	u_int simm_mask = SIMM13;

	switch (fmt) {

	case atRegOrSimm11:
		simm_mask = SIMM11;
		break;

	case atRegOrSimm:
		if ((bits & OPOP3MSK) == (OP(2) + OP3(0))) { /* add */
			break;
		}
		return (0);

	case atRegOrUimm:
		if ((bits & OPOP3MSK) == (OP(2) + OP3(2))) { /* or */
			break;
		}
		return (0);

	case atAddressOrImm:
		return(0);
	
	case atAddress:
	case atToAddress:
			break;

	default:
		return (0);
	}
	/*
	 * We've qualified the instruction, now lets check the operands
	 */

	if ((bits & (IMSK + (simm_mask & ~LOBITS))) == I(1)) {
		if ((bits & RS1MSK) != RS1(0)) {
			if ((pBits & (OPMSK + OP2MSK)) == (OP(0) + OP2(4))) {
				if (((pBits & RDMSK) >> RD_SHIFT_CT) ==
					((bits & RS1MSK) >> RS1_SHIFT_CT)) {
						return (1);
				}
			}
		}
	}
	return (0);
}


/*
 * prtLo -
 *	print effective address of a register-or-immediate instruction.
 *	This can take one of the following forms:
 *		%rs1
 *		%rs1 + %rs2
 */
static char *
prtLo(cp, pBits, bits, prtAddress)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
	register Instruction pBits;	/* Bits of previous instruction */
	FUNCPTR prtAddress;		/* Routine to print addresses */
{
	if (combuf_flag)
		sprintf(combuf, "\t! %s",prtAddress(
                        ((pBits & IMM22)<<IMM22_SHIFT_CT) | (bits & LOBITS)));
	cp = dis_print(cp, "%s", prtAddress( 
               (((pBits & IMM22)<<IMM22_SHIFT_CT) | (bits & LOBITS)) & 0x3ff));
	return (cp);
}


/*
 * prtRegAddr -
 *	print effective address of a register-or-immediate instruction.
 *	This can take one of the following forms:
 *		%rs1
 *		%rs1 + %rs2
 */
static char *
prtRegAddr(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	if ((bits & IMSK) != 0) {
		dsmError();
		return (cp);
	}
	cp = prtReg(cp, bits >> RS1_SHIFT_CT);
	if ((bits & RS2MSK) != RS2(0)) {
		cp = dis_print(cp, plusStr);
		cp = prtReg(cp, bits);
	}
	return (cp);
}


/*
 * prtSimm13 - print sign-extended simm13 field as positive or negative hex
 *		number.
 */
static char *
prtSimm13(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	int seval = sign_extend13(bits);
	int small = (seval < 10 && seval > -10) ? 1 : 0;

	if ((bits & SIMM13_SIGN) == SIMM13_SIGN)
		cp = dis_print(cp, "-");
	cp = dis_print(cp, "%d", abs(seval));

	return (cp);
}


/*
 * prtConst22 - print the Const22 field.
 */
static char *
prtConst22(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	cp = dis_print(cp, "0x%x", (bits & CONST22));
	return (cp);
}


/*
 * prtDisp22 - print effective address of a pc-relative disp22 field.
 */
static char *
prtDisp22(cp, addr, bits, prtAddress)
	register char *cp;
	unsigned long int addr;		/* Address at which instr'n resides */
	register Instruction bits;	/* Bits of current instruction */
	FUNCPTR		  prtAddress;	/* Routine to print addresses */
{

	if ((bits & DISP22_SIGN) != 0)
		cp = dis_print(cp, "%s", prtAddress(addr +
			(((bits & DISP22) |
				DISP22_SIGN_EXTEND) << DISP22_SHIFT_CT)));
	else
		cp = dis_print(cp, "%s", prtAddress(addr +
				((bits & DISP22) << DISP22_SHIFT_CT)));
	return (cp);
}

/*
 * prtOpfc - print the opfc field.
 */
static char *
prtOpfc(cp, bits)
	register Instruction bits;	/* Bits of current instruction */
{
	return (dis_print(cp, "%03x", ((bits & OPFCMSK) >> OPFC_SHIFT_CT)));
}


/*
 * prtAsi - print the Asi field.
 */
static char *
prtAsi(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	if ((bits & OPOP3MSK) == (OP(2) + OP3(0x37)) &
	    (IMM_ASI(bits) & 0xC0)) { 		/* stda partial store*/
		cp = prtReg(cp, bits);
		cp = dis_print(cp, commaStr);
	}
	return(dis_print(cp, "0x%02x", ((bits & ASIMSK) >> ASI_SHIFT_CT)));
}


/*
 * prtReg - print name of register.
 */
static char *
prtReg(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	return (dis_print(cp, "%s", iureg[ (bits & RS2MSK) ]));
}


/*
 * prtFreg - print name of floating point register.
 */
static char *
prtFreg(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	return (dis_print(cp, "%%f%d", (bits & RS2MSK)));
}


/*
 * prtDreg - print name of floating point d register.
 */
static char *
prtDreg(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_short reg = bits & RS2MSK;

	if (reg % 2 != 0)
		reg = (reg + 32) & DBLMSK;
	return (dis_print(cp, "%%f%d", reg));
}


/*
 * prtQreg - print name of floating point q register.
 */
static char *
prtQreg(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_short reg = bits & RS2MSK;

	if (reg % 4 != 0)
		reg = (reg + 32) & DBLMSK;
	return (dis_print(cp, "%%f%d", reg));
}


/*
 * prtCreg - print name of coprocessor register.
 */
static char *
prtCreg(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	return (dis_print(cp, "%%c%d", (bits & RS2MSK)));
}


/*
 * prtSpec - print name of coprocessor register.
 */
static char *
prtSpec(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{

	switch (bits & (OPOP3MSK)) {

	case (OP(2) + OP3(0x28)):			/* rd %y or %asr[rs1] */
		if ((bits & RS1MSK) == RS1(0)) {
			cp = dis_print(cp, "%%y");
		} else {
			cp = dis_print(cp, "%%asr%d",
				((bits & RS1MSK) >> RS1_SHIFT_CT));
		}
		break;

	case (OP(2) + OP3(0x30)):			/* wr %y or %asr[rd] */
		if ((bits & RDMSK) == RD(0)) {
			cp = dis_print(cp, "%%y");
		} else {
			cp = dis_print(cp, "%%asr%d",
				((bits & RDMSK) >> RD_SHIFT_CT));
		}
		break;

	case (OP(2) + OP3(0x29)):			/* rd %psr */
	case (OP(2) + OP3(0x31)):			/* wr %psr */
		cp = dis_print(cp, "%%psr");
		break;

	case (OP(2) + OP3(0x2A)):			/* rd %wim */
	case (OP(2) + OP3(0x32)):			/* wr %wim */
		cp = dis_print(cp, "%%wim");
		break;

	case (OP(2) + OP3(0x2B)):			/* rd %tbr */
	case (OP(2) + OP3(0x33)):			/* wr %tbr */
		cp = dis_print(cp, "%%tbr");
		break;

	case (OP(3) + OP3(0x21)):			/* ld %fsr */
	case (OP(3) + OP3(0x25)):			/* st %fsr */
		cp = dis_print(cp, "%%fsr");
		break;

	case (OP(3) + OP3(0x26)):			/* std %fq */
		cp = dis_print(cp, "%%fq");
		break;

	case (OP(3) + OP3(0x31)):			/* ld %csr */
	case (OP(3) + OP3(0x35)):			/* st %csr */
		cp = dis_print(cp, "%%csr");
		break;

	case (OP(3) + OP3(0x36)):			/* std %cq */
		cp = dis_print(cp, "%%cq");
		break;

	default:
		dsmError();
		return (0);
	}
	return (cp);
}

/*
 * prtDisp30 - print effective address of a pc-relative disp30 field.
 */
static char *
prtDisp30(cp, addr, bits, prtAddress)
	register char *cp;
	unsigned long int addr;	/* Address at which instr'n resides */
	register Instruction  bits;	/* Bits of current instruction */
	FUNCPTR		  prtAddress;	/* Routine to print addresses */
{
	return (dis_print(cp, "%s", prtAddress(addr +
					((bits & DISP30) << DISP30_SHIFT_CT))));
}


/*
 * prtCond4 - print name of integer or floating condition code.
 */
static char *
prtCond4(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_int cc;

	if ((bits & OP3MSK) ==  OP3(0x2C)) {	/* MOVcc */
		if ((bits & CC2_4MSK) == CC2_4(1))	/* integer */
			cp = prtICond4(cp, bits);
		else					/* floating */
	        	cp = prtFCond4(cp, bits);
	} else {				/* FMOVcc */
		if (bits & IXCC_4MSK)			/* integer */
			cp = prtICond4(cp, bits);
		else					/* floating */
	        	cp = prtFCond4(cp, bits);
	}
	return(cp);
}

/*
 * prtCc - print name of integer or floating condition code.
 */
static char *
prtCc(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_int cc;

	switch (bits & OPMSK) {

	case OP(0):
		cc = (bits & CC01_2MSK) >> CC01_2SHIFT_CT;
		if ((bits & OP2MSK) == OP2(1)) {	/* BPcc */
			cp = prtixcc(cp, cc);
		} else {				/* FBPfcc */
			cp = prtfcc(cp, cc);
		}
		break;

	case OP(2):
		switch (bits & OP3MSK) {

		case OP3(0x2C):				/* MOVcc */
			cc = (bits & CC01_4MSK) >> CC01_4SHIFT_CT;
			if ((bits & CC2_4MSK) == CC2_4(1)) {	/* integer */
				cp = prtixcc(cp, cc);
			} else {				/* floating */
				cp = prtfcc(cp, cc);
			}
			break;

		case OP3(0x35):				/* FMOVcc */
			cc = (bits & OPF_CCMSK) >> CC01_4SHIFT_CT;
			if (bits & IXCC_4MSK) {			/* integer */
				cp = prtixcc(cp, cc);
			} else {				/* floating */
				cp = prtfcc(cp, cc);
			}
			break;

		case OP3(0x3A):				/* Tcc */
			cc = (bits & CC01_4MSK) >> CC01_4SHIFT_CT;
			cp = prtixcc(cp, cc);
			break;
		}
		break;

	default:
		break;

	}
	return (cp);
}


/*
 * prtixcc - print name of integer condition code.
 */
static char *
prtixcc(cp, ixcc)
	register char *cp;
	register u_int ixcc;
{
	if ((ixcc & CCMSK) == CC(0))
		cp = dis_print(cp, "%%icc,");
	else
		cp = dis_print(cp, "%%xcc,");
	return(cp);
}


/*
 * prtfcc - print name of floating condition code.
 */
static char *
prtfcc(cp, fcc)
	register char *cp;
	register u_int fcc;
{
	return(dis_print(cp, "%%fcc%d, ", (fcc & CCMSK)));
}


/*
 * prtFCond4 - print end of name of floating instruction.
 */
static char *
prtFCond4(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	switch(bits & COND4MSK) {

	case COND4(0):
		cp = dis_print(cp, "n ");
		break;

	case COND4(1):
		cp = dis_print(cp, "ne ");	/* alias nz */
		break;

	case COND4(2):
		cp = dis_print(cp, "lg ");
		break;

	case COND4(3):
		cp = dis_print(cp, "ul ");
		break;

	case COND4(4):
		cp = dis_print(cp, "l ");
		break;

	case COND4(5):
		cp = dis_print(cp, "ug ");
		break;

	case COND4(6):
		cp = dis_print(cp, "g ");
		break;

	case COND4(7):
		cp = dis_print(cp, "u ");
		break;

	case COND4(8):
		cp = dis_print(cp, "a ");
		break;

	case COND4(9):
		cp = dis_print(cp, "e ");	/* alias z */
		break;

	case COND4(0xA):
		cp = dis_print(cp, "ue ");
		break;

	case COND4(0xB):
		cp = dis_print(cp, "ge ");
		break;

	case COND4(0xC):
		cp = dis_print(cp, "uge ");
		break;

	case COND4(0xD):
		cp = dis_print(cp, "le ");
		break;

	case COND4(0xE):
		cp = dis_print(cp, "ule ");
		break;

	case COND4(0xF):
		cp = dis_print(cp, "o ");
		break;

	default:
		break;

	}
	return (cp);
}


/*
 * prtICond4 - print end of name of integer instruction.
 */
static char *
prtICond4(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	switch(bits & COND4MSK) {

	case COND4(0):
		cp = dis_print(cp, "n ");
		break;

	case COND4(1):
		cp = dis_print(cp, "e ");	/* alias z */
		break;

	case COND4(2):
		cp = dis_print(cp, "le ");
		break;

	case COND4(3):
		cp = dis_print(cp, "l ");
		break;

	case COND4(4):
		cp = dis_print(cp, "leu ");
		break;

	case COND4(5):
		cp = dis_print(cp, "cs ");	/* alias lu */
		break;

	case COND4(6):
		cp = dis_print(cp, "neg ");
		break;

	case COND4(7):
		cp = dis_print(cp, "vs ");
		break;

	case COND4(8):
		cp = dis_print(cp, "a ");
		break;

	case COND4(9):
		cp = dis_print(cp, "ne ");	/* alias nz */
		break;

	case COND4(0xA):
		cp = dis_print(cp, "g ");
		break;

	case COND4(0xB):
		cp = dis_print(cp, "ge ");
		break;

	case COND4(0xC):
		cp = dis_print(cp, "gu ");
		break;

	case COND4(0xD):
		cp = dis_print(cp, "cc ");	/* alias geu */
		break;

	case COND4(0xE):
		cp = dis_print(cp, "pos ");
		break;

	case COND4(0xF):
		cp = dis_print(cp, "vc ");
		break;

	default:
		break;

	}
	return (cp);
}


/*
 * prtRFCond3 - print end of name of floating instruction.
 */
static char *
prtRFCond3(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	switch(bits & RCOND3MSK) {

	case RCOND3(1):
		cp = dis_print(cp, "e ");	/* alias z */
		break;

	case RCOND3(2):
		cp = dis_print(cp, "lez ");
		break;

	case RCOND3(3):
		cp = dis_print(cp, "lz ");
		break;

	case RCOND3(5):
		cp = dis_print(cp, "ne ");
		break;

	case RCOND3(6):
		cp = dis_print(cp, "gz ");
		break;

	case RCOND3(7):
		cp = dis_print(cp, "gez ");
		break;

	default:
		break;

	}
	return (cp);
}


/*
 * prtCmask - print name of constraint mask(s).
 */
static char *
prtCmask(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_int mask;

	mask = bits & CMSKMSK;
	if (mask & CMSK(1))
		cp = dis_print(cp, "#Lookaside");
	if (mask & CMSK(2))
		cp = dis_print(cp, "#MemIssue");
	if (mask & CMSK(4))
		cp = dis_print(cp, "#Sync");
	return (cp);
}


/*
 * prtMmask - print name of memory reference mask(s).
 */
static char *
prtMmask(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_int mask;

	mask = bits & MMSKMSK;
	if (mask & MMSK(1))
		cp = dis_print(cp, "#LoadLoad");
	if (mask & MMSK(2))
		cp = dis_print(cp, "#StoreLoad");
	if (mask & MMSK(4))
		cp = dis_print(cp, "#LoadStore");
	if (mask & MMSK(8))
		cp = dis_print(cp, "#StoreStore");
	return (cp);
}


/*
 * prtV9Spec - print name of V9 special register.
 */
static char *
prtV9Spec(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	u_int specreg;

	switch (bits & (OPOP3MSK)) {

	case (OP(2) + OP3(0x28)):		/* rd %y, %asr[rs1], etc. */
	case (OP(2) + OP3(0x30)):		/* wr %y, %asr[rd], etc. */

		if ((bits & OP3MSK) == OP3(0x28))
			specreg = ((bits & RS1MSK) >> RS1_SHIFT_CT);
		else
			specreg = ((bits & RDMSK) >> RD_SHIFT_CT);

		switch (specreg) {

		case 0:
			cp = dis_print(cp, "%%y");
			break;

		case 2:
			cp = dis_print(cp, "%%ccr");
			break;

		case 3:
			cp = dis_print(cp, "%%asi");
			break;

		case 4:
			cp = dis_print(cp, "%%tick");
			break;

		case 5:
			cp = dis_print(cp, "%%pc");
			break;

		case 6:
			cp = dis_print(cp, "%%fprs");
			break;

		case 16:
			cp = dis_print(cp, "%%pcr");
			break;

		case 17:
			cp = dis_print(cp, "%%pic");
			break;

		case 18:
			cp = dis_print(cp, "%%dcr");
			break;

		case 19:
			cp = dis_print(cp, "%%gsr");
			break;

		case 20:
			cp = dis_print(cp, "%%set_softint");
			break;

		case 21:
			cp = dis_print(cp, "%%clear_softint");
			break;

		case 22:
			cp = dis_print(cp, "%%softint");
			break;

		case 23:
			cp = dis_print(cp, "%%tick_cmpr");
			break;

		default:
			cp = dis_print(cp, "%%asr%d", (specreg));
			break;
		}

		break;

	case (OP(2) + OP3(0x2A)):		/* rdpr %tpc, %tnpc, etc. */
	case (OP(2) + OP3(0x32)):		/* wrpr %tpc, %tnpc, etc. */

		if ((bits & OP3MSK) == OP3(0x2A))
			specreg = ((bits & RS1MSK) >> RS1_SHIFT_CT);
		else
			specreg = ((bits & RDMSK) >> RD_SHIFT_CT);

		switch (specreg) {

		case (0):
			cp = dis_print(cp, "%%tpc");
			break;

		case (1):
			cp = dis_print(cp, "%%tnpc");
			break;

		case (2):
			cp = dis_print(cp, "%%tstate");
			break;

		case (3):
			cp = dis_print(cp, "%%tt");
			break;

		case (4):
			cp = dis_print(cp, "%%tick");
			break;

		case (5):
			cp = dis_print(cp, "%%tba");
			break;

		case (6):
			cp = dis_print(cp, "%%pstate");
			break;

		case (7):
			cp = dis_print(cp, "%%tl");
			break;

		case (8):
			cp = dis_print(cp, "%%pil");
			break;

		case (9):
			cp = dis_print(cp, "%%cwp");
			break;

		case (0xA):
			cp = dis_print(cp, "%%cansave");
			break;

		case (0xB):
			cp = dis_print(cp, "%%canrestore");
			break;

		case (0xC):
			cp = dis_print(cp, "%%cleanwin");
			break;

		case (0xD):
			cp = dis_print(cp, "%%otherwin");
			break;

		case (0xE):
			cp = dis_print(cp, "%%wstate");
			break;

		case (0xF):
			cp = dis_print(cp, "%%fq");
			break;

		case (0x1F):
			cp = dis_print(cp, "%%ver");
			break;

		default:
			dsmError();
			return(0);
		}

		break;

	default:
		dsmError();
		return(0);

	}
	return (cp);
}


/*
 * prtShcnt64 - printf shcnt64 field
 */
static char *
prtShcnt64(cp, bits)
	register char *cp;
	register Instruction	bits;	/* Bits of current instruction */
{
	cp = dis_print(cp, "%d", (bits & SHCNT64MSK));
	return (cp);
}


/*
 * prtTrap - printf software_trap# field
 */
static char *
prtTrap(cp, bits)
	register char *cp;
	register Instruction	bits;	/* Bits of current instruction */
{
	cp = dis_print(cp, "%d", (bits & TRAPMSK));
	return (cp);
}


/*
 * prtSimm10 - print sign-extended simm11 field as positive or negative hex
 *		number.
 */
static char *
prtSimm10(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	int seval = sign_extend10(bits);
	int small = (seval < 10 && seval > -10) ? 1 : 0;

	if ((bits & SIMM10_SIGN) == SIMM10_SIGN)
		cp = dis_print(cp, "-");
	cp = dis_print(cp, "0x%x", abs(seval));
	return (cp);
}


/*
 * prtSimm11 - print sign-extended simm11 field as positive or negative hex
 *		number.
 */
static char *
prtSimm11(cp, bits)
	register char *cp;
	register Instruction bits;	/* Bits of current instruction */
{
	int seval = sign_extend11(bits);
	int small = (seval < 10 && seval > -10) ? 1 : 0;

	if ((bits & SIMM11_SIGN) == SIMM11_SIGN)
		cp = dis_print(cp, "-");
	cp = dis_print(cp, "0x%x", abs(seval));
	return (cp);
}


/*
 * prtDisp16 - print effective address of a pc-relative disp16 field.
 */
static char *
prtDisp16(cp, addr, bits, prtAddress)
	register char *cp;
	unsigned long int addr;		/* Address at which instr'n resides */
	register Instruction bits;	/* Bits of current instruction */
	FUNCPTR		  prtAddress;	/* Routine to print addresses */
{
	u_int disp16;

	disp16 = ((bits & D16HIMSK) >> D16HI_SHIFT_CT) | (bits & D16LOMSK);
	if ((disp16 & DISP16_SIGN) != 0)
		cp = dis_print(cp, "%s", prtAddress(addr +
			(((disp16 & DISP16) |
				DISP16_SIGN_EXTEND) << DISP16_SHIFT_CT)));
	else
		cp = dis_print(cp, "%s", prtAddress(addr +
				((disp16 & DISP16) << DISP16_SHIFT_CT)));
	return (cp);
}


/*
 * prtDisp19 - print effective address of a pc-relative disp19 field.
 */
static char *
prtDisp19(cp, addr, bits, prtAddress)
	register char *cp;
	unsigned long int addr;		/* Address at which instr'n resides */
	register Instruction bits;	/* Bits of current instruction */
	FUNCPTR		  prtAddress;	/* Routine to print addresses */
{

	if ((bits & DISP19_SIGN) != 0)
		cp = dis_print(cp, "%s", prtAddress(addr +
			(((bits & DISP19) |
				DISP19_SIGN_EXTEND) << DISP19_SHIFT_CT)));
	else
		cp = dis_print(cp, "%s", prtAddress(addr +
				((bits & DISP19) << DISP19_SHIFT_CT)));
	return (cp);
}

