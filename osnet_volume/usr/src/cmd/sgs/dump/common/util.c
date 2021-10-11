/*	Copyright (c) 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)util.c	1.10	99/05/04 SMI"

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>

/* Get definitions for the relocation types supported. */
#define	ELF_TARGET_ALL
#include <elf.h>

static const char *Fmtrel = "%-20s";
static const char *Fmtreld = "%-20d";



/*
 * MACHINE DEPENDENT
 *
 * Print the ASCII representation of the ELF relocation type `type' to
 * stdout.  This function should work for any machine type supported by
 * ELF.  Since the set of machine-specific relocation types is machine-
 * specific (hah!), if a machine type or relocation type is not recognized,
 * the decimal value of the relocation type is printed.
 *
 * This function needs to be updated any time the set of machine types
 * supported by ELF is enlarged (tho' it won't malfunction, dump won't
 * be maximally helpful if print_reloc_type() isn't updated).
 */
void
print_reloc_type(int machine, int type)
{
	switch (machine) {
	case EM_M32:
		switch (type) {
		case (R_M32_NONE):
			(void) printf(Fmtrel,
				"R_M32_NONE");
			break;
		case (R_M32_32):
			(void) printf(Fmtrel,
				"R_M32_32");
			break;
		case (R_M32_32_S):
			(void) printf(Fmtrel,
				"R_M32_32_S");
			break;
		case (R_M32_PC32_S):
			(void) printf(Fmtrel,
				"R_M32_PC32_S");
			break;
		case (R_M32_GOT32_S):
			(void) printf(Fmtrel,
				"R_M32_GOT32_S");
			break;
		case (R_M32_PLT32_S):
			(void) printf(Fmtrel,
				"R_M32_PLT32_S");
			break;
		case (R_M32_COPY):
			(void) printf(Fmtrel,
				"R_M32_COPY");
			break;
		case (R_M32_GLOB_DAT):
			(void) printf(Fmtrel,
				"R_M32_GLOB_DAT");
			break;
		case (R_M32_JMP_SLOT):
			(void) printf(Fmtrel,
				"R_M32_JMP_SLOT");
			break;
		case (R_M32_RELATIVE):
			(void) printf(Fmtrel,
				"R_M32_RELATIVE");
			break;
		case (R_M32_RELATIVE_S):
			(void) printf(Fmtrel,
				"R_M32_RELATIVE_S");
			break;
		default:
			(void) printf(Fmtreld, type);
			break;
		}
		break;
	case EM_386:
		switch (type) {
		case (R_386_NONE):
			(void) printf(Fmtrel,
				"R_386_NONE");
			break;
		case (R_386_32):
			(void) printf(Fmtrel,
				"R_386_32");
			break;
		case (R_386_GOT32):
			(void) printf(Fmtrel,
				"R_386_GOT32");
			break;
		case (R_386_PLT32):
			(void) printf(Fmtrel,
				"R_386_PLT32");
			break;
		case (R_386_COPY):
			(void) printf(Fmtrel,
				"R_386_COPY");
			break;
		case (R_386_GLOB_DAT):
			(void) printf(Fmtrel,
				"R_386_GLOB_DAT");
			break;
		case (R_386_JMP_SLOT):
			(void) printf(Fmtrel,
				"R_386_JMP_SLOT");
			break;
		case (R_386_RELATIVE):
			(void) printf(Fmtrel,
				"R_386_RELATIVE");
			break;
		case (R_386_GOTOFF):
			(void) printf(Fmtrel,
				"R_386_GOTOFF");
			break;
		case (R_386_GOTPC):
			(void) printf(Fmtrel,
				"R_386_GOTPC");
			break;
		default:
			(void) printf(Fmtreld, type);
			break;
		}
		break;
	case EM_SPARC:		/* SPARC */
	case EM_SPARC32PLUS:	/* SPARC32PLUS */
	case EM_SPARCV9:	/* SPARC V9 */
		switch (type) {
		case (R_SPARC_NONE):
			(void) printf(Fmtrel,
				"R_SPARC_NONE");
			break;
		case (R_SPARC_8):
			(void) printf(Fmtrel,
				"R_SPARC_8");
			break;
		case (R_SPARC_16):
			(void) printf(Fmtrel,
				"R_SPARC_16");
			break;
		case (R_SPARC_32):
			(void) printf(Fmtrel,
				"R_SPARC_32");
			break;
		case (R_SPARC_DISP8):
			(void) printf(Fmtrel,
				"R_SPARC_DISP8");
			break;
		case (R_SPARC_DISP16):
			(void) printf(Fmtrel,
				"R_SPARC_DISP16");
			break;
		case (R_SPARC_DISP32):
			(void) printf(Fmtrel,
				"R_SPARC_DISP32");
			break;
		case (R_SPARC_WDISP30):
			(void) printf(Fmtrel,
				"R_SPARC_WDISP30");
			break;
		case (R_SPARC_WDISP22):
			(void) printf(Fmtrel,
				"R_SPARC_WDISP22");
			break;
		case (R_SPARC_HI22):
			(void) printf(Fmtrel,
				"R_SPARC_HI22");
			break;
		case (R_SPARC_22):
			(void) printf(Fmtrel,
				"R_SPARC_22");
			break;
		case (R_SPARC_13):
			(void) printf(Fmtrel,
				"R_SPARC_13");
			break;
		case (R_SPARC_LO10):
			(void) printf(Fmtrel,
				"R_SPARC_LO10");
			break;
		case (R_SPARC_GOT10):
			(void) printf(Fmtrel,
				"R_SPARC_GOT10");
			break;
		case (R_SPARC_GOT13):
			(void) printf(Fmtrel,
				"R_SPARC_GOT13");
			break;
		case (R_SPARC_GOT22):
			(void) printf(Fmtrel,
				"R_SPARC_GOT22");
			break;
		case (R_SPARC_PC10):
			(void) printf(Fmtrel,
				"R_SPARC_PC10");
			break;
		case (R_SPARC_PC22):
			(void) printf(Fmtrel,
				"R_SPARC_PC22");
			break;
		case (R_SPARC_WPLT30):
			(void) printf(Fmtrel,
				"R_SPARC_WPLT30");
			break;
		case (R_SPARC_COPY):
			(void) printf(Fmtrel,
				"R_SPARC_COPY");
			break;
		case (R_SPARC_GLOB_DAT):
			(void) printf(Fmtrel,
				"R_SPARC_GLOB_DAT");
			break;
		case (R_SPARC_JMP_SLOT):
			(void) printf(Fmtrel,
				"R_SPARC_JMP_SLOT");
			break;
		case (R_SPARC_RELATIVE):
			(void) printf(Fmtrel,
				"R_SPARC_RELATIVE");
			break;
		case (R_SPARC_UA32):
			(void) printf(Fmtrel,
				"R_SPARC_UA32");
			break;
		case (R_SPARC_PLT32):
			(void) printf(Fmtrel,
				"R_SPARC_PLT32");
			break;
		case (R_SPARC_HIPLT22):
			(void) printf(Fmtrel,
				"R_SPARC_HIPLT22");
			break;
		case (R_SPARC_LOPLT10):
			(void) printf(Fmtrel,
				"R_SPARC_LOPLT10");
			break;
		case (R_SPARC_PCPLT32):
			(void) printf(Fmtrel,
				"R_SPARC_PCPLT32");
			break;
		case (R_SPARC_PCPLT22):
			(void) printf(Fmtrel,
				"R_SPARC_PCPLT22");
			break;
		case (R_SPARC_PCPLT10):
			(void) printf(Fmtrel,
				"R_SPARC_PCPLT10");
			break;
		case (R_SPARC_10):
			(void) printf(Fmtrel,
				"R_SPARC_10");
			break;
		case (R_SPARC_11):
			(void) printf(Fmtrel,
				"R_SPARC_11");
			break;
		case (R_SPARC_64):
			(void) printf(Fmtrel,
				"R_SPARC_64");
			break;
		case (R_SPARC_OLO10):
			(void) printf(Fmtrel,
				"R_SPARC_OLO10");
			break;
		case (R_SPARC_HH22):
			(void) printf(Fmtrel,
				"R_SPARC_HH22");
			break;
		case (R_SPARC_HM10):
			(void) printf(Fmtrel,
				"R_SPARC_HM10");
			break;
		case (R_SPARC_LM22):
			(void) printf(Fmtrel,
				"R_SPARC_LM22");
			break;
		case (R_SPARC_PC_HH22):
			(void) printf(Fmtrel,
				"R_SPARC_PC_HH22");
			break;
		case (R_SPARC_PC_HM10):
			(void) printf(Fmtrel,
				"R_SPARC_PC_HM10");
			break;
		case (R_SPARC_PC_LM22):
			(void) printf(Fmtrel,
				"R_SPARC_PC_LM22");
			break;
		case (R_SPARC_WDISP16):
			(void) printf(Fmtrel,
				"R_SPARC_WDISP16");
			break;
		case (R_SPARC_WDISP19):
			(void) printf(Fmtrel,
				"R_SPARC_WDISP19");
			break;
		case (R_SPARC_GLOB_JMP):
			(void) printf(Fmtrel,
				"R_SPARC_GLOB_JMP");
			break;
		case (R_SPARC_7):
			(void) printf(Fmtrel,
				"R_SPARC_7");
			break;
		case (R_SPARC_5):
			(void) printf(Fmtrel,
				"R_SPARC_5");
			break;
		case (R_SPARC_6):
			(void) printf(Fmtrel,
				"R_SPARC_6");
			break;
		case (R_SPARC_DISP64):
			(void) printf(Fmtrel,
				"R_SPARC_DISP64");
			break;
		case (R_SPARC_PLT64):
			(void) printf(Fmtrel,
				"R_SPARC_PLT64");
			break;
		case (R_SPARC_HIX22):
			(void) printf(Fmtrel,
				"R_SPARC_HIX22");
			break;
		case (R_SPARC_LOX10):
			(void) printf(Fmtrel,
				"R_SPARC_LOX10");
			break;
		case (R_SPARC_H44):
			(void) printf(Fmtrel,
				"R_SPARC_H44");
			break;
		case (R_SPARC_M44):
			(void) printf(Fmtrel,
				"R_SPARC_M44");
			break;
		case (R_SPARC_L44):
			(void) printf(Fmtrel,
				"R_SPARC_L44");
			break;
		case (R_SPARC_REGISTER):
			(void) printf(Fmtrel,
				"R_SPARC_REGISTER");
			break;
		case (R_SPARC_UA64):
			(void) printf(Fmtrel,
				"R_SPARC_UA64");
			break;
		case (R_SPARC_UA16):
			(void) printf(Fmtrel,
				"R_SPARC_UA16");
			break;
		default:
			(void) printf(Fmtreld, type);
			break;
		}
		break;
	case EM_IA_64:	/* Intel IA64 */
		switch (type) {
		case (R_IA_64_NONE):
			(void) printf(Fmtrel,
				"R_IA_64_NONE");
			break;
		case (R_IA_64_IMM14):
			(void) printf(Fmtrel,
				"R_IA_64_IMM14");
			break;
		case (R_IA_64_IMM22):
			(void) printf(Fmtrel,
				"R_IA_64_IMM22");
			break;
		case (R_IA_64_IMM64):
			(void) printf(Fmtrel,
				"R_IA_64_IMM64");
			break;
		case (R_IA_64_DIR32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_DIR32MSB");
			break;
		case (R_IA_64_DIR32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_DIR32LSB");
			break;
		case (R_IA_64_DIR64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_DIR64MSB");
			break;
		case (R_IA_64_DIR64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_DIR64LSB");
			break;
		case (R_IA_64_GPREL22):
			(void) printf(Fmtrel,
				"R_IA_64_GPREL22");
			break;
		case (R_IA_64_GPREL64I):
			(void) printf(Fmtrel,
				"R_IA_64_GPREL64I");
			break;
		case (R_IA_64_GPREL64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_GPREL64MSB");
			break;
		case (R_IA_64_GPREL64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_GPREL64LSB");
			break;
		case (R_IA_64_LTOFF22):
			(void) printf(Fmtrel,
				"R_IA_64_LTOFF22");
			break;
		case (R_IA_64_LTOFF64I):
			(void) printf(Fmtrel,
				"R_IA_64_LTOFF64I");
			break;
		case (R_IA_64_PLTOFF22):
			(void) printf(Fmtrel,
				"R_IA_64_PLTOFF22");
			break;
		case (R_IA_64_PLTOFF64I):
			(void) printf(Fmtrel,
				"R_IA_64_PLTOFF64I");
			break;
		case (R_IA_64_PLTOFF64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_PLTOFF64MSB");
			break;
		case (R_IA_64_PLTOFF64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_PLTOFF64LSB");
			break;
		case (R_IA_64_FPTR64I):
			(void) printf(Fmtrel,
				"R_IA_64_FPTR64I");
			break;
		case (R_IA_64_FPTR32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_FPTR32MSB");
			break;
		case (R_IA_64_FPTR32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_FPTR32LSB");
			break;
		case (R_IA_64_FPTR64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_FPTR64MSB");
			break;
		case (R_IA_64_FPTR64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_FPTR64LSB");
			break;
		case (R_IA_64_PCREL21B):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL21B");
			break;
		case (R_IA_64_PCREL21M):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL21M");
			break;
		case (R_IA_64_PCREL21F):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL21F");
			break;
		case (R_IA_64_PCREL32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL32MSB");
			break;
		case (R_IA_64_PCREL32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL32LSB");
			break;
		case (R_IA_64_PCREL64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL64MSB");
			break;
		case (R_IA_64_PCREL64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_PCREL64LSB");
			break;
		case (R_IA_64_LTOFF_FPTR22):
			(void) printf(Fmtrel,
				"R_IA_64_LTOFF_FPTR22");
			break;
		case (R_IA_64_LTOFF_FPTR64I):
			(void) printf(Fmtrel,
				"R_IA_64_LTOFF_FPTR64I");
			break;
		case (R_IA_64_SEGREL32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_SEGREL32MSB");
			break;
		case (R_IA_64_SEGREL32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_SEGREL32LSB");
			break;
		case (R_IA_64_SEGREL64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_SEGREL64MSB");
			break;
		case (R_IA_64_SEGREL64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_SEGREL64LSB");
			break;
		case (R_IA_64_SECREL32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_SECREL32MSB");
			break;
		case (R_IA_64_SECREL32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_SECREL32LSB");
			break;
		case (R_IA_64_SECREL64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_SECREL64MSB");
			break;
		case (R_IA_64_SECREL64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_SECREL64LSB");
			break;
		case (R_IA_64_REL32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_REL32MSB");
			break;
		case (R_IA_64_REL32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_REL32LSB");
			break;
		case (R_IA_64_REL64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_REL64MSB");
			break;
		case (R_IA_64_REL64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_REL64LSB");
			break;
		case (R_IA_64_LTV32MSB):
			(void) printf(Fmtrel,
				"R_IA_64_LTV32MSB");
			break;
		case (R_IA_64_LTV32LSB):
			(void) printf(Fmtrel,
				"R_IA_64_LTV32LSB");
			break;
		case (R_IA_64_LTV64MSB):
			(void) printf(Fmtrel,
				"R_IA_64_LTV64MSB");
			break;
		case (R_IA_64_LTV64LSB):
			(void) printf(Fmtrel,
				"R_IA_64_LTV64LSB");
			break;
		case (R_IA_64_IPLTMSB):
			(void) printf(Fmtrel,
				"R_IA_64_IPLTMSB");
			break;
		case (R_IA_64_IPLTLSB):
			(void) printf(Fmtrel,
				"R_IA_64_IPLTLSB");
			break;
		default:
			(void) printf(Fmtreld, type);
			break;
		}
		break;
	default:
		(void) printf(Fmtreld, type);
		break;
	}
}
