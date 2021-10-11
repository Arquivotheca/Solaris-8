/*
 * Copyright (c) 1992 - 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "@(#)opsetir.c	1.13	98/08/05 SMI"

#include <sys/types.h>
#include <string.h>
#include "db_as.h"
#include "dis.h"
#include "adb.h"
#include "symtab.h"

#define	MAXERRS	10		/* maximum # of errors allowed before	*/
				/* abandoning this disassembly as a	*/
				/* hopeless case			*/
static int signed_disp;		/*
				 * set to 1 before call to displacement() when
				 * a signed decimal displacement is desired,
				 * as opposed to a hexadecimal one
				 */
static int reljmp;
#define	OPLEN	256		/* maximum length of a single operand	*/
				/* (will be used for printing)		*/
#define	WBIT(x)	(x & 0x1)	/* to get w bit				*/
#define	REGNO(x) (x & 0x7)	/* to get 3 bit register 		*/
#define	VBIT(x)	((x)>>1 & 0x1)	/* to get 'v' bit 			*/
#define	OPSIZE(data16,wbit) ((wbit) ? ((data16) ? 2:4) : 1)

#define	REG_ONLY 3	/* mode indicates a single register with	*/
			/* no displacement is an operand		*/
#define	LONGOPERAND 1	/* value of the w-bit indicating a long		*/
			/* operand (2-bytes or 4-bytes)			*/
#define	MMXOPERAND 2	/* value of the w-bit indicating a mmx reg	*/
/*
 * The following two definistions are different from ESP and EBP 
 * in <sys/reg.h>, these two are from dis.h in dis.
 */
#ifdef EBP
#undef	EBP
#undef	ESP
#endif
#define _EBP 5 	
#define _ESP 4

unsigned short	curbyte;	/* for storing the results of 'getbyte()' */
unsigned short	cur1byte;	/* for storing the results of 'get1byte()' */
unsigned short	cur2bytes;	/* for storing the results of 'get2bytes()' */
unsigned long	cur4bytes;	/* for storing the results of 'get4bytes()' */
char bytebuf[4];
as_addr_t loc;		/* byte location in section being disassembled	*/
			/* IMPORTANT: remember that loc is incremented	*/
			/* only by the getbyte routine			*/
as_addr_t loc_start;	/* starting value of loc			*/
char mneu[NLINE];	/* array to store mnemonic code for output	*/
char sl_name[MAXSYMSIZE];
int errlev = 0;		/* to keep track of errors encountered during	*/
			/* the disassembly, probably due to being out	*/
			/* of sync.					*/
static char operand[3][OPLEN];	/* to store operands as encountered     */
static char symarr[3][OPLEN];
static char *overreg;	/* save the segment override register if any    */
static int data16;	/* 16- or 32-bit data */
static int addr16;	/* 16- or 32-bit addressing */
int	dotinc;

/*
 * Disassemble instructions.
 */

void
printins(modif, disp, inst)
	char modif;
	int disp;
	ins_type inst;
{
	db_printf(5, "printins: called");
	db_dis(dot);
}

/*
 * Disassemble instructions starting at `addr'
 */

#ifndef KADB
static
#endif /* !KADB */
db_dis(as_addr_t addr)
{
	extern _start, etext;
	long l;
	as_addr_t oldaddr, dis_dot();

	db_printf(5, "db_dis: addr=%X", addr);
	errlev = 0;

	adrtoext(addr);
#ifndef KADB
	if (charpos() < 16)
		printf("%16t");
#endif
	oldaddr = addr;
	addr = dis_dot(addr, 0, 0);
	dotinc = addr - oldaddr;
	if (errlev >= MAXERRS) {
		(void) printf("dis: probably not text section\n");
		(void) printf("\tdisassembly terminated\n");
	}
#ifdef KADB
	printf("%-36s", mneu);
#else
	(void) printf("%-s", mneu);
#endif
	sl_name[0] = '\0';
	prassym();

	db_printf(5, "db_dis: returned");
}

/*
 * Disassemble a text section.
 *
 * Using this routine for multi-purposes; using the currently unused second 
 * argument "idsp" to trigger disassembly printout.  This way, we can use
 * this routine to compute the starting offset of the next 
 * instruction.... (cute!)
 */

#ifndef KADB
static as_addr_t
#else /* KADB */
as_addr_t
#endif /* KADB */
dis_dot(ldot, idsp, fmt)
	as_addr_t ldot;
	int idsp;
	char fmt;
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
	extern	struct	instable	opFP7123[4][4];

	extern	char	*REG16[8][2];
	extern	char	*REG32[8][2];
	extern	char	*SEGREG[8];
	extern	char	*DEBUGREG[8];
	extern	char	*CONTROLREG[8];
	extern	char	*TESTREG[8];
	extern	char	*MMXREG[8];
	/* the following entries are from _extn.c	*/
	extern	char	mneu[];
	extern	unsigned short curbyte;
	extern	unsigned short cur2bytes;
	/* the following routines are in _utls.c	*/
	extern	short	sect_name();
	/* libc */
	/* forward */
	void get_modrm_byte();
	void check_override();
	void imm_data();
	void get_opcode();
	void displacement();

	struct instable *dp;
	int wbit, vbit;
	unsigned mode, reg, r_m;
	/* nibbles of the opcode */
	unsigned opcode1, opcode2, opcode3, opcode4, opcode5, opcode6, opcode7;
	unsigned short tmpshort;
	short	sect_num;
	long	lngval;
	char *reg_name;
	int got_modrm_byte;
	char mnemonic[OPLEN];
	char	temp[NCPS+1];
	/* number of bytes of opcode - used to get wbit */
	static char opcode_bytes;

	loc = ldot;
	loc_start = loc;
	mnemonic[0] = '\0';
	mneu[0] = '\0';
	operand[0][0] = '\0';
	operand[1][0] = '\0';
	operand[2][0] = '\0';
	symarr[0][0] = '\0';
	symarr[1][0] = '\0';
	symarr[2][0] = '\0';
	overreg = (char *) 0;
	data16 = addr16 = 0;
	opcode_bytes = 0;

	/*
	 * An instruction is disassembled with each iteration of the
	 * following loop.  The loop is terminated upon completion of the
	 * section (loc minus the section's physical address becomes equal
	 * to the section size) or if the number of bad op codes encountered
	 * would indicate this disassembly is hopeless.
	 *
	 * As long as there is a prefix, the default segment register,
	 * addressing-mode, or data-mode in the instruction will be overridden.
	 * This may be more general than the chip actually is.
	 */
	for(;;) {
		get_opcode(&opcode1, &opcode2);
		db_printf(9, "dis_dot: loc=%X, ldot=%X, dotinc=%X, opcode1=%X, opcode2=%X",loc, ldot, dotinc, opcode1, opcode2);
		dp = &distable[opcode1][opcode2];
		db_printf(9, "dis_dot: adr_mode=%X, name='%s'", dp->adr_mode, dp->name);
		if (dp->adr_mode == PREFIX)
			strcat(mnemonic, dp->name);
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
	 * Some 386 instructions have 2 bytes of opcode before the mod_r/m
	 * byte so we need to perform a table indirection.
	 */
	if (dp->indirect == (struct instable *) op0F) {
		get_opcode(&opcode4,&opcode5);
		db_printf(9, "dis_dot: loc=%X, ldot=%X, dotinc=%X, opcode4=%X, opcode5=%X",loc, ldot, dotinc, opcode4, opcode5);
		opcode_bytes = 2;
		if ((opcode4 == 0x7) &&
			    ((opcode5 >= 0x1) && (opcode5 <= 0x3))) {
			get_opcode(&opcode6,&opcode7);
			opcode_bytes = 3;
			dp = &opFP7123[opcode5][opcode6 & 0x3];
		} else if ((opcode4 == 0xc) && (opcode5 >= 0x8))
			dp = &opFC8[0];
		else
			dp = &op0F[opcode4][opcode5];
	}

	got_modrm_byte = 0;
	if (dp->indirect != TERM) {
		/* This must have been an opcode for which several
		 * instructions exist.  The opcode3 field further decodes
		 * the instruction.
		 */
		got_modrm_byte = 1;
		get_modrm_byte(&mode, &opcode3, &r_m);
		db_printf(9, "dis_dot: loc=%X, ldot=%X, dotinc=%X, opcode3=%X, mode=%X, r_m=%X",loc, ldot, dotinc, opcode3, mode, r_m);

		/*
		 * decode 287 instructions (D8-DF) from opcodeN
		 */
		if (opcode1 == 0xD && opcode2 >= 0x8) {
			/* instruction form 5 */
			if (opcode2 == 0xB && mode == 0x3 && opcode3 == 4)
				dp = &opFP5[r_m];
			else if (opcode2 == 0xA && mode == 0x3 && opcode3 < 4)
				dp = &opFP7[opcode3];
			else if (opcode2 == 0xB && mode == 0x3)
				dp = &opFP6[opcode3];
			/* instruction form 4 */
			else if (opcode2==0x9 && mode==0x3 && opcode3 >= 4)
				dp = &opFP4[opcode3-4][r_m];
			/* instruction form 3 */
			else if (mode == 0x3)
				dp = &opFP3[opcode2-8][opcode3];
			/* instruction form 1 and 2 */
			else
				dp = &opFP1n2[opcode2-8][opcode3];
		} else
			dp = dp -> indirect + opcode3;
			/* now dp points the proper subdecode table entry */
	}

	if (dp->indirect != TERM) {
		sprintf(mneu,"***** Error - bad opcode\n");
		errlev++;
		return loc;
	}

	/* print the mnemonic */
	if (dp->adr_mode != CBW  && dp->adr_mode != CWD) {
		(void) strcat(mnemonic, dp -> name);  /* print the mnemonic */
		if (dp->suffix)
			(void) strcat(mnemonic, (data16? "w" : "l"));
		(void) sprintf(mneu, (strlen(mnemonic)<7 ? "%-7s" : "%-7s "),
		               mnemonic);
	}


	/*
	 * Each instruction has a particular instruction syntax format
	 * stored in the disassembly tables.  The assignment of formats
	 * to instructins was made by the author.  Individual formats
	 * are explained as they are encountered in the following
	 * switch construct.
	 */

	switch(dp -> adr_mode){

	/* movsbl movsbw (0x0FBE) or movswl (0x0FBF) */
	/* movzbl movzbw (0x0FB6) or mobzwl (0x0FB7) */
	/* wbit lives in 2nd byte, note that operands are different sized */
	case MOVZ:
		/* Get second operand first so data16 can be destroyed */
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];

		wbit = WBIT(opcode5);
		data16 = 1;
		get_operand(mode, r_m, wbit, 0);
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* imul instruction, with either 8-bit or longer immediate */
	case IMUL:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 1);
		/* opcode 0x6B for byte, sign-extended displacement, 0x69 for word(s)*/
		imm_data(OPSIZE(data16,opcode2 == 0x9), 0);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];
		(void) sprintf(mneu,"%s%s,%s,%s",
					mneu,operand[0],operand[1],reg_name);
		return loc;

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
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

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
		(void) sprintf(mneu,"%s%s,%s",mneu,reg_name,operand[0]);
		return loc;

	/* special case for the Pentium xaddb instruction */
	case XADDB:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, 0, 0);
		if (data16)
			reg_name = REG16[reg][0];
		else
			reg_name = REG32[reg][0];
		(void) sprintf(mneu,"%s%s,%s",mneu,reg_name,operand[0]);
		return loc;

	/* MMX register to memory or register operand		*/
	case MMXS:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
		    MMXOPERAND : LONGOPERAND, 0);
		reg_name = MMXREG[reg];
		(void) sprintf(mneu,"%s%s,%s",mneu,reg_name,operand[0]);
		return loc;

	/* Double shift. Has immediate operand specifying the shift. */
	case DSHIFT:
		get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 1);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];
		imm_data(1, 0);
		sprintf(mneu,"%s%s,%s,%s",mneu,operand[0],reg_name,operand[1]);
		return loc;

	/* Double shift. With no immediate operand, specifies using %cl. */
	case DSHIFTcl:
		get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 0);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];
		sprintf(mneu,"%s%s,%s",mneu,reg_name,operand[0]);
		return loc;

	/* immediate to memory or register operand */
	case IMlw:
		wbit = WBIT(opcode2);
		get_operand(mode, r_m, wbit, 1);
		/* A long immediate is expected for opcode 0x81, not 0x80 nor 0x83 */
		imm_data(OPSIZE(data16,opcode2 == 1), 0);
		sprintf(mneu,"%s%s,%s",mneu,operand[0],operand[1]);
		return loc;

	/* immediate to memory or register operand with the	*/
	/* 'w' bit present					*/
	case IMw:
		wbit = WBIT(opcode2);
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, wbit, 1);
		imm_data(OPSIZE(data16,wbit), 0);
		sprintf(mneu,"%s%s,%s",mneu,operand[0],operand[1]);
		return loc;

	/* immediate to register with register in low 3 bits	*/
	/* of op code						*/
	case IR:
		wbit = opcode2 >>3 & 0x1; /* w-bit here (with regs) is bit 3 */
		reg = REGNO(opcode2);
		imm_data(OPSIZE(data16,wbit), 0);
		if (data16)
			reg_name = REG16[reg][wbit];
		else
			reg_name = REG32[reg][wbit];
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* MMX immediate shift of register			*/
	case MMXSH:
		reg = REGNO(opcode7);
		imm_data(1, 0);
		reg_name = MMXREG[reg];
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* memory operand to accumulator			*/
	case OA:
		wbit = WBIT(opcode2);
		displacement(OPSIZE(addr16,LONGOPERAND), 0,&lngval);
		reg_name = (data16 ? REG16 : REG32)[0][wbit];
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* accumulator to memory operand			*/
	case AO:
		wbit = WBIT(opcode2);
		
		displacement(OPSIZE(addr16,LONGOPERAND), 0,&lngval);
		reg_name = (addr16 ? REG16 : REG32)[0][wbit];
		(void) sprintf(mneu,"%s%s,%s",mneu, reg_name, operand[0]);
		return loc;

	/* memory or register operand to segment register	*/
	case MS:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 0);
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],SEGREG[reg]);
		return loc;

	/* segment register to memory or register operand	*/
	case SM:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 0);
		(void) sprintf(mneu,"%s%s,%s",mneu,SEGREG[reg],operand[0]);
		return loc;

	/* rotate or shift instrutions, which may shift by 1 or */
	/* consult the cl register, depending on the 'v' bit	*/
	case Mv:
		vbit = VBIT(opcode2);
		wbit = WBIT(opcode2);
		get_operand(mode, r_m, wbit, 0);
		/* When vbit is set, register is an operand, otherwise just $0x1 */
		reg_name = vbit ? "%cl," : "$1," ;
		(void) sprintf(mneu,"%s%s%s",mneu, reg_name, operand[0]);
		return loc;

	/* immediate rotate or shift instrutions, which may or */
	/* may not consult the cl register, depending on the 'v' bit	*/
	case MvI:
		vbit = VBIT(opcode2);
		wbit = WBIT(opcode2);
		get_operand(mode, r_m, wbit, 0);
		imm_data(1,1);
		/* When vbit is set, register is an operand, otherwise just $0x1 */
		reg_name = vbit ? "%cl," : "" ;
		(void) sprintf(mneu,"%s%s,%s%s",mneu,operand[1], reg_name, operand[0]);
		return loc;

	case MIb:
		get_operand(mode, r_m, LONGOPERAND, 0);
		imm_data(1,1);
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[1], operand[0]);
		return loc;

	/* single memory or register operand with 'w' bit present*/
	case Mw:
		wbit = WBIT(opcode2);
		get_operand(mode, r_m, wbit, 0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

	/* single memory or register operand			*/
	case M:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

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
		
		(void) sprintf(mneu, "%s%s,%s", mneu, reg_name, operand[0]);
		return loc;

	/* single register operand with register in the low 3	*/
	/* bits of op code					*/
	case R:
		if (opcode_bytes == 2)
			reg = REGNO(opcode5);
		else
			reg = REGNO(opcode2);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];
		(void) sprintf(mneu,"%s%s",mneu,reg_name);
		return loc;

	/* register to accumulator with register in the low 3	*/
	/* bits of op code, xchg instructions                   */
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
		(void) sprintf(mneu,"%s%s,%%%sax",
			mneu,reg_name,eprefix);
		return loc;
	}

	/* single segment register operand, with register in	*/
	/* bits 3-4 of op code					*/
	case SEG:
		reg = curbyte >> 3 & 0x3; /* segment register */
		(void) sprintf(mneu,"%s%s",mneu,SEGREG[reg]);
		return loc;

	/* single segment register operand, with register in	*/
	/* bits 3-5 of op code					*/
	case LSEG:
		reg = curbyte >> 3 & 0x7; /* long seg reg from opcode */
		(void) sprintf(mneu,"%s%s",mneu,SEGREG[reg]);
		return loc;

	/* memory or register operand to register		*/
	case MR:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, LONGOPERAND, 0);
		if (data16)
			reg_name = REG16[reg][LONGOPERAND];
		else
			reg_name = REG32[reg][LONGOPERAND];
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* MMX memory or register operand to register		*/
	case MMXL:
		if (!got_modrm_byte)
			get_modrm_byte(&mode, &reg, &r_m);
		get_operand(mode, r_m, strcmp(mnemonic, "movd") ?
		    MMXOPERAND : LONGOPERAND, 0);
		reg_name = MMXREG[reg];
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],reg_name);
		return loc;

	/* immediate operand to accumulator			*/
	case IA: {
		int no_bytes = OPSIZE(data16,WBIT(opcode2));
		switch (no_bytes) {
			case 1: reg_name = "%al"; break;
			case 2: reg_name = "%ax"; break;
			case 4: reg_name = "%eax"; break;
		}
		imm_data(no_bytes, 0);
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0], reg_name);
		return loc;
	}
	/* memory or register operand to accumulator		*/
	case MA:
		wbit = WBIT(opcode2);
		get_operand(mode, r_m, wbit, 0);
		reg_name = (data16 ? REG16 : REG32)[0][wbit];
		(void) sprintf(mneu,"%s%s,%s",mneu, operand[0], reg_name);
		return loc;

	/* si register to di register				*/
	case SD:
		check_override(0);
		(void) sprintf(mneu,"%s%s(%%%ssi),(%%%sdi)",mneu,operand[0],
			addr16? "" : "e" , addr16? "" : "e");
		return loc;

	/* accumulator to di register				*/
	case AD:
		wbit = WBIT(opcode2);
		check_override(0);
		reg_name = (data16 ? REG16 : REG32)[0][wbit] ;
		(void) sprintf(mneu,"%s%s,%s(%%%sdi)",mneu, reg_name, operand[0],
			addr16? "" : "e");
		return loc;

	/* si register to accumulator				*/
	case SA:
		wbit = WBIT(opcode2);
		check_override(0);
		reg_name = (addr16 ? REG16 : REG32)[0][wbit] ;
		(void) sprintf(mneu,"%s%s(%%%ssi),%s",mneu,operand[0],
			addr16? "" : "e", reg_name);
		return loc;

	/* single operand, a 16/32 bit displacement		*/
	/* added to current offset by 'compoff'			*/
	case D:
		reljmp = 1;
		displacement(OPSIZE(data16,LONGOPERAND), 0, &lngval);
		compoff(lngval, operand[1]);
		if (lngval == -4)
			(void) sprintf(mneu, "%s<extern>", mneu);
		else
			(void) sprintf(mneu, *operand[0] == '-' ?
			    "%s.%s <%s>" : "%s.+%s <%s>", mneu,
			    operand[0], operand[1]);
		return loc;

	/* indirect to memory or register operand		*/
	case INM:
		get_operand(mode, r_m, LONGOPERAND, 0);
		(void) sprintf(mneu,"%s*%s",mneu,operand[0]);
		return loc;

	/* for long jumps and long calls -- a new code segment   */
	/* register and an offset in IP -- stored in object      */
	/* code in reverse order                                 */
	case SO:
		displacement(OPSIZE(addr16,LONGOPERAND), 1,&lngval);
		/* will now get segment operand*/
		displacement(2, 0,&lngval);
		(void) sprintf(mneu,"%s$%s,$%s",mneu,operand[0],operand[1]);
		return loc;

	/* jmp/call. single operand, 8 bit displacement.	*/
	/* added to current EIP in 'compoff'			*/
	case BD:
		reljmp = 1;
		displacement(1, 0, &lngval);
		compoff(lngval, operand[1]);
		if (lngval == -4)
			(void) sprintf(mneu, "%s<extern>", mneu);
		else
			(void) sprintf(mneu, *operand[0] == '-' ?
			    "%s.%s <%s>" : "%s.+%s <%s>", mneu,
			    operand[0], operand[1]);
		return loc;

	/* single 32/16 bit immediate operand			*/
	case I:
		imm_data(OPSIZE(data16,LONGOPERAND), 0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

	/* single 8 bit immediate operand			*/
	case Ib:
		imm_data(1, 0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

	case ENTER:
		imm_data(2,0);
		imm_data(1,1);
		(void) sprintf(mneu,"%s%s,%s",mneu,operand[0],operand[1]);
		return loc;

	/* 16-bit immediate operand */
	case RET:
		imm_data(2,0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

	/* single 8 bit port operand				*/
	case P:
		check_override(0);
		imm_data(1, 0);
		(void) sprintf(mneu,"%s%s",mneu,operand[0]);
		return loc;

	/* single operand, dx register (variable port instruction)*/
	case V:
		check_override(0);
		(void) sprintf(mneu,"%s%s(%%dx)",mneu,operand[0]);
		return loc;

	/* The int instruction, which has two forms: int 3 (breakpoint) or  */
	/* int n, where n is indicated in the subsequent byte (format Ib).  */
	/* The int 3 instruction (opcode 0xCC), where, although the 3 looks */
	/* like an operand, it is implied by the opcode. It must be converted */
	/* to the correct base and output. */
	case INT3:
		dis_convert(3, temp, LEAD);
		(void) sprintf(mneu,"%s$%s",mneu,temp);
		return loc;

	/* an unused byte must be discarded			*/
	case U:
		getbyte();
		return loc;

	case CBW:
		if (data16)
			(void) strcat(mneu,"cbtw");
		else
			(void) strcat(mneu,"cwtl");
		return loc;

	case CWD:
		if (data16)
			(void) strcat(mneu,"cwtd");
		else
			(void) strcat(mneu,"cltd");
		return loc;

	/* no disassembly, the mnemonic was all there was	*/
	/* so go on						*/
	case GO_ON:
		return loc;

	/* Special byte indicating a the beginning of a 	*/
	/* jump table has been seen. The jump table addresses	*/
	/* will be printed until the address 0xffff which	*/
	/* indicates the end of the jump table is read.		*/
	case JTAB:
		(void) sprintf(mneu,"***JUMP TABLE BEGINNING***");
		printline();
		lookbyte();
		if (curbyte == FILL) {
			(void) sprintf(mneu,"FILL BYTE FOR ALIGNMENT");
			printline();
			(void) printf("\t");
			lookbyte();
			tmpshort = curbyte;
			lookbyte();
		}
		else {
			tmpshort = curbyte;
			lookbyte();
			(void) printf("\t");
		}
		(void) sprintf(mneu,"");
		while ((curbyte != 0x00ff) || (tmpshort != 0x00ff)) {
			printline();
			(void) printf("\t");
			lookbyte();
			tmpshort = curbyte;
			lookbyte();
		}
		(void) sprintf(mneu,"***JUMP TABLE END***");
		return loc;

	/* float reg */
	case F:
		(void) sprintf(mneu,"%s%%st(%1.1d)",mneu,r_m);
		return loc;

	/* float reg to float reg, with ret bit present */
	case FF:
		if (opcode2 >> 2 & 0x1) {
			/* return result bit for 287 instructions	*/
			/* st -> st(i) */
			(void) sprintf(mneu,"%s%%st,%%st(%1.1d)",mneu,r_m);
		}
		else {
			/* st(i) -> st */
			(void) sprintf(mneu,"%s%%st(%1.1d),%%st",mneu,r_m);
		}
		return loc;

	/* an invalid op code */
	case AM:
	case DM:
	case OVERRIDE:
	case PREFIX:
	case UNKNOWN:
		sprintf(mneu,"***** Error - bad opcode\n");
		errlev++;
		return loc;

	default:
		(void) printf("dis bug: notify implementor:");
		(void) printf(" case from instruction table not found");
		return loc;
	} /* end switch */

	/*NOTREACHED*/
}


/*
 * Get the byte following the op code and separate it into the
 * mode, register, and r/m fields.
 * Scale-Index-Bytes have a similar format.
 */

#ifndef KADB
static void
#else /* KADB */
void
#endif /* KADB */
get_modrm_byte(mode, reg, r_m)
	unsigned *mode;
	unsigned *reg;
	unsigned *r_m;
{
	extern	unsigned short curbyte;	/* in _extn.c */

	getbyte();

	*r_m = curbyte & 0x7;		/* r/m field from curbyte */
	*reg = curbyte >> 3 & 0x7;	/* register field from curbyte */
	*mode = curbyte >> 6 & 0x3;	/* mode field from curbyte */
}

/*
 * Check to see if there is a segment override prefix pending.
 * If so, print it in the current 'operand' location and set
 * the override flag back to false.
 */

#ifndef KADB
static void
#else /* KADB */
void
#endif /* KADB */
check_override(opindex)
	int opindex;
{
	if (overreg) {
		(void) sprintf(operand[opindex],"%s",overreg);
		(void) sprintf(symarr[opindex],"%s",overreg);
	}
	overreg = (char *) 0;
}

/*
 * Get and print in the 'operand' array a one, two or four
 * byte displacement from a register.
 */

void
displacement(no_bytes, opindex, value)
	int no_bytes;
	int opindex;
	long *value;
{
	char temp[(NCPS*2)+1];
	long val;
	void check_override();

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
			sprintf(temp, "0x%lx",
			    val);
		else
			sprintf(temp, "-0x%lx", -val);
		signed_disp = 0;
	}
	(void) sprintf(operand[opindex],"%s%s",operand[opindex],temp);
}

#ifndef KADB
static
#endif /* !KADB */
get_operand(mode, r_m, wbit, opindex)
	unsigned mode;
	unsigned r_m;
	int wbit;
	int opindex;
{
	extern	char	dispsize16[8][4];	/* tables.c */
	extern	char	dispsize32[8][4];
	extern	char	*regname16[4][8];
	extern	char	*regname32[4][8];
	extern	char	*indexname[8];
	extern	char 	*REG16[8][2];	  /* in tables.c */
	extern	char 	*REG32[8][2];	  /* in tables.c */
	extern	char 	*MMXREG[8];
	extern	char	*scale_factor[4];	  /* in tables.c */
	int dispsize;   /* size of displacement in bytes */
	int dispvalue;  /* value of the displacement */
	char *resultreg; /* representation of index(es) */
	char *format;   /* output format of result */
	char *symstring; /* position in symbolic representation, if any */
	int s_i_b;      /* flag presence of scale-index-byte */
	unsigned ss;    /* scale-factor from opcode */
	unsigned index; /* index register number */
	unsigned base;  /* base register number */
	char indexbuffer[16]; /* char representation of index(es) */

	/* if symbolic representation, skip override prefix, if any */
	check_override(opindex);

	/* check for the presence of the s-i-b byte */
	if (r_m==_ESP && mode!=REG_ONLY && !addr16) {
		s_i_b = TRUE;
		get_modrm_byte(&ss, &index, &base);
	} else
		s_i_b = FALSE;

	if (addr16)
		dispsize = dispsize16[r_m][mode];
	else
		dispsize = dispsize32[r_m][mode];

	if (s_i_b && mode==0 && base==_EBP)
		dispsize = 4;

	if (dispsize != 0) {
		if (s_i_b || mode) signed_disp = 1;
		displacement(dispsize, opindex, &dispvalue);
	}

	if (s_i_b) {
		register char *basereg = regname32[mode][base];
		(void) sprintf(indexbuffer, "%s%s,%s", basereg,
			indexname[index], scale_factor[ss]);
		resultreg = indexbuffer;
		format = "%s(%s)";
	} else { /* no s-i-b */
		if (mode == REG_ONLY) {
			format = "%s%s";
			if (wbit == MMXOPERAND)
				resultreg = MMXREG[r_m];
			else if (data16)
				resultreg = REG16[r_m][wbit] ;
			else
				resultreg = REG32[r_m][wbit] ;
		} else { /* Modes 00, 01, or 10 */
			if (addr16)
				resultreg = regname16[mode][r_m];
			else
				resultreg = regname32[mode][r_m];
			if (r_m ==_EBP && mode == 0) { /* displacement only */
				format = "%s";
			} else { /* Modes 00, 01, or 10, not displacement only, and no s-i-b */
				format = "%s(%s)";
			}
		}
	}
	(void) sprintf(operand[opindex],format,operand[opindex], resultreg);
}

/*
 * getbytes() reads no_bytes from a file and converts them into destbuf.
 * A sign-extended value is placed into destvalue if it is non-null.
 */

#ifndef KADB
static
#endif /* !KADB */
getbytes(no_bytes, destbuf, destvalue)
	int no_bytes;
	char *destbuf;
	long *destvalue;
{
	extern	unsigned short curbyte;	/* from _extn.c */

	int i;
	unsigned long shiftbuf = 0;
	char *format;
	long value;
	int not_signed;

	for (i=0; i<no_bytes; i++) {
		getbyte();
		shiftbuf |= (long) curbyte << (8*i);
	}
#ifndef KADB
	format = "0x%lx";
#endif
	switch(no_bytes) {
		case 1:
#ifdef KADB
			format = "0x%2lx";
#endif
			if (shiftbuf & 0x80)
				shiftbuf |= 0xffffff00L;
			break;
		case 2:
#ifdef KADB
			format = "0x%4lx";
#endif
			if (shiftbuf & 0x8000)
				shiftbuf |= 0xffff0000L;
			break;
		case 4:
#ifdef KADB
			format = "0x%8lx";
#endif
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
	sprintf(destbuf,format,shiftbuf);
}

/*
 * Determine if 1, 2 or 4 bytes of immediate data are needed, then
 * get and print them.
 */

#ifndef KADB
static void
#else /* KADB */
void
#endif /* KADB */
imm_data(no_bytes, opindex)
	int no_bytes;
	int opindex;
{
	long value;
	int len = strlen(operand[opindex]);

	operand[opindex][len] = '$';
	getbytes(no_bytes, &operand[opindex][len+1], &value);
}

/*
 * Get the next byte and separate the op code into the high and low nibbles.
 */

#ifndef KADB
static void
#else /* KADB */
void
#endif /* KADB */
get_opcode(high, low)
	unsigned *high;
	unsigned *low;		/* low 4 bits of op code   */
{
	extern	unsigned short curbyte;		/* from _extn.c */

	getbyte();
	*low = curbyte & 0xf;  		/* ----xxxx low 4 bits */
	*high = curbyte >> 4 & 0xf;	/* xxxx---- bits 7 to 4 */
}

#define	BADADDR	-1L	/* used by the resynchronization	*/
			/* function to indicate that a restart	*/
			/* candidate has not been found		*/

/*
 * This routine will compute the location to which control is to be
 * transferred.  'lng' is the number indicating the jump amount
 * (already in proper form, meaning masked and negated if necessary)
 * and 'temp' is a character array which already has the actual
 * jump amount.  The result computed here will go at the end of 'temp'.
 * (This is a great routine for people that don't like to compute in
 * hex arithmetic.)
 */

#ifndef KADB
static
#endif /* !KADB */
compoff(lng, temp)
	long	lng;
	char	*temp;
{
	lng += (long) loc;
	sprintf(temp,"0x%lx",lng);
}

/*
 * Convert the passed number to hex
 * leaving the result in the supplied string array.
 * If  LEAD  is specified, precede the number with '0x' to
 * indicate the base (used for information going to the mnemonic
 * printout).  NOLEAD  will be used for all other printing (for
 * printing the offset, object code, and the second byte in two
 * byte immediates, displacements, etc.) and will assure that
 * there are leading zeros.
 */

#ifndef KADB
static
#endif /* !KADB */
dis_convert(num,temp,flag)
unsigned	num;
	char	temp[];
	int	flag;
{

	if (flag == NOLEAD) 
		sprintf(temp,"%4x",num);
	if (flag == LEAD)
		sprintf(temp,"0x%x",num);
	if (flag == NOLEADSHORT)
		sprintf(temp,"%2x",num);
}

/*
 * Read a byte, mask it, then return the result in 'curbyte'.
 * The getting of all single bytes is done here.  The 'getbyte[s]'
 * routines are the only place where the global variable 'loc'
 * is incremented.
 */

#ifndef KADB
static
#endif /* !KADB */
getbyte(void)
{
	extern	unsigned short curbyte;		/* from _extn.c */
	char	byte;

	byte = dis_chkget(loc);
	loc++;
	curbyte = byte & 0377;
}


/*
 * Read a byte, mask it, then return the result in 'curbyte'.
 * loc is incremented.
 */

#ifndef KADB
static
#endif /* !KADB */
lookbyte(void)
{
	extern	unsigned short curbyte;		/* from _extn.c */
	char	byte;

	byte = dis_chkget(loc);
	loc++;
	curbyte = byte & 0377;
}

/*
 * Print the disassembled line, consisting of the object code
 * and the mnemonics.  The breakpointable line number, if any,
 * has already been printed.
 */

#ifndef KADB
static
#endif /* !KADB */
printline(void)
{
	extern	char	mneu[];

	printf("%s",mneu); /* to print hex */
}

/*
 * Get the byte at `addr' if it is a legal address.
 */

#ifndef KADB
static
#endif /* !KADB */
dis_chkget(addr)
	as_addr_t	addr;
{
	return chkget(addr, ISP);
}

#ifndef KADB
static
#endif /* !KADB */
adrtoext(addr)
	int addr;
{
	/* The "1" shd be DSYM or ISYM */
	(void) ssymoff(addr, 1, sl_name, sizeof (sl_name));
	return;
}

typedef long		L_INT;
typedef unsigned long	ADDR;

char *findsymname();

/*
 *	UNIX debugger
 *
 *		Instruction printing routines.
 *		MACHINE DEPENDENT.
 *		Tweaked for i386.
 */

/* prassym(): symbolic printing of disassembled instruction */

#ifndef KADB
static
#endif /* !KADB */
prassym(void)
{
	int cnt, jj;
	long value, diff;
	register char *os;
	char *pos;
	int neg;

	extern	char	mneu[];		/* in dis/extn.c */

	db_printf(6, "prassym: called");
	/* depends heavily on format output by disassembler */
	cnt = 0;
	os = mneu;	/* instruction disassembled by dis_dot() */
	while(*os != '\t' && *os != ' ' && *os != '\0')
		os++;		/* skip over instr mneumonic */
	while (*os) {
		while(*os == '\t' || *os == ',' || *os == ' ')
			os++;
		value = jj = 0;
		neg = 0;
		pos = os;
		switch (*os) {
		/*
		** This counts on disassembler not lying about
		** lengths of displacements.
		*/
		case '-':
			neg = 1;
			/* fall through */
		case '*':
		case '$':
			pos++;
			/* fall through */

		case '0':
			value = strtoul(pos,&pos,0);
			jj = (value != 0);
			if (*pos != '(')
				break;
			/* fall through */

		case '(':
			while (*pos != ')')
				pos++;
			os = pos;
			break;

		case '.':
		case '+':
			while(*os != '\t' && *os != ' ' && *os != '\0')
				os++;
			if ((os[0] == ' ' || os[0] == '\t') && os[1] == '<') {
				char *cp;

				value = strtoul(os + 2, &cp, 16);
				if (*cp == '>')
					jj = 1;
				os = cp;
			}
			if (value == 0) /* probably a .o, unrelocated displacement*/
				jj = 0;
			break;
		}
		if (neg)
			value = -value;
		if (jj > 0 && (diff = adrtoext((ADDR) value)) != -1) {
			if (cnt < 0) {
				printf(" [-,");
				cnt = 1;
			} else if (cnt++ == 0)
				printf(" [");
			else
				printf(",");
			printf("%s", sl_name);
		} else if (cnt > 0)
			printf(",-");
		else
			--cnt;
		while(*os != '\t' && *os != ',' && *os != ' ' && *os != '\0')
			os++;
	} /* for */
	if (cnt > 0)
		printf("]");
}

#if defined(KADB)

/*
 * scaled down version of sprintf.  Only handles %s and %x
 */
char *gcp;

static
sprintf(cp, fmt, arg)
	char *cp, *fmt, *arg;
{
	char **ap, *str, c;
	long l;
	int prec;

	ap = &arg;
	while (c = *fmt)
		switch (*fmt++) {

		case '\\':
			switch (*fmt++) {

			case 'n':
				*cp++ = '\n';
				break;
			case '0':
				*cp++ = '\0';
				break;
			case 'b':
				*cp++ = '\b';
				break;
			case 't':
				*cp++ = '\t';
				break;
			default:
				cp++;

			}
			break;
		case '%':
			prec = 0;
again:
			switch (*fmt++) {

			case '%':
				*cp++ = '%';
				break;
			case 's':
				str = *ap++;
				while (*str) {
					prec--;
					*cp++ = *str++;
				}
				while (prec-- > 0)
					*cp++ = ' ';
				break;
			case 'x':
				l = *(long *)ap;
				ap++;
				gcp = cp;
				hex(l, prec);
				cp = gcp;
				break;
			case '0':	case '1':	case '2':
			case '3':	case '4':	case '5':
			case '6':	case '7':	case '8':
			case '9':
				prec = (prec * 10) + (*(fmt - 1) - '0');
			case '-':
			case 'l':
			case '.':
				goto again;
			default:
				break;

			}
			break;
		default:
			*cp++ = c;
			break;

		}
	*cp = '\0';
	return;
}

static
hex(l, n)
	unsigned long l;
	int n;
{

	if (l > 15 || n > 1)
		hex(l >> 4, n - 1);
	*gcp++ = "0123456789abcdef"[l & 0xf];
	return;
}

static char *
strchr(cp, c)
	char *cp, c;
{

	while (*cp)
		if (*cp == c)
			return cp;
		else
			cp++;
	return (char *) 0;
}

strtoul(sp, cp, base)
	const char *sp;
	char **cp;
	int base;
{
	register int n;
	char c;

	if (base == 0)
		base = 10;
	n = 0;
	if (*sp == '0') {
		sp++;
		base = 8;
		if (*sp == 'x') {
			sp++;
			base = 16;
		}
	}
	while ((*sp >= '0' && *sp <= '9') ||
	       (*sp >= 'a' && *sp <= 'f')) {
		if ((c = *sp++) >= 'a' && c <= 'f')
			c -= 'a' - '9' - 1;
		n = (n * base) + (c - '0');
	}
	if (cp)
		*cp = (char *)sp;
	return n;
}

#endif /* defined(KADB) */
