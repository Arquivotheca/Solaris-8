/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ident "@(#)bits.c 1.9 98/05/08 SMI"


#include	"dis.h"
#include	"extn.h"
/*
 * maximum # of errors allowed before
 * abandoning this disassembly as a
 * hopeless case
 */
#define	MAXERRS	10
/*
 * set to 1 before call to displacement() when
 * a signed decimal displacement is desired,
 * as opposed to a hexadecimal one
 */
static int signed_disp;
/* keep track of relative jumps */
static int reljmp;
/* keep track of original value of 'loc' */
static long loc_start;
/*
 * to  keep track of errors encountered during
 * the disassembly, probably due to being out
 * of sync
 */
static short errlev = 0;
/*
 * maximum length of a single operand
 * (will be used for printing)
 */
#define	OPLEN	256

static	char	operand[3][OPLEN];	/* to store operands as they	*/
					/* are encountered		*/
static	char	symarr[3][OPLEN];
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
/*
 * function prototypes
 */
void get_modrm_byte(unsigned *, unsigned *, unsigned *);
void check_override(int);
void imm_data(int, int);
void get_opcode(unsigned *, unsigned *);
void bad_opcode(void);
void displacement(int, int, long *);
void get_operand(unsigned, unsigned, int, int);
void getbytes(int, char *, long *);
void Lextsympr(long, char **);
static void Getbyte(void);
static void savebytes(unsigned);

/*
 *	void dis_text ( shdr )
 *
 *	disassemble a text section
 */

void
dis_text(GElf_Shdr *shdr)
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
	unsigned short tmpshort;
	long	lngval;
	char *reg_name;
	int got_modrm_byte;
	char mnemonic[OPLEN];
	char	temp[NCPS+1];
	/* number of bytes of opcode - used to get wbit */
	static char opcode_bytes;
	int switch_heuristic = 0;
	extern int check_func(void);
	extern int at_func(void);
	extern void compofff(long, char *, char *);

	/*
	 * An instruction is disassembled with each iteration of the
	 * following loop.  The loop is terminated upon completion of the
	 * section (loc minus the section's physical address becomes equal
	 * to the section size) or if the number of bad op codes encountered
	 * would indicate this disassembly is hopeless.
	 */

	for (loc = shdr->sh_addr; ((loc-shdr->sh_addr) <
	    shdr->sh_size) && (errlev < MAXERRS); printline()) {
		loc_start = loc;
		if (Lflag > 0)			/* look for C source labels */
			looklabel(loc);
		line_nums();	/* print breakpoint line # */
		prt_offset();			/* print offset		   */
		mnemonic[0] = '\0';
		mneu[0] = '\0';
		symrep[0] = '\0';
		operand[0][0] = '\0';
		operand[1][0] = '\0';
		operand[2][0] = '\0';
		symarr[0][0] = '\0';
		symarr[1][0] = '\0';
		symarr[2][0] = '\0';
		addr16 = Rflag;
		data16 = Rflag;
		overreg = (char *) 0;
		opcode_bytes = 0;

		if (at_func())
			switch_heuristic = 0;
		else if (switch_heuristic) {
			getbytes(4, temp, &lngval);
			if (switch_heuristic == 1 &&
			    (lngval & 0xff) == 0x90 &&
			    (loc & 3) != 0)	/* assume it's a nop */
				;
			else if (lngval > -32000 && lngval < 32000) {
				sprintf(mneu, ".long\t%d", lngval);
				switch_heuristic = 2;
				continue;
			}
			savebytes((unsigned)lngval);
			if ((lngval & 0xff) != 0x90)
				switch_heuristic = 3;
		}
		/*
		** As long as there is a prefix, the default segment register,
		** addressing-mode, or data-mode in the instruction
		** will be overridden.
		** This may be more general than the chip actually is.
		*/
		for (;;) {
			get_opcode(&opcode1, &opcode2);
			if (opcode1 == 0 && opcode2 == 0) {
				if (check_func() ||	/* found 0 pad */
				    switch_heuristic == 3) {
					sprintf(mneu, ".byte\t0");
					goto loop_end;
				}
			}
			if (switch_heuristic > 1)
				switch_heuristic = 0;
			dp = &distable[opcode1][opcode2];

			if (dp->adr_mode == PREFIX)
				strcat(mnemonic, dp->name);
			else if (dp->adr_mode == AM) addr16 = !addr16;
			else if (dp->adr_mode == DM) data16 = !data16;
			else if (dp->adr_mode == OVERRIDE) overreg = dp->name;
			else break;
		}
		/*
		 * some 386 instructions have 2 bytes of opcode
		 * before the mod_r/m
		 * byte so we need to perform a table indirection.
		 */
		if (dp->indirect == (struct instable *) op0F) {
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
			continue;
		}

		/* print the mnemonic */
		if (dp->adr_mode != CBW && dp->adr_mode != CWD) {
			/* print the mnemonic */
			(void) strcat(mnemonic, dp -> name);
			if (dp->suffix)
				(void) strcat(mnemonic, (data16? "w" : "l"));
			(void) sprintf(mneu,
			    (strlen(mnemonic) < (unsigned int)7 ?
				"%-7s" : "%-7s "), mnemonic);
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
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				(void) sprintf(symrep, "%s,%s",
				    symarr[0], reg_name);
			continue;

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
			(void) sprintf(mneu, "%s%s,%s,%s", mneu,
			    operand[0], operand[1], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s,%s", symarr[0],
				    symarr[1], reg_name);
			continue;

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
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

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
			(void) sprintf(mneu, "%s%s,%s", mneu, reg_name,
			    operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", reg_name, symarr[0]);
			continue;

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
			(void) sprintf(mneu, "%s%s,%s", mneu, reg_name,
			    operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", reg_name, symarr[0]);
			continue;

		/* MMX register to memory or register operand		*/
		case MMXS:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
			    MMXOPERAND : LONGOPERAND, 0);
			reg_name = MMXREG[reg];
			(void) sprintf(mneu, "%s%s,%s", mneu, reg_name,
			    operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", reg_name, symarr[0]);
			continue;

		/* Double shift. Has immediate operand specifying the shift. */
		case DSHIFT:
			get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 1);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			imm_data(1, 0);
			sprintf(mneu, "%s%s,%s,%s", mneu,
			    operand[0], reg_name, operand[1]);
			if (sflag)
				sprintf(symrep, "%s,%s,%s", symarr[0],
				    reg_name, symarr[1]);
			continue;

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
			sprintf(mneu, "%s%s,%s", mneu, reg_name, operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", reg_name, symarr[0]);
			continue;

			/* immediate to memory or register operand */
		case IMlw:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 1);
			/*
			 * A long immediate is expected for opcode 0x81,
			 * not 0x80 nor 0x83
			 */
			imm_data(OPSIZE(data16, opcode2 == 1), 0);
			sprintf(mneu, "%s%s,%s", mneu, operand[0], operand[1]);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], symarr[1]);
			continue;

		/* immediate to memory or register operand with the	*/
		/* 'w' bit present					*/
		case IMw:
			wbit = WBIT(opcode2);
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, wbit, 1);
			imm_data(OPSIZE(data16, wbit), 0);
			sprintf(mneu, "%s%s,%s", mneu, operand[0], operand[1]);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], symarr[1]);
			continue;

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
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

		/* MMX immediate shift of register			*/
		case MMXSH:
			reg = REGNO(opcode7);
			imm_data(1, 0);
			reg_name = MMXREG[reg];
			(void) sprintf(mneu, "%s%s,%s", mneu, operand[0],
			    reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

		/* memory operand to accumulator			*/
		case OA:
			wbit = WBIT(opcode2);
			displacement(OPSIZE(addr16, LONGOPERAND), 0, &lngval);
			reg_name = (data16 ? REG16 : REG32)[0][wbit];
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag) {
				char *symstring = symarr[0];
				if (Rel_data)
					search_rel_data(lngval+loc,
					    lngval+loc, &symstring);
				else
					Lextsympr(lngval+loc, &symstring);
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			}
			continue;

		/* accumulator to memory operand			*/
		case AO:
			wbit = WBIT(opcode2);
			displacement(OPSIZE(addr16, LONGOPERAND), 0, &lngval);
			reg_name = (addr16 ? REG16 : REG32)[0][wbit];
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    reg_name, operand[0]);
			if (sflag) {
				char *symstring = symarr[0];
				if (Rel_data)
					search_rel_data(lngval+loc,
					    lngval+loc, &symstring);
				else
					Lextsympr(lngval+loc, &symstring);
				sprintf(symrep, "%s,%s", reg_name, symarr[0]);
			}
			continue;

		/* memory or register operand to segment register	*/
		case MS:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], SEGREG[reg]);
			continue;

		/* segment register to memory or register operand	*/
		case SM:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    SEGREG[reg], operand[0]);
			continue;
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
			(void) sprintf(mneu, "%s%s%s", mneu,
			    reg_name, operand[0]);
			if (sflag)
				sprintf(symrep, "%s%s", reg_name, symarr[0]);
			continue;
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
			(void) sprintf(mneu, "%s%s,%s%s", mneu, operand[1],
			    reg_name, operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s%s", symarr[1],
				    reg_name, symarr[0]);
			continue;

		case MIb:
			get_operand(mode, r_m, LONGOPERAND, 0);
			imm_data(1, 1);
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[1], operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[1], symarr[0]);
			continue;

		/* single memory or register operand with 'w' bit present */
		case Mw:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			continue;

		/* single memory or register operand */
		case M:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			continue;

		case SREG: /* special register */
			get_modrm_byte(&mode, &reg, &r_m);
			vbit = 0;
			switch (opcode5) {
			case 2:
				vbit = 1;
				/* fall thru */
			case 0:
				reg_name = CONTROLREG[reg];
				break;
			case 3:
				vbit = 1;
				/* fall thru */
			case 1:
				reg_name = DEBUGREG[reg];
				break;
			case 6:
				vbit = 1;
				/* fall thru */
			case 4:
				reg_name = TESTREG[reg];
				break;
			}
			strcpy(operand[0], REG32[r_m][1]);

			if (vbit) {
				strcpy(operand[0], reg_name);
				reg_name = REG32[r_m][1];
			}

			(void) sprintf(mneu, "%s%s,%s",
				mneu, reg_name, operand[0]);
			if (sflag)
				sprintf(symrep, "%s,%s", reg_name, operand[0]);
			continue;

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
			(void) sprintf(mneu, "%s%s", mneu, reg_name);
			if (sflag)
				strcpy(symrep, reg_name);
			continue;

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
			(void) sprintf(mneu, "%s%s,%%%sax",
				mneu, reg_name, eprefix);
			if (sflag)
				(void) sprintf(symrep, "%s,%%%sax",
					reg_name, eprefix);
			continue;
		}

		/*
		 * single segment register operand, with register in
		 * bits 3-4 of op code
		 */
		case SEG:
			reg = curbyte >> 3 & 0x3; /* segment register */
			(void) sprintf(mneu, "%s%s", mneu, SEGREG[reg]);
			if (sflag)
				(void) strcpy(symrep, SEGREG[reg]);
			continue;

		/*
		 * single segment register operand, with register in
		 * bits 3-5 of op code
		 */
		case LSEG:
			/* long seg reg from opcode */
			reg = curbyte >> 3 & 0x7;
			(void) sprintf(mneu, "%s%s", mneu, SEGREG[reg]);
			if (sflag)
				(void) strcpy(symrep, SEGREG[reg]);
			continue;

			/* memory or register operand to register */
		case MR:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, LONGOPERAND, 0);
			if (data16)
				reg_name = REG16[reg][LONGOPERAND];
			else
				reg_name = REG32[reg][LONGOPERAND];
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

		/* MMX memory or register operand to register		*/
		case MMXL:
			if (!got_modrm_byte)
				get_modrm_byte(&mode, &reg, &r_m);
			get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
			    MMXOPERAND : LONGOPERAND, 0);
			reg_name = MMXREG[reg];
			(void) sprintf(mneu, "%s%s,%s", mneu, operand[0],
			    reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

		/* immediate operand to accumulator */
		case IA: {
			int no_bytes = OPSIZE(data16, WBIT(opcode2));
			switch (no_bytes) {
				case 1: reg_name = "%al"; break;
				case 2: reg_name = "%ax"; break;
				case 4: reg_name = "%eax"; break;
			}
			imm_data(no_bytes, 0);
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;
		}
		/* memory or register operand to accumulator */
		case MA:
			wbit = WBIT(opcode2);
			get_operand(mode, r_m, wbit, 0);
			reg_name = (data16 ? REG16 : REG32) [0][wbit];
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], reg_name);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], reg_name);
			continue;

		/* si register to di register				*/
		case SD:
			check_override(0);
			(void) sprintf(mneu, "%s%s(%%%ssi),(%%%sdi)",
			    mneu, operand[0], addr16? "" : "e",
			    addr16? "" : "e");
			if (sflag)
				(void) sprintf(symrep, "%s%%%ssi,%%%sdi",
				    symarr[0], addr16? "" : "e",
				    addr16? "" : "e");
			continue;

		/* accumulator to di register				*/
		case AD:
			wbit = WBIT(opcode2);
			check_override(0);
			reg_name = (data16 ? REG16 : REG32) [0][wbit];
			(void) sprintf(mneu, "%s%s,%s(%%%sdi)", mneu,
			    reg_name, operand[0], addr16? "" : "e");
			if (sflag)
				(void) sprintf(symrep, "%s,%s%%%sdi",
				    reg_name, symarr[0], addr16? "" : "e");
			continue;

		/* si register to accumulator				*/
		case SA:
			wbit = WBIT(opcode2);
			check_override(0);
			reg_name = (addr16 ? REG16 : REG32) [0][wbit];
			(void) sprintf(mneu, "%s%s(%%%ssi),%s", mneu,
			    operand[0], addr16? "" : "e", reg_name);
			if (sflag)
				(void) sprintf(symrep, "%s%%%ssi,%s",
				    symarr[0], addr16? "" : "e", reg_name);
			continue;

		/*
		 * single operand, a 16/32 bit displacement
		 * added to current offset by 'compofff'
		 */
		case D:
			reljmp = 1;
			displacement(OPSIZE(data16, LONGOPERAND), 0, &lngval);
			compofff(lngval, operand[1], operand[0]);
			if (lngval == -4)
				(void) sprintf(mneu, "%s<extern>", mneu);
			else if (strncmp(operand[1], "0x", 2) == 0)
				(void) sprintf(mneu, *operand[0] == '-' ?
				    "%s.%s\t[%s]" : "%s.+%s\t[%s]", mneu,
				    operand[0], (lngval == 0) ? "" :
				    operand[1]);
			else
				(void) sprintf(mneu, "%s%s\t[%s]", mneu,
				    operand[1], operand[0]);
			if (sflag) {
				char *symstring = symarr[0];
				if (Rel_data)
					search_rel_data(lngval+loc,
					    lngval+loc, &symstring);
				else
					Lextsympr(lngval+loc, &symstring);
				sprintf(symrep, "%s", symarr[0]);
			}
			continue;

		/* indirect to memory or register operand		*/
		case INM:
			get_operand(mode, r_m, LONGOPERAND, 0);
			(void) sprintf(mneu, "%s*%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			if ((strcmp(mneu, "jmp    *%ecx") == 0)) {
				/*
				 * heuristic fix for disassembling
				 * jump tables used to implement
				 * the C switch statement.
				 */
				switch_heuristic = 1;
			}
			continue;

		/*
		 * for long jumps and long calls -- a new code segment
		 * register and an offset in IP -- stored in object
		 * code in reverse order
		 */
		case SO:
			displacement(OPSIZE(addr16, LONGOPERAND), 1, &lngval);
			/* will now get segment operand */
			displacement(2, 0, &lngval);
			(void) sprintf(mneu, "%s$%s,$%s", mneu,
			    operand[0], operand[1]);
			if (sflag)
				sprintf(symrep, "%s,%s",
				    operand[0], operand[1]);
			continue;

		/*
		 * jmp/call. single operand, 8 bit displacement.
		 * added to current EIP in 'compofff'
		 */
		case BD:
			reljmp = 1;
			displacement(1, 0, &lngval);
			compofff(lngval, operand[1], operand[0]);
			if (lngval == -4)
				(void) sprintf(mneu, "%s<extern>", mneu);
			else if (strncmp(operand[1], "0x", 2) == 0)
				(void) sprintf(mneu, *operand[0] == '-' ?
				    "%s.%s\t[%s]" : "%s.+%s\t[%s]", mneu,
				    operand[0], (lngval == 0) ? "" :
				    operand[1]);
			else
				(void) sprintf(mneu, "%s%s\t[%s]", mneu,
				    operand[1], operand[0]);
			if (sflag) {
				char *symstring = symarr[0];
				if (Rel_data)
					search_rel_data(lngval+loc,
					    lngval+loc, &symstring);
				else
					Lextsympr(lngval+loc, &symstring);
				sprintf(symrep, "%s", symarr[0]);
			}
			continue;

		/* single 32/16 bit immediate operand			*/
		case I:
			imm_data(OPSIZE(data16, LONGOPERAND), 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			continue;

		/* single 8 bit immediate operand			*/
		case Ib:
			imm_data(1, 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			continue;

		case ENTER:
			imm_data(2, 0);
			imm_data(1, 1);
			(void) sprintf(mneu, "%s%s,%s", mneu,
			    operand[0], operand[1]);
			if (sflag)
				sprintf(symrep, "%s,%s", symarr[0], symarr[1]);
			continue;

		/* 16-bit immediate operand */
		case RET:
			imm_data(2, 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", symarr[0]);
			continue;

		/* single 8 bit port operand				*/
		case P:
			check_override(0);
			imm_data(1, 0);
			(void) sprintf(mneu, "%s%s", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s", operand[0]);
			continue;

		/* single operand, dx register (variable port instruction) */
		case V:
			check_override(0);
			(void) sprintf(mneu, "%s%s(%%dx)", mneu, operand[0]);
			if (sflag)
				sprintf(symrep, "%s(%%dx)", operand[0]);
			continue;

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
			convert(3, temp, LEAD);
			(void) sprintf(mneu, "%s$%s", mneu, temp);
			if (sflag)
				sprintf(symrep, "$%s", temp);
			continue;

		/* an unused byte must be discarded */
		case U:
			Getbyte();
			continue;

		case CBW:
			if (data16)
				(void) strcat(mneu, "cbtw");
			else
				(void) strcat(mneu, "cwtl");
			continue;

		case CWD:
			if (data16)
				(void) strcat(mneu, "cwtd");
			else
				(void) strcat(mneu, "cltd");
			continue;

		/*
		 * no disassembly, the mnemonic was all there was
		 * so go on
		 */
		case GO_ON:
			continue;

		/*
		 * Special byte indicating a the beginning of a
		 * jump table has been seen. The jump table addresses
		 * will be printed until the address 0xffff which
		 * indicates the end of the jump table is read.
		 */
		case JTAB:
			(void) sprintf(mneu, "***JUMP TABLE BEGINNING***");
			printline();
			prt_offset();
			lookbyte();
			if (curbyte == FILL) {
				if (Lflag && debug)
					looklabel(loc);
				line_nums();
				(void) sprintf(mneu,
				    "FILL BYTE FOR ALIGNMENT");
				(void) sprintf(object, "%s90", object);
				printline();
				prt_offset();
				(void) printf("\t");
				lookbyte();
				tmpshort = curbyte;
				lookbyte();
				if (oflag)
					(void) sprintf(object, "%s%03o %03o",
						object, curbyte, tmpshort);
				else
					(void) sprintf(object, "%s%02x %02x",
						object, curbyte, tmpshort);
			} else {
				tmpshort = curbyte;
				lookbyte();
				(void) printf("\t");
				if (oflag)
					(void) sprintf(object, "%s%03o %03o",
						object, curbyte, tmpshort);
				else
					(void) sprintf(object, "%s%02x %02x",
						object, curbyte, tmpshort);
			}
			(void) sprintf(mneu, "");
			while ((curbyte != 0x00ff) || (tmpshort != 0x00ff)) {
				printline();
				prt_offset();
				(void) printf("\t");
				lookbyte();
				tmpshort = curbyte;
				lookbyte();
				if (oflag)
					(void) sprintf(object, "%s%03o %03o",
						object, curbyte, tmpshort);
				else
					(void) sprintf(object, "%s%02x %02x",
						object, curbyte, tmpshort);
			}
			(void) sprintf(mneu, "***JUMP TABLE END***");
			continue;

		/* float reg */
		case F:
			(void) sprintf(mneu, "%s%%st(%1.1d)", mneu, r_m);
			if (sflag)
				sprintf(symrep, "%%st(%1.1d)", r_m);
			continue;

		/* float reg to float reg, with ret bit present */
		case FF:
			if (opcode2 >> 2 & 0x1) {
				/*
				 * return result bit for 287 instructions
				 * st -> st(i)
				 */
				(void) sprintf(mneu, "%s%%st,%%st(%1.1d)",
				    mneu, r_m);
				if (sflag)
					sprintf(symrep, "%%st,%%st(%1.1d)",
					    r_m);
			} else {
				/* st(i) -> st */
				(void) sprintf(mneu, "%s%%st(%1.1d),%%st",
				    mneu, r_m);
				if (sflag)
					sprintf(symrep, "%%st(%1.1d),%%st",
					    r_m);
			}
			continue;

		/* an invalid op code */
		case AM:
		case DM:
		case OVERRIDE:
		case PREFIX:
		case UNKNOWN:
			bad_opcode();
			continue;

		default:
			(void) printf("dis bug: notify implementor:");
			(void) printf(" case from instruction "
			    "table not found");
			exit(4);
			break;
		} /* end switch */
loop_end:
		;
	}  /* end of for */

	if (errlev >= MAXERRS) {
		(void) printf("dis: %s: %s: section probably not "
		    "text section\n",
			fname, sname);
		(void) printf("\tdisassembly terminated\n");
		exit(4);
	}
}

/*
 *	void get_modrm_byte (mode, reg, r_m)
 *
 *	Get the byte following the op code and separate it into the
 *	mode, register, and r/m fields.
 * Scale-Index-Bytes have a similar format.
 */

void
get_modrm_byte(unsigned *mode, unsigned *reg, unsigned *r_m)
{
	Getbyte();

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

void
check_override(int opindex)
{
	if (overreg) {
		(void) sprintf(operand[opindex], "%s", overreg);
		(void) sprintf(symarr[opindex], "%s", overreg);
	}
	overreg = (char *) 0;
}

/*
 *	void displacement (no_bytes, opindex, value)
 *
 *	Get and print in the 'operand' array a one, two or four
 *	byte displacement from a register.
 */

void
displacement(int no_bytes, int opindex, long *value)
{
	char	temp[(NCPS*2)+1];
	long	val;
	void	check_override(int);

	getbytes(no_bytes, temp, value);
	val = *value;
	if (reljmp) {
		val += loc - loc_start;
		signed_disp = 1;
		reljmp = 0;
	}
	check_override(opindex);
	if (signed_disp) {
		if (val >= 0 || val < 0xfff00000)
			sprintf(temp, (val >= 0 && val < 10) ? "%ld" : "0x%lx",
			    val);
		else
			sprintf(temp, val > -10 ? "-%ld" : "-0x%lx", -val);
		signed_disp = 0;
	}
	(void) sprintf(operand[opindex], "%s%s", operand[opindex], temp);
}

void
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
	char *symstring; /* position in symbolic representation, if any */
	int s_i_b;	/* flag presence of scale-index-byte */
	unsigned ss;    /* scale-factor from opcode */
	unsigned index; /* index register number */
	unsigned base;  /* base register number */
	char indexbuffer[16]; /* char representation of index(es) */

	/* if symbolic representation, skip override prefix, if any */
	check_override(opindex);
	if (sflag)
		symstring = symarr[opindex] + strlen(symarr[opindex]);

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
		if (s_i_b || mode) signed_disp = !oflag;
		displacement(dispsize, opindex, &dispvalue);
	}

	if (s_i_b) {
		register char *basereg = regname32[mode][base];
		if (ss)
			(void) sprintf(indexbuffer, "%s%s,%s", basereg,
				indexname[index], scale_factor[ss]);
		else
			(void) sprintf(indexbuffer, "%s%s", basereg,
				indexname[index]);
		resultreg = indexbuffer;
		format = "%s(%s)";
		if (sflag) {
			regname = regname32[01]; /* Array address */
			if (base == EBP) {
				if (mode == 0) /* Displacement only */
					if (Rel_data)
						search_rel_data(loc, loc,
						    &symstring);
					else
						Lextsympr(dispvalue,
						    &symstring);
				else	/* Displacement off of frame */
					locsympr(dispvalue, base, &symstring);
				if (index != ESP) { /* No index */
					symstring += sprintf(symstring,
					    "+%s", regname[index]);
					if (ss)
						symstring += sprintf(
						    symstring, "*%s",
							scale_factor[ss]);
				}
			} else {
				if (dispsize) {
					if (Rel_data)
						search_rel_data(loc, loc,
						    &symstring);
					else
						Lextsympr(dispvalue,
						    &symstring);
					*symstring++ = '+';
				}
				symstring += sprintf(symstring, "%s",
				    basereg);
				if (index != ESP) {  /* No index */
					symstring += sprintf(symstring, "+%s",
					    regname[index]);
					if (ss)
						symstring += sprintf(
						    symstring, "*%s",
							scale_factor[ss]);
				}
			}
		}
	} else { /* no s-i-b */
		if (mode == REG_ONLY) {
			format = "%s%s";
			if (wbit == MMXOPERAND)
				resultreg = MMXREG[r_m];
			else if (data16)
				resultreg = REG16[r_m][wbit];
			else
				resultreg = REG32[r_m][wbit];
			if (sflag)
				strcpy(symstring, resultreg);
		} else { /* Modes 00, 01, or 10 */
			if (addr16)
				resultreg = regname16[mode][r_m];
			else
				resultreg = regname32[mode][r_m];
			if (r_m == EBP && mode == 0) { /* displacement only */
				format = "%s";
				if (sflag)
					if (Rel_data)
						search_rel_data(loc, loc,
						    &symstring);
					else
						Lextsympr(dispvalue,
						    &symstring);
			} else {
				/*
				* Modes 00, 01, or 10, not displacement only,
				*and no s-i-b
				*/
				format = "%s(%s)";
				if (sflag) {
					if (dispsize == 0) /* Mode 00 */
						(void) strcpy(symstring,
						    resultreg);
					else if (r_m == EBP) {
						/* Array address */
						regname = regname32[01];
						locsympr(dispvalue, r_m,
						    &symstring);
					} else {
					if (Rel_data)
						search_rel_data(loc, loc,
						    &symstring);
					else
						Lextsympr(dispvalue,
						    &symstring);
						(void) sprintf(symstring,
						    "+%s", resultreg);
					}
				}
			}
		}
	}
	(void) sprintf(operand[opindex], format, operand[opindex], resultreg);
}

/*
** getbytes() reads no_bytes from a file and converts them into destbuf.
** A sign-extended value is placed into destvalue if it is non-null.
*/
void
getbytes(int no_bytes, char *destbuf, long *destvalue)
{
	int i;
	unsigned long shiftbuf = 0;
	long value;
	int not_signed;

	for (i = 0; i < no_bytes; i++) {
		Getbyte();
#if I386	/* target is RBO */
#if I386 || vax /* host is RBO */
		shiftbuf |= (long) curbyte << (8*i);
#endif
#if m32		/* host is FBO  */
		shiftbuf = shiftbuf << 8 | curbyte;
#endif
#endif
	}

	switch (no_bytes) {
		case 1:
			if (shiftbuf & 0x80)
				shiftbuf |= 0xffffff00L;
			break;
		case 2:
			if (shiftbuf & 0x8000)
				shiftbuf |= 0xffff0000;
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
	convert(shiftbuf, destbuf, LEAD);
}

/*
 *	void imm_data (no_bytes, opindex)
 *
 *	Determine if 1, 2 or 4 bytes of immediate data are needed, then
 *	get and print them.
 */

void
imm_data(int no_bytes, int opindex)
{
	long value;
	int len = strlen(operand[opindex]);

	operand[opindex][len] = '$';
	getbytes(no_bytes, &operand[opindex][len+1], &value);
	if (sflag) {
		char *symstring = symarr[opindex] + len;
		int len = strlen(symstring);
		symstring[len] = '$';
		symstring += len + 1;
		if (Rel_data)
			search_rel_data(value, value, &symstring);
		else
			Lextsympr(value, &symstring);
	}
}


/*
 *	get_opcode (high, low)
 *	Get the next byte and separate the op code into the high and
 *	low nibbles.
 */

unsigned char savedbytes[4];
int fubar;

void
get_opcode(unsigned *high, unsigned *low)
{
	Getbyte();
	*low = curbyte & 0xf;  /* ----xxxx low 4 bits */
	*high = curbyte >> 4 & 0xf;  /* xxxx---- bits 7 to 4 */
}

void
Getbyte(void)
{
	if (fubar == 0)
		getbyte();
	else {
		curbyte = savedbytes[--fubar];
		loc++;
	}
}

void
savebytes(unsigned val)
{
	int i;

	for (i = 4; --i >= 0; ) {
		savedbytes[i] = val & 0xff;
		val >>= 8;
	}
	fubar = 4;
	loc -= 4;
}

/* 	bad_opcode	*/
/* 	print message and try to recover */

void
bad_opcode(void)
{
	(void) sprintf(mneu, "***ERROR--unknown op code***");
	printline();	/* to print the error message	*/
	/* attempt to resynchronize */
#if 0
	if (resync() == FAILURE)	/* if cannot recover */
		errlev++;		/* stop eventually.  */
#endif
	/*
	 * to prevent extraneous printing when
	 * continuing to the 'for' loop iteration
	 */
	(void) sprintf(object, "");
	(void) sprintf(mneu, "");
}

/*
 *      compofff (lng, temp, temp2)
 *
 *      This routine will compute the location to which control is to be
 *      transferred.  'lng' is the number indicating the jump amount
 *      (already in proper form, meaning masked and negated if necessary)
 *      and 'temp' is a character array which already has the actual
 *      jump amount.  The result computed here will go at the end of 'temp'.
 *      (This is a great routine for people that don't like to compute in
 *      hex arithmetic.)
 */
void
compofff(long lng, char *temp, char *temp2)
{
	char *temp_save = temp;

	lng += loc;
	Lextsympr(lng, &temp);
	if (strncmp(temp_save, "0x", 2))
		sprintf(temp2, "0x%x", lng);
}

void
Lextsympr(long loc, char **symptr)
{
	extsympr((GElf_Sxword)((unsigned long)loc & 0xffffffff), symptr);
}
