/*
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

#ident	"@(#)sr_instruction.h	1.10	94/12/06 SMI"
/* From Will Brown's "sas" Sparc Architectural Simulator,
 * Version "@:-)instruction.h	3.1 11/11/85
 */

/* instruction.h renamed to sr_instruction.h
	Instruction formats, and some FAB1 -> FAB2 differences.
*/

/* All formats are 32 bits. All instructions fit into an unsigned integer */

/* General 32-bit sign extension macro */
/* The (long) cast is necessary in case we're given unsigned arguments */
#define SR_SEXT(sex,size) ((((long)(sex)) << (32 - size)) >> (32 - size))

/* Change a word address to a byte address (call & branches need this) */
#define SR_WA2BA(woff) ((woff) << 2)

/* Format 1: CALL instruction only */

/* ifdef is used to provide machine independence */

/* Instruction Field Width definitions */
#define IFW_OP		2
#define IFW_DISP30	30

#ifdef LOW_END_FIRST

struct fmt1Struct {
	u_int disp30		:IFW_DISP30;	/* PC relative, word aligned,
								disp. */
	u_int op		:IFW_OP;	/* op field = 00 for format 1 */
};

#else

struct fmt1Struct {
	u_int op		:IFW_OP;	/* op field = 00 for format 1 */
	u_int disp30		:IFW_DISP30;	/* PC relative, word aligned,
								disp. */
};

#endif

/* extraction macros. These are portable, but don't work for registers */
/* with sun's C compiler, they are also quite efficient */

#define X_OP(x)		( ((struct fmt1Struct *)&(x))->op )	
#define OP		X_OP(instruction)

#define X_DISP30(x)	( ((struct fmt1Struct *)&(x))->disp30 )
#define DISP30		X_DISP30(instruction)

/* Format 2: SETHI, Bicc, Bfcc */

#define IFW_RD		5
#define IFW_OP2		3
#define IFW_DISP16	16
#define IFW_DISP19	19
#define IFW_DISP22	22

#define SR_JOIN16( hisex, losex ) ((hisex << 14) | losex)
#define SR_SEX16( sex ) SR_SEXT(sex, IFW_DISP16 )
#define SR_SEX19( sex ) SR_SEXT(sex, IFW_DISP19 )
#define SR_SEX22( sex ) SR_SEXT(sex, IFW_DISP22 )
#define SR_HI(disp22) ((disp22) << (32 - IFW_DISP22) )

#ifdef LOW_END_FIRST

struct fmt2Struct {
	u_int disp22		:IFW_DISP22;	/* 22 bit displacement */
	u_int op2		:IFW_OP2;	/* opcode for format 2
							instructions */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 01 for format 2 */
};

#else

struct fmt2Struct {
	u_int op		:IFW_OP;	/* op field = 01 for format 2 */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op2		:IFW_OP2;	/* opcode for format 2
							instructions */
	u_int disp22		:IFW_DISP22;	/* 22 bit displacement */
};

#endif;

#define X_RD(x)		( ((struct fmt2Struct *)&(x))->rd )
#define RD		X_RD(instruction)

#define X_OP2(x)	( ((struct fmt2Struct *)&(x))->op2 )
#define OP2		X_OP2(instruction)

#define X_DISP22(x)	( ((struct fmt2Struct *)&(x))->disp22 )
#define DISP22		X_DISP22(instruction)

/* The rd field of formats 2, 3, and 3I also has another form: */

#define IFW_ANNUL	1
#define IFW_COND	4


#ifdef LOW_END_FIRST

struct condStruct {
	u_int disp22		:IFW_DISP22;	/* use format 2 definition */
	u_int op2		:IFW_OP2;	/* because it is simplest */
	u_int cond		:IFW_COND;	/* cond field */
	u_int annul		:IFW_ANNUL;	/* annul field for BICC,
								and BFCC */
	u_int op		:IFW_OP;	/* common to all instructions */
};

#else

struct condStruct {
	u_int op		:IFW_OP;
	u_int annul		:IFW_ANNUL;	/* annul field for BICC,
								and BFCC */
	u_int cond		:IFW_COND;	/* cond field */
	u_int op2		:IFW_OP2;
	u_int disp22		:IFW_DISP22;
};

#endif

#define X_ANNUL(x)	( ((struct condStruct *)&(x))->annul )
#define ANNUL		X_ANNUL(instruction)

#define X_COND(x)	( ((struct condStruct *)&(x))->cond )
#define COND		X_COND(instruction)


/* The rd field of formats 2 has another form for V9: */

#define IFW_PREDICT	1
#define IFW_CC0		1
#define IFW_CC1		1


#ifdef LOW_END_FIRST

struct pcondStruct {
	u_int disp19		:IFW_DISP19;	/* use format 2 definition */
	u_int predict		:IFW_PREDICT;	/* V9 predict field */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int op2		:IFW_OP2;	/* because it is simplest */
	u_int cond		:IFW_COND;	/* cond field */
	u_int annul		:IFW_ANNUL;	/* annul field */
	u_int op		:IFW_OP;	/* common to all instructions */
};

#else

struct pcondStruct {
	u_int op		:IFW_OP;
	u_int annul		:IFW_ANNUL;	/* annul field */
	u_int cond		:IFW_COND;	/* cond field */
	u_int op2		:IFW_OP2;
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int predict		:IFW_PREDICT;	/* V9 predict field */
	u_int disp19		:IFW_DISP19;
};

#endif

#define X_DISP19(x)	( ((struct pcondStruct *)&(x))->disp19 )
#define DISP19		X_DISP19(instruction)

#define X_PREDICT(x)	( ((struct pcondStruct *)&(x))->predict )
#define PREDICT		X_PREDICT(instruction)

#define X_CC0(x)	( ((struct pcondStruct *)&(x))->cc0 )
#define CC0		X_CC0(instruction)

#define X_CC1(x)	( ((struct pcondStruct *)&(x))->cc1 )
#define CC1		X_CC1(instruction)

/* The rd field of formats 2 and 3 also has yet another form for V9: */

#define IFW_D16LO	14
#define IFW_D16HI	2
#define IFW_RS1		5
#define IFW_RCOND	3
#define IFW_ZERO	1


#ifdef LOW_END_FIRST

struct rcondStruct {
	u_int d16lo		:IFW_D16LO;	/* low 14 bits of address */
	u_int rs1		:IFW_RS1;	/* which register to check */
	u_int predict		:IFW_PREDICT;	/* predict field */
	u_int d16hi		:IFW_D16HI;	/* high 16 bits of address */
	u_int op2		:IFW_OP2;	/* because it is simplest */
	u_int rcond		:IFW_RCOND;	/* 3 bit cond field */
	u_int zero		:IFW_ZERO;	/* always 0 */
	u_int annul		:IFW_ANNUL;	/* annul field for BPr,
							MOVr and FMOVr */
	u_int op		:IFW_OP;	/* common to all instructions */
};

#else

struct rcondStruct {
	u_int op		:IFW_OP;	/* common to all instructions */
	u_int annul		:IFW_ANNUL;	/* annul field for BPr,
							MOVr and FMOVr */
	u_int zero		:IFW_ZERO;	/* always 0 */
	u_int rcond		:IFW_RCOND;	/* 3 bit cond field */
	u_int op2		:IFW_OP2;	/* because it is simplest */
	u_int d16hi		:IFW_D16HI;	/* high 16 bits of address */
	u_int predict		:IFW_PREDICT;	/* predict field */
	u_int rs1		:IFW_RS1;	/* which register to check */
	u_int d16lo		:IFW_D16LO;	/* low 14 bits of address */
};

#endif

#define X_D16LO(x)	( ((struct rcondStruct *)&(x))->d16lo )
#define D16LO		X_D16LO(instruction)

#define X_D16HI(x)	( ((struct rcondStruct *)&(x))->d16hi )
#define D16HI		X_D16HI(instruction)

/* format 3: all other instructions including FPop with a register source 2 */

#define IFW_OP3		6
#define IFW_RS1		5
#define IFW_IMM		1
#define IFW_OPF		8
#define IFW_RS2		5

#ifdef LOW_END_FIRST

struct fmt3Struct {
	u_int rs2		:IFW_RS2;	/* register source 2 */
	u_int opf		:IFW_OPF;	/* floating-point opcode */
	u_int imm		:IFW_IMM;	/* immediate. distinguishes fmt3
						and fmt3I */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
							fmt3 instrns */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 1- for format 3 */
};

#else

struct fmt3Struct {
	u_int op		:IFW_OP;	/* op field = 1- for format 3 */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
							fmt3 instrns */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int imm		:IFW_IMM;	/* immediate. distinguishes fmt3
						and fmt3I */
	u_int opf		:IFW_OPF;	/* floating-point opcode */
	u_int rs2		:IFW_RS2;	/* register source 2 */
};

#endif

#define X_OP3(x)	( ((struct fmt3Struct *)&(x))->op3 )
#define OP3		X_OP3(instruction)

#define X_RS1(x)	( ((struct fmt3Struct *)&(x))->rs1 )
#define RS1		X_RS1(instruction)

#define X_IMM(x)	( ((struct fmt3Struct *)&(x))->imm )
#define IMM		X_IMM(instruction)

#define X_OPF(x)	( ((struct fmt3Struct *)&(x))->opf )
#define OPF		X_OPF(instruction)

#define X_RS2(x)	( ((struct fmt3Struct *)&(x))->rs2 )
#define RS2		X_RS2(instruction)

/* format 3I: all other instructions (except FPop) with immediate source 2 */

#define IFW_SIMM13	13

#define SR_SEX13( sex ) SR_SEXT(sex, IFW_SIMM13 )

#ifdef LOW_END_FIRST

struct fmt3IStruct {
	u_int simm13		:IFW_SIMM13;	/* immediate source 2 */
	u_int imm		:IFW_IMM;	/* immediate.
							distinguishes fmt3
						and fmt3I */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
							fmt3 instrns */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 1- for format
									3I */
};

#else

struct fmt3IStruct {
	u_int op		:IFW_OP;	/* op field = 1- for format
									3I */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
								fmt3 instrns */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int imm		:IFW_IMM;	/* immediate. distinguishes fmt3
						and fmt3I */
	u_int simm13		:IFW_SIMM13;	/* immediate source 2 */
};

#endif

#define X_SIMM13(x)	( ((struct fmt3IStruct *)&(x))->simm13 )
#define SIMM13		X_SIMM13(instruction)

/* format 3S: shift instructions */

#define IFW_SHCNT64	6
#define IFW_RSV6	6
#define IFW_X		1

#ifdef LOW_END_FIRST

struct fmt3SStruct {
	u_int shcnt64		:IFW_SHCNT64;	/* shcnt64 field */
	u_int rsv6		:IFW_RSV6;	/* not used */
	u_int xx		:IFW_X;		/* x distinguishes fmt3 I
						and fmt3S */
	u_int imm		:IFW_IMM;	/* immediate. distinguishes fmt3
						and fmt3I */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
							fmt3 instrns */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 1- for format
									3S */
};

#else

struct fmt3SStruct {
	u_int op		:IFW_OP;	/* op field = 1- for format
									3S */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op3		:IFW_OP3;	/* opcode distinguishing
							fmt3 instrns */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int imm		:IFW_IMM;	/* immediate. distinguishes fmt3
						and fmt3I */
	u_int xx		:IFW_X;		/* x distinguishes fmt3 I
						and fmt3S */
	u_int rsv6		:IFW_RSV6;	/* not used */
	u_int shcnt64		:IFW_SHCNT64;	/* shcnt64 field */
};

#endif

#define X_SHCNT64(x)	( ((struct fmt3SStruct *)&(x))->shcnt64 )
#define SHCNT64		X_SHCNT64(instruction)

#define X_XX(x)		( ((struct fmt3SStruct *)&(x))->xx )
#define XX		X_XX(instruction)

/* format 4: Tcc instructions */

#define IFW_HITRAP	2
#define IFW_RSV4	4
#define IFW_RSV1	1
#define IFW_OP4		6

#ifdef LOW_END_FIRST

struct fmt4Struct {
	u_int rs2		:IFW_RS2;	/* register source 2 */
	u_int hitrap		:IFW_HITRAP;	/* hi bits of software trap# */
	u_int rsv4		:RSV4;		/* not used */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int cond		:IFW_COND;	/* cond field */
	u_int rsv1		:IFW_RSV1;	/* not used */
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
};

#else

struct fmt4Struct {
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
	u_int rsv1		:IFW_RSV1;	/* not used */
	u_int cond		:IFW_COND;	/* cond field */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int rsv4		:IFW_RSV4;	/* not used */
	u_int hitrap		:IFW_HITRAP;	/* hi bits of software trap# */
	u_int rs2		:IFW_RS2;	/* register source 2 */
};

#endif

#define X_HITRAP(x)	( ((struct fmt4Struct *)&(x))->hitrap )
#define HITRAP		X_HITRAP(instruction)

#define X_F4CC0(x)	( ((struct fmt4Struct *)&(x))->cc0 )
#define F4CC0		X_F4CC0(instruction)

#define X_F4CC1(x)	( ((struct fmt4Struct *)&(x))->cc1 )
#define F4CC1		X_F4CC1(instruction)

/* format 4: MOVcc instructions */

#define IFW_CC2		1
#define IFW_SIMM11	11

#define SR_SEX11( sex ) SR_SEXT(sex, IFW_SIMM11 )

#ifdef LOW_END_FIRST

struct fmt4MStruct {
	u_int simm11		:IFW_SIMM11;	/* simm11 field */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int cond		:IFW_COND;	/* cond field */
	u_int cc2		:IFW_CC2;	/* V9 cc2 field */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
};

#else

struct fmt4MStruct {
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int cc2		:IFW_CC2;	/* V9 cc2 field */
	u_int cond		:IFW_COND;	/* cond field */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int cc1		:IFW_CC1;	/* V9 cc1 field */
	u_int cc0		:IFW_CC0;	/* V9 cc0 field */
	u_int simm11		:IFW_SIMM11;	/* simm11 field */

};

#endif

#define X_SIMM11(x)	( ((struct fmt4MStruct *)&(x))->simm11 )
#define SIMM11		X_SIMM11(instruction)

#define X_CC2(x)	( ((struct fmt4MStruct *)&(x))->cc2 )
#define CC2		X_CC2(instruction)

#define X_OP4(x)	( ((struct fmt4MStruct *)&(x))->op4 )
#define OP4		X_OP4(instruction)

/* format 4: FMOVr instructions */

#define IFW_OPF_LOW	5
#define IFW_SIMM10	10

#define SR_SEX10( sex ) SR_SEXT(sex, IFW_SIMM10 )

#ifdef LOW_END_FIRST

struct fmt4RStruct {
	u_int rs2		:IFW_RS2;	/* register source 2 */
	u_int opf_low		:IFW_OPF_LOW;	/* single, double, quad */
	u_int rcond		:IFW_RCOND;	/* 3 bit cond field */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
};

#else

struct fmt4RStruct {
	u_int op		:IFW_OP;	/* op field = 10- for format
									4 */
	u_int rd		:IFW_RD;	/* destination register */
	u_int op4		:IFW_OP4;	/* opcode distinguishing
							fmt4 instrns */
	u_int rs1		:IFW_RS1;	/* register source 1 */
	u_int imm		:IFW_IMM;	/* immediate */
	u_int rcond		:IFW_RCOND;	/* 3 bit cond field */
	u_int opf_low		:IFW_OPF_LOW;	/* single, double, quad */
	u_int rs2		:IFW_RS2;	/* register source 2 */
};

#endif

#define X_RCOND(x)	( ((struct fmt4RStruct *)&(x))->rcond )
#define RCOND		X_RCOND(instruction)

#define X_OPF_LOW(x)	( ((struct fmt4RStruct *)&(x))->opf_low )
#define OPF_LOW		X_OPF_LOW(instruction)

/* ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** ** */
/*
 * #Defined constants for some of the operations
 * that the debuggers need to know about.
 */
#define SR_UNIMP_OP             0
#define SR_BPCC_OP              1
#define SR_BICC_OP              2
#define SR_BPR_OP               3
#define SR_SETHI_OP             4
#define SR_FBPCC_OP             5
#define SR_FBCC_OP              6
#define SR_OR_OP 	        2
