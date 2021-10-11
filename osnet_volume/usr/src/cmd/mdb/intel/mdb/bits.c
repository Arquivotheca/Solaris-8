/*
 * Copyright (c) 1988 AT&T
 * All rights reserved.
 *
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 *
 * Copyright (c) 1991-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bits.c	1.2	99/11/19 SMI"

#include	"dis.h"
/*
 * set to 1 before call to displacement() when
 * a signed decimal displacement is desired,
 * as opposed to a hexadecimal one
 */
static int signed_disp;
/* keep track of relative jumps */
static int reljmp;
/*
 * maximum length of a single operand
 * (will be used for printing)
 */
#define	OPLEN	256

static	char	operand[3][OPLEN];	/* to store operands as they	*/
					/* are encountered		*/
static char *overreg;	/* save the segment override register if any    */
static int data16;	/* 16- or 32-bit data */
static int addr16;	/* 16- or 32-bit addressing */

#define	WBIT(x)	(x & 0x1)		/* to get w bit	*/
#define	REGNO(x) (x & 0x7)		/* to get 3 bit register */
#define	VBIT(x)	((x)>>1 & 0x1)		/* to get 'v' bit */
#define	OPSIZE(data16, wbit) ((wbit) ? ((data16) ? 2:4) : 1)

#define	REG_ONLY 3	/* mode indicates a single register with	*/
			/* no displacement is an operand		*/
#define	LONGOPERAND 1	/* value of the w-bit indicating a long	*/
				/* operand (2-bytes or 4-bytes)		*/
#define	MMXOPERAND 2	/* value of the w-bit indicating a mmx reg	*/

static uchar_t curbyte;	/* result of getbyte() */
static char mneu[256];	/* array to store disassembly for return */
/*
 * function prototypes
 */
static void getbyte(void);
static void get_modrm_byte(unsigned *, unsigned *, unsigned *);
static void check_override(int);
static void imm_data(int, int);
static void get_opcode(unsigned *, unsigned *);
static void bad_opcode(void);
static void displacement(int, int, long *);
static void get_operand(unsigned, unsigned, int, int);
static void getbytes(int, char *, long *);
static void compoff(long);

/*
 *	disassemble an instruction
 */

static void
disasm()
{
	/* the following arrays are contained in tables.c	*/
	extern	struct	instable	distable[16][16];
	extern	struct	instable	op0F[16][16];
	extern	struct	instable	opFP1n2[8][8];
	extern	struct	instable	opFP3[8][8];
	extern	struct	instable	opFP4[4][8];
	extern	struct	instable	opFP5[8];
	extern	struct	instable	opFP6[8];
	extern	struct	instable	opFP7[8];
	extern	struct	instable	opFC8[4];
	extern  struct  instable	opFP7123[4][4];

	extern	char	*REG16[8][2];
	extern	char	*REG32[8][2];
	extern	char	*SEGREG[6];
	extern	char	*DEBUGREG[8];
	extern	char	*CONTROLREG[8];
	extern	char	*TESTREG[8];
	extern	char	*MMXREG[8];
	struct instable *dp;
	int wbit, vbit;
	unsigned mode, reg, r_m;
	/* nibbles of the opcode */
	unsigned opcode1, opcode2, opcode3, opcode4, opcode5, opcode6, opcode7;
	long	lngval;
	char *reg_name;
	int got_modrm_byte;
	char mnemonic[OPLEN];
	/* number of bytes of opcode - used to get wbit */
	char opcode_bytes;

	/*
	 * The code from here to the end of the function
	 * is indented by an extra tab because this code
	 * was stolen from usr/src/cmd/sgs/dis/i386/bits.c
	 * and hacked over a lot.  The original code here
	 * was in a loop that has been deleted.  We leave
	 * the tabs in so we can do diffs sometime in the
	 * future when the sgs version is updated.
	 */

		mnemonic[0] = '\0';
		mneu[0] = '\0';
		operand[0][0] = '\0';
		operand[1][0] = '\0';
		operand[2][0] = '\0';
		addr16 = 0;
		data16 = 0;
		overreg = NULL;
		opcode_bytes = 0;

		/*
		** As long as there is a prefix, the default segment register,
		** addressing-mode, or data-mode in the instruction
		** will be overridden.
		** This may be more general than the chip actually is.
		*/
		for (;;) {
			get_opcode(&opcode1, &opcode2);
			dp = &distable[opcode1][opcode2];

			if (dp->adr_mode == PREFIX)
				(void) strcat(mnemonic, dp->name);
			else if (dp->adr_mode == AM)
				addr16 = !addr16;
			else if (dp->adr_mode == DM)
				data16 = !data16;
			else if (dp->adr_mode == OVERRIDE)
				overreg = dp->name;
			else
				break;
		}
		/*
		 * some 386 instructions have 2 bytes of opcode
		 * before the mod_r/m
		 * byte so we need to perform a table indirection.
		 */
		if (dp->indirect == (struct instable *)op0F) {
			get_opcode(&opcode4, &opcode5);
			opcode_bytes = 2;
			if ((opcode4 == 0x7) &&
				    ((opcode5 >= 0x1) && (opcode5 <= 0x3))) {
				get_opcode(&opcode6, &opcode7);
				opcode_bytes = 3;
				dp = &opFP7123[opcode5][opcode6 & 0x3];
			} else if ((opcode4 == 0xc) && (opcode5 >= 0x8))
				dp = &opFC8[0];
			else
				dp = &op0F[opcode4][opcode5];
		}

		got_modrm_byte = 0;
		if (dp->indirect != TERM) {
			/*
			 * This must have been an opcode for which several
			 * instructions exist.  The opcode3 field further
			 * decodes the instruction.
			 */
			got_modrm_byte = 1;
			get_modrm_byte(&mode, &opcode3, &r_m);
			/*
			 * decode 287 instructions (D8-DF) from opcodeN
			 */
			if (opcode1 == 0xD && opcode2 >= 0x8) {
				/* instruction form 5 */
				if (opcode2 == 0xB && mode == 0x3 &&
				    opcode3 == 4)
					dp = &opFP5[r_m];
				else if (opcode2 == 0xA && mode == 0x3 &&
				    opcode3 < 4)
					dp = &opFP7[opcode3];
				else if (opcode2 == 0xB && mode == 0x3)
					dp = &opFP6[opcode3];
				/* instruction form 4 */
				else if (opcode2 == 0x9 && mode == 0x3 &&
				    opcode3 >= 4)
					dp = &opFP4[opcode3-4][r_m];
				/* instruction form 3 */
				else if (mode == 0x3)
					dp = &opFP3[opcode2-8][opcode3];
				/* instruction form 1 and 2 */
				else dp = &opFP1n2[opcode2-8][opcode3];
			}
			else
				dp = dp -> indirect + opcode3;
			/* now dp points the proper subdecode table entry */
		}

		if (dp->indirect != TERM) {
			bad_opcode();
			return;
		}

		/* print the mnemonic */
		if (dp->adr_mode != CBW && dp->adr_mode != CWD) {
			/* print the mnemonic */
			(void) strcat(mnemonic, dp -> name);
			if (dp->suffix)
				(void) strcat(mnemonic, (data16? "w" : "l"));
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    (strlen(mnemonic) < (unsigned int)8 ?
				"%-8s" : "%-8s "), mnemonic);
		}


		/*
		 * Each instruction has a particular instruction syntax format
		 * stored in the disassembly tables.  The assignment of formats
		 * to instructins was made by the author.  Individual formats
		 * are explained as they are encountered in the following
		 * switch construct.
		 */

		switch (dp -> adr_mode) {
			/*
			 * movsbl movsbw (0x0FBE) or movswl (0x0FBF)
			 * movzbl movzbw (0x0FB6) or mobzwl (0x0FB7)
			 * wbit lives in 2nd byte, note that operands
			 * are different sized
			 */
		case MOVZ:
			/*
			 * Get second operand first so data16 can be destroyed
			 */
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];

			wbit = WBIT(opcode5);
			data16 = 1;
			get_operand(mode, r_m, wbit, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* imul instruction, with either 8-bit or longer immediate */
		case IMUL:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 1);
			/*
			 * opcode 0x6B for byte, sign-extended
			 * displacement, 0x69 for word(s)
			 */
			imm_data(OPSIZE(data16, opcode2 == 0x9), 0);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s,%s", mneu, operand[0], operand[1],
			    reg_name);
			return;

		/* memory or register operand to register, with 'w' bit	*/
		case MRw:
			wbit = WBIT(opcode2);
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, wbit, 0);
			if (data16)
				reg_name = REG16[reg][wbit];
			else
				reg_name = REG32[reg][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* register to memory or register operand, with 'w' bit	*/
		/* arpl happens to fit here also because it is odd */
		case RMw:
			if (opcode_bytes == 2)
				wbit = WBIT(opcode5);
			else
				wbit = WBIT(opcode2);
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, wbit, 0);
			if (data16)
				reg_name = REG16[reg][wbit];
			else
				reg_name = REG32[reg][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

		/* special case for the Pentium xaddb instruction */
		case XADDB:
			wbit = 0;
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, wbit, 0);
			if (data16)
				reg_name = REG16[reg][wbit];
			else
				reg_name = REG32[reg][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

		/* MMX register to memory or register operand		*/
		case MMXS:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
			    MMXOPERAND : LONGOPERAND, 0);
			reg_name = MMXREG[reg];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

		/* Double shift. Has immediate operand specifying the shift. */
		case DSHIFT:
			get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 1);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			imm_data(1, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s,%s", mneu, operand[0], reg_name,
			    operand[1]);
			return;

			/*
			 * Double shift. With no immediate operand,
			 * specifies using %cl.
			 */
		case DSHIFTcl:
			get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

			/* immediate to memory or register operand */
		case IMlw:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 1);
			/*
			 * A long immediate is expected for opcode 0x81,
			 * not 0x80 nor 0x83
			 */
			imm_data(OPSIZE(data16, opcode2 == 1), 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], operand[1]);
			return;

		/* immediate to memory or register operand with the	*/
		/* 'w' bit present					*/
		case IMw:
			wbit = WBIT(opcode2);
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, wbit, 1);
			imm_data(OPSIZE(data16, wbit), 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], operand[1]);
			return;

		/* immediate to register with register in low 3 bits	*/
		/* of op code						*/
		case IR:
			/* w-bit here (with regs) is bit 3 */
			wbit = opcode2 >>3 & 0x1;
			reg = REGNO(opcode2);
			imm_data(OPSIZE(data16, wbit), 0);
			if (data16)
				reg_name = REG16[reg][wbit];
			else
				reg_name = REG32[reg][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* MMX immediate shift of register			*/
		case MMXSH:
			reg = REGNO(opcode7);
			imm_data(1, 0);
			reg_name = MMXREG[reg];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* memory operand to accumulator			*/
		case OA:
			wbit = WBIT(opcode2);
			displacement(OPSIZE(addr16, LONGOPERAND), 0, &lngval);
			reg_name = (data16 ? REG16 : REG32)[0][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* accumulator to memory operand			*/
		case AO:
			wbit = WBIT(opcode2);
			displacement(OPSIZE(addr16, LONGOPERAND), 0, &lngval);
			reg_name = (addr16 ? REG16 : REG32)[0][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

		/* memory or register operand to segment register	*/
		case MS:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], SEGREG[reg]);
			return;

		/* segment register to memory or register operand	*/
		case SM:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, SEGREG[reg], operand[0]);
			return;
		/*
		 * rotate or shift instrutions, which may shift by 1 or
		 * consult the cl register, depending on the 'v' bit
		 */
		case Mv:
			vbit = VBIT(opcode2);
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			/*
			 * When vbit is set, register is an operand,
			 *otherwise just $0x1
			 */
			reg_name = vbit ? "%cl," : "$1,";
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s%s", mneu, reg_name, operand[0]);
			return;
		/*
		 * immediate rotate or shift instrutions, which may or
		 * may not consult the cl register, depending on the 'v' bit
		 */
		case MvI:
			vbit = VBIT(opcode2);
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			imm_data(1, 1);
			/*
			 * When vbit is set, register is an operand,
			 * otherwise just $0x1
			 */
			reg_name = vbit ? "%cl," : "";
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s%s", mneu, operand[1], reg_name,
			    operand[0]);
			return;

		case MIb:
			get_operand(mode, r_m, LONGOPERAND, 0);
			imm_data(1, 1);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[1], operand[0]);
			return;

		/* single memory or register operand with 'w' bit present */
		case Mw:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		/* single memory or register operand */
		case M:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		case SREG: /* special register */
			get_modrm_byte(&mode, &reg, &r_m);
			vbit = 0;
			switch (opcode5) {
			case 2:
				vbit = 1;
				/* FALLTHROUGH */
			case 0:
				reg_name = CONTROLREG[reg];
				break;
			case 3:
				vbit = 1;
				/* FALLTHROUGH */
			case 1:
				reg_name = DEBUGREG[reg];
				break;
			case 6:
				vbit = 1;
				/* FALLTHROUGH */
			case 4:
				reg_name = TESTREG[reg];
				break;
			}
			(void) strcpy(operand[0], REG32[r_m][1]);

			if (vbit) {
				(void) strcpy(operand[0], reg_name);
				reg_name = REG32[r_m][1];
			}

			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, reg_name, operand[0]);
			return;

		/*
		 * single register operand with register in the low 3
		 * bits of op code
		 */
		case R:
			if (opcode_bytes == 2)
				reg = REGNO(opcode5);
			else
				reg = REGNO(opcode2);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, reg_name);
			return;

		/*
		 * register to accumulator with register in the low 3
		 * bits of op code, xchg instructions
		 */
		case RA: {
			char *eprefix;
			reg = REGNO(opcode2);
			if (data16) {
				eprefix = "";
				reg_name = REG16[reg][LONGOPERAND];
			} else {
				eprefix = "e";
				reg_name = REG32[reg][LONGOPERAND];
			}
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%%%sax", mneu, reg_name, eprefix);
			return;
		}

		/*
		 * single segment register operand, with register in
		 * bits 3-4 of op code
		 */
		case SEG:
			reg = curbyte >> 3 & 0x3; /* segment register */
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, SEGREG[reg]);
			return;

		/*
		 * single segment register operand, with register in
		 * bits 3-5 of op code
		 */
		case LSEG:
			/* long seg reg from opcode */
			reg = curbyte >> 3 & 0x7;
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, SEGREG[reg]);
			return;

			/* memory or register operand to register */
		case MR:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* MMX memory or register operand to register		*/
		case MMXL:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
			    MMXOPERAND : LONGOPERAND, 0);
			reg_name = MMXREG[reg];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* immediate operand to accumulator */
		case IA: {
			int no_bytes = OPSIZE(data16, WBIT(opcode2));
			switch (no_bytes) {
				case 1: reg_name = "%al"; break;
				case 2: reg_name = "%ax"; break;
				case 4: reg_name = "%eax"; break;
			}
			imm_data(no_bytes, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;
		}
		/* memory or register operand to accumulator */
		case MA:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			reg_name = (data16 ? REG16 : REG32) [0][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], reg_name);
			return;

		/* si register to di register				*/
		case SD:
			check_override(0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s(%%%ssi),(%%%sdi)",
			    mneu, operand[0], addr16? "" : "e",
			    addr16? "" : "e");
			return;

		/* accumulator to di register				*/
		case AD:
			wbit = WBIT(opcode2);
			check_override(0);
			reg_name = (data16 ? REG16 : REG32) [0][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s(%%%sdi)", mneu,
			    reg_name, operand[0], addr16? "" : "e");
			return;

		/* si register to accumulator				*/
		case SA:
			wbit = WBIT(opcode2);
			check_override(0);
			reg_name = (addr16 ? REG16 : REG32) [0][wbit];
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s(%%%ssi),%s", mneu,
			    operand[0], addr16? "" : "e", reg_name);
			return;

		/*
		 * single operand, a 16/32 bit displacement
		 * added to current offset by 'compoff'
		 */
		case D:
			reljmp = 1;
			displacement(OPSIZE(data16, LONGOPERAND), 0, &lngval);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s%s", mneu, operand[0], operand[1]);
			compoff(lngval);
			return;

		/* indirect to memory or register operand		*/
		case INM:
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s*%s", mneu, operand[0]);
			return;

		/*
		 * for long jumps and long calls -- a new code segment
		 * register and an offset in IP -- stored in object
		 * code in reverse order
		 */
		case SO:
			displacement(OPSIZE(addr16, LONGOPERAND), 1, &lngval);
			/* will now get segment operand */
			displacement(2, 0, &lngval);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s$%s,$%s", mneu, operand[0], operand[1]);
			return;

		/*
		 * jmp/call. single operand, 8 bit displacement.
		 * added to current EIP in 'compoff'
		 */
		case BD:
			reljmp = 1;
			displacement(1, 0, &lngval);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s%s", mneu, operand[0], operand[1]);
			compoff(lngval);
			return;

		/* single 32/16 bit immediate operand			*/
		case I:
			imm_data(OPSIZE(data16, LONGOPERAND), 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		/* single 8 bit immediate operand			*/
		case Ib:
			imm_data(1, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		case ENTER:
			imm_data(2, 0);
			imm_data(1, 1);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s,%s", mneu, operand[0], operand[1]);
			return;

		/* 16-bit immediate operand */
		case RET:
			imm_data(2, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		/* single 8 bit port operand				*/
		case P:
			check_override(0);
			imm_data(1, 0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s", mneu, operand[0]);
			return;

		/* single operand, dx register (variable port instruction) */
		case V:
			check_override(0);
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%s(%%dx)", mneu, operand[0]);
			return;

		/*
		 * The int instruction, which has two forms:
		 * int 3 (breakpoint) or
		 * int n, where n is indicated in the subsequent
		 * byte (format Ib).  The int 3 instruction (opcode 0xCC),
		 * where, although the 3 looks  like an operand,
		 * it is implied by the opcode. It must be converted
		 * to the correct base and output.
		 */
		case INT3:
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s$0x3", mneu);
			return;

		/* an unused byte must be discarded */
		case U:
			getbyte();
			return;

		case CBW:
			if (data16)
				(void) strcat(mneu, "cbtw");
			else
				(void) strcat(mneu, "cwtl");
			return;

		case CWD:
			if (data16)
				(void) strcat(mneu, "cwtd");
			else
				(void) strcat(mneu, "cltd");
			return;

		/*
		 * no disassembly, the mnemonic was all there was
		 * so go on
		 */
		case GO_ON:
			return;

		/*
		 * Special byte indicating a the beginning of a
		 * jump table has been seen. The jump table addresses
		 * will be printed until the address 0xffff which
		 * indicates the end of the jump table is read.
		 */
		case JTAB:
			(void) strcpy(mneu, "***JUMP TABLE BEGINNING***");
			return;

		/* float reg */
		case F:
			(void) mdb_iob_snprintf(mneu, sizeof (mneu),
			    "%s%%st(%1d)", mneu, r_m);
			return;

		/* float reg to float reg, with ret bit present */
		case FF:
			if (opcode2 >> 2 & 0x1) {
				/*
				 * return result bit for 287 instructions
				 * st -> st(i)
				 */
				(void) mdb_iob_snprintf(mneu, sizeof (mneu),
				    "%s%%st,%%st(%1d)", mneu, r_m);
			} else {
				/* st(i) -> st */
				(void) mdb_iob_snprintf(mneu, sizeof (mneu),
				    "%s%%st(%1d),%%st", mneu, r_m);
			}
			return;

		/* an invalid op code */
		case AM:
		case DM:
		case OVERRIDE:
		case PREFIX:
		case UNKNOWN:
			bad_opcode();
			return;

		default:
			warn("internal disassembler error: "
			    "case from instruction table not found\n");
			return;
		} /* end switch */

	/* NOTREACHED */
}

static	mdb_tgt_t 	*dis_target;
static	mdb_tgt_as_t	dis_as;
static	mdb_tgt_addr_t	dis_offset;
static	ssize_t		dis_size;
static	uchar_t		dis_buffer[64];

static	mdb_tgt_addr_t	curloc;
static	mdb_tgt_addr_t	loc_start;

/*
 * Get next byte from the instruction stream,
 * set curbyte and increment curloc.
 */
static void
getbyte()
{
	ulong_t index = (ulong_t)(curloc - dis_offset);

	if (index >= dis_size) {
		dis_size = mdb_tgt_aread(dis_target, dis_as, dis_buffer,
			sizeof (dis_buffer), curloc);

		if (dis_size <= 0) {
			dis_offset = 0;
			dis_size = 0;
			curbyte = 0;
			return;
		}

		dis_offset = curloc;
		index = 0;
	}

	curbyte = dis_buffer[index];
	curloc++;
}

/*ARGSUSED*/
mdb_tgt_addr_t
ia32dis_ins2str(mdb_disasm_t *dp, mdb_tgt_t *t, mdb_tgt_as_t as,
    char *buf, mdb_tgt_addr_t pc)
{
	char *cp;

	dis_target = t;		/* target pointer */
	dis_as = as;		/* address space identifier */
	dis_offset = pc;	/* address of current instruction */
	dis_size = 1;		/* size of current instruction */

	if (mdb_tgt_aread(t, as, &dis_buffer[0], sizeof (char), pc) == -1) {
		warn("failed to read instruction at %llr", pc);
		return (pc);
	}

	/*
	 * Disassemble one instruction starting at curloc,
	 * increment curloc to the following location,
	 * and leave the ascii result in mneu[].
	 */
	loc_start = pc;
	curloc = pc;
	disasm();

	cp = mneu + strlen(mneu);
	while (cp-- > mneu && *cp == ' ')
		*cp = '\0';
	(void) strcpy(buf, mneu);
	return (curloc);
}

static char *
tab(int n)
{
	char *cp = mneu + strlen(mneu);

	while (cp < mneu + n)
		*cp++ = ' ';
	if (*(cp-1) != ' ')
		*cp++ = ' ';
	return (cp);
}

/*
 *	void get_modrm_byte (mode, reg, r_m)
 *
 *	Get the byte following the op code and separate it into the
 *	mode, register, and r/m fields.
 * Scale-Index-Bytes have a similar format.
 */

static void
get_modrm_byte(unsigned *mode, unsigned *reg, unsigned *r_m)
{
	getbyte();

	*r_m = curbyte & 0x7; /* r/m field from curbyte */
	*reg = curbyte >> 3 & 0x7; /* register field from curbyte */
	*mode = curbyte >> 6 & 0x3; /* mode field from curbyte */
}

/*
 *	void check_override (opindex)
 *
 *	Check to see if there is a segment override prefix pending.
 *	If so, print it in the current 'operand' location and set
 *	the override flag back to false.
 */

static void
check_override(int opindex)
{
	if (overreg)
		(void) strcpy(operand[opindex], overreg);
	overreg = NULL;
}

/*
 *	void displacement (no_bytes, opindex, value)
 *
 *	Get and print in the 'operand' array a one, two or four
 *	byte displacement from a register.
 */

static void
displacement(int no_bytes, int opindex, long *value)
{
	char	temp[(NCPS*2)+1];
	long	val;

	getbytes(no_bytes, temp, value);
	val = *value;
	if (reljmp) {
		val += curloc - loc_start;
		signed_disp = 1;
		reljmp = 0;
	}
	check_override(opindex);
	if (signed_disp) {
		if (val >= 0 || val < 0xfff00000)
			(void) mdb_iob_snprintf(temp, sizeof (temp),
			    "+%#lr", val);
		else
			(void) mdb_iob_snprintf(temp, sizeof (temp),
			    "-%#lr", -val);
		signed_disp = 0;
	}
	(void) strcat(operand[opindex], temp);
}

static void
get_operand(unsigned mode, unsigned r_m, int wbit, int opindex)
{
	extern	char	dispsize16[8][4];	/* tables.c */
	extern	char	dispsize32[8][4];
	extern	char	*regname16[4][8];
	extern	char	*regname32[4][8];
	extern	char	**regname;	/* External parameter to locsympr() */
	extern	char	*indexname[8];
	extern	char 	*REG16[8][2];	  /* in tables.c */
	extern	char 	*REG32[8][2];	  /* in tables.c */
	extern	char	*MMXREG[8];
	extern	char	*scale_factor[4];	  /* in tables.c */
	int dispsize;   /* size of displacement in bytes */
	long dispvalue;  /* value of the displacement */
	char *resultreg; /* representation of index(es) */
	char *format;   /* output format of result */
	int s_i_b;	/* flag presence of scale-index-byte */
	unsigned ss;    /* scale-factor from opcode */
	unsigned index; /* index register number */
	unsigned base;  /* base register number */
	char indexbuffer[16]; /* char representation of index(es) */

	/* if symbolic representation, skip override prefix, if any */
	check_override(opindex);

	/* check for the presence of the s-i-b byte */
	if (r_m == ESP && mode != REG_ONLY && !addr16) {
		s_i_b = TRUE;
		get_modrm_byte(&ss, &index, &base);
	}
	else
		s_i_b = FALSE;

	if (addr16) {
		dispsize = dispsize16[r_m][mode];
		regname = regname16[mode]; /* Address of an array */
	} else {
		dispsize = dispsize32[r_m][mode];
		regname = regname32[mode]; /* Address of an array */
	}

	if (s_i_b && mode == 0 && base == EBP) dispsize = 4;

	if (dispsize != 0) {
		if (s_i_b || mode) signed_disp = 1;
		displacement(dispsize, opindex, &dispvalue);
	}

	if (s_i_b) {
		register char *basereg = regname32[mode][base];
		if (ss)
			(void) mdb_iob_snprintf(indexbuffer,
			    sizeof (indexbuffer), "%s%s,%s", basereg,
			    indexname[index], scale_factor[ss]);
		else
			(void) mdb_iob_snprintf(indexbuffer,
			    sizeof (indexbuffer), "%s%s", basereg,
			    indexname[index]);
		resultreg = indexbuffer;
		format = "%s(%s)";
	} else { /* no s-i-b */
		if (mode == REG_ONLY) {
			format = "%s%s";
			if (wbit == MMXOPERAND)
				resultreg = MMXREG[r_m];
			else if (data16)
				resultreg = REG16[r_m][wbit];
			else
				resultreg = REG32[r_m][wbit];
		} else { /* Modes 00, 01, or 10 */
			if (addr16)
				resultreg = regname16[mode][r_m];
			else
				resultreg = regname32[mode][r_m];
			if (r_m == EBP && mode == 0) { /* displacement only */
				format = "%s";
			} else {
				/*
				* Modes 00, 01, or 10, not displacement only,
				*and no s-i-b
				*/
				format = "%s(%s)";
			}
		}
	}
	(void) mdb_iob_snprintf(operand[opindex], OPLEN, format,
	    operand[opindex], resultreg);
}

/*
** getbytes() reads no_bytes from a file and converts them into destbuf.
** A sign-extended value is placed into destvalue if it is non-null.
*/
static void
getbytes(int no_bytes, char *destbuf, long *destvalue)
{
	int i;
	unsigned long shiftbuf = 0;
	int not_signed;

	for (i = 0; i < no_bytes; i++) {
		getbyte();
		shiftbuf |= (long)curbyte << (8 * i);
	}

	switch (no_bytes) {
		case 1:
			if (shiftbuf & 0x80)
				shiftbuf |= 0xffffff00ul;
			break;
		case 2:
			if (shiftbuf & 0x8000)
				shiftbuf |= 0xffff0000ul;
			break;
	}
	if (destvalue)
		*destvalue = shiftbuf;
	if (shiftbuf & 0x80000000) {
		not_signed = 0;
		if ((strncmp(mneu, "or", 2) == 0) ||
		    (strncmp(mneu, "and", 3) == 0) ||
		    (strncmp(mneu, "xor", 3) == 0) ||
		    (strncmp(mneu, "test", 4) == 0) ||
		    (strncmp(mneu, "in", 2) == 0) ||
		    (strncmp(mneu, "out", 3) == 0) ||
		    (strncmp(mneu, "lcall", 5) == 0) ||
		    (strncmp(mneu, "ljmp", 4) == 0) ||
		    ((mneu[0] == 'r') &&		/* rotate/shift */
		    ((mneu[1] == 'c') || (mneu[1] == 'o'))) ||
		    ((mneu[0] == 's') &&
		    ((mneu[1] == 'a') || (mneu[1] == 'h'))) ||
		    ((mneu[0] == 'p') && (mneu[1] == 's') &&
		    ((mneu[2] == 'r') || (mneu[2] == 'l')))) {
			if (no_bytes == 1)
				shiftbuf &= 0xff;
			else if (no_bytes == 2)
				shiftbuf &= 0xffff;
			not_signed = 1;
		} else if (shiftbuf < 0xfff00000) {
			/* don't negate kernel and kadb addresses */
			not_signed = 1;
		}
		if (not_signed == 0) {
			shiftbuf = -shiftbuf;
			*destbuf++ = '-';
		}
	}
	(void) mdb_iob_snprintf(destbuf, BUFSIZ, "%#lr", shiftbuf);
}

/*
 *	void imm_data (no_bytes, opindex)
 *
 *	Determine if 1, 2 or 4 bytes of immediate data are needed, then
 *	get and print them.
 */

static void
imm_data(int no_bytes, int opindex)
{
	long value;
	int len = strlen(operand[opindex]);

	operand[opindex][len] = '$';
	getbytes(no_bytes, &operand[opindex][len+1], &value);
}


/*
 *	get_opcode (high, low)
 *	Get the next byte and separate the op code into the high and
 *	low nibbles.
 */

static void
get_opcode(unsigned *high, unsigned *low)
{
	getbyte();
	*low = curbyte & 0xf;  /* ----xxxx low 4 bits */
	*high = curbyte >> 4 & 0xf;  /* xxxx---- bits 7 to 4 */
}

/* 	bad_opcode	*/
/* 	print message and try to recover */

static void
bad_opcode(void)
{
	(void) strcpy(mneu, "***ERROR--unknown op code***");
}

/*
 * Compute the location to which control will be transferred by a jump, and
 * print it in the margin as a symbol name + offset.  We do this by adding
 * the offset 'lng' (already masked and negated) to the current location and
 * then printing it into mneu using mdb's %a format character.
 */
static void
compoff(long lng)
{
	(void) mdb_iob_snprintf(tab(24), sizeof (mneu), "<%a>",
	    (uintptr_t)(curloc + lng));
}
