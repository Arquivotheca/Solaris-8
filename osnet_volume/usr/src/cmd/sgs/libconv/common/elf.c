/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.14	99/05/04 SMI"

/*
 * String conversion routines for ELF header attributes.
 */
#include	<stdio.h>
#include	<string.h>
#include	"_conv.h"
#include	"elf_msg.h"
#include	<sys/elf_SPARC.h>

static const Msg classes[] = {
	MSG_ELFCLASSNONE,	MSG_ELFCLASS32,		MSG_ELFCLASS64
};

const char *
conv_eclass_str(Byte class)
{
	static char	string[STRSIZE] = { '\0' };

	if (class >= ELFCLASSNUM)
		return (conv_invalid_str(string, (Lword)class, 0));
	else
		return (MSG_ORIG(classes[class]));

}

static const Msg datas[] = {
	MSG_ELFDATANONE,	MSG_ELFDATA2LSB, 	MSG_ELFDATA2MSB
};

const char *
conv_edata_str(Byte data)
{
	static char	string[STRSIZE] = { '\0' };

	if (data >= ELFDATANUM)
		return (conv_invalid_str(string, (Lword)data, 0));
	else
		return (MSG_ORIG(datas[data]));

}

static const Msg machines[] = {
	MSG_EM_NONE,		MSG_EM_M32,		MSG_EM_SPARC,
	MSG_EM_386,		MSG_EM_68K,		MSG_EM_88K,
	MSG_EM_486,		MSG_EM_860,		MSG_EM_MIPS,
	MSG_EM_UNKNOWN9,	MSG_EM_MIPS_RS3_LE, 	MSG_EM_RS6000,
	MSG_EM_UNKNOWN12,	MSG_EM_UNKNOWN13,	MSG_EM_UNKNOWN14,
	MSG_EM_PA_RISC,		MSG_EM_nCUBE,		MSG_EM_VPP500,
	MSG_EM_SPARC32PLUS,	MSG_EM_UNKNOWN19,	MSG_EM_PPC
};

const char *
conv_emach_str(Half machine)
{
	static char	string[STRSIZE] = { '\0' };

	if (machine == EM_SPARCV9)
		/* special case, not contiguous with other EM_'s */
		return (MSG_ORIG(MSG_EM_SPARCV9));
	else if (machine == EM_IA_64)
		return (MSG_ORIG(MSG_EM_IA_64));
	else if (machine > (EM_PPC))
		return (conv_invalid_str(string, (Lword)machine, 0));
	else
		return (MSG_ORIG(machines[machine]));

}

static const Msg etypes[] = {
	MSG_ET_NONE,		MSG_ET_REL,		MSG_ET_EXEC,
	MSG_ET_DYN,		MSG_ET_CORE
};

const char *
conv_etype_str(Half etype)
{
	static char	string[STRSIZE] = { '\0' };

	if (etype >= ET_NUM)
		return (conv_invalid_str(string, (Lword)etype, 0));
	else
		return (MSG_ORIG(etypes[etype]));
}

static const Msg versions[] = {
	MSG_EV_NONE,		MSG_EV_CURRENT
};

const char *
conv_ever_str(uint_t version)
{
	static char	string[STRSIZE] = { '\0' };

	if (version >= EV_NUM)
		return (conv_invalid_str(string, (Lword)version, 0));
	else
		return (MSG_ORIG(versions[version]));
}


static const Msg mm_flags[] = {
	MSG_EF_SPARCV9_TSO,	MSG_EF_SPARCV9_PSO,	MSG_EF_SPARCV9_RMO
};

#define	EFLAGSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_EF_SPARCV9_TSO_SIZE + \
		MSG_EF_SPARC_SUN_US1_SIZE + \
		MSG_EF_SPARC_HAL_R1_SIZE + \
		MSG_EF_SPARC_SUN_US3_SIZE + \
		MSG_GBL_CSQBRKT_SIZE
/*
 * Valid vendor extension bits for SPARCV9. This must be
 * updated along with elf_SPARC.h.
 */
#define	EXTBITS_V9	(EF_SPARC_SUN_US1 | EF_SPARC_HAL_R1 | EF_SPARC_SUN_US3)

const char *
conv_eflags_str(Half mach, uint_t flags)
{
	static char	string[EFLAGSZ] = { '\0' };

	/*
	 * Make a string representation of the e_flags field.
	 * If any bogus bits are set, then just return a string
	 * containing the numeric value.
	 */
	if ((mach == EM_SPARC) && (flags == EF_SPARC_32PLUS)) {
		(void) strcpy(string, MSG_ORIG(MSG_EF_SPARC_32PLUS));
	} else if ((mach == EM_SPARCV9) &&
	    ((flags & ~(EF_SPARCV9_MM | EXTBITS_V9)) == 0)) {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		(void) strcat(string, MSG_ORIG(mm_flags[flags &
		    EF_SPARCV9_MM]));

		if (flags & EF_SPARC_SUN_US1)
			(void) strcat(string, MSG_ORIG(MSG_EF_SPARC_SUN_US1));
		if (flags & EF_SPARC_HAL_R1)
			(void) strcat(string, MSG_ORIG(MSG_EF_SPARC_HAL_R1));
		if (flags & EF_SPARC_SUN_US3)
			(void) strcat(string, MSG_ORIG(MSG_EF_SPARC_SUN_US3));

		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));
	} else
		(void) sprintf(string, MSG_ORIG(MSG_ELF_GEN_FLAGS), flags);

	return (string);
}
