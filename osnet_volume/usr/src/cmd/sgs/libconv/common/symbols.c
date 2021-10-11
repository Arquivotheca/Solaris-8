/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)symbols.c	1.9	98/09/19 SMI"

/*
 * String conversion routines for symbol attributes.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"symbols_msg.h"
#include	<sys/elf_SPARC.h>

static const Msg types[] = {
	MSG_STT_NOTYPE,		MSG_STT_OBJECT,		MSG_STT_FUNC,
	MSG_STT_SECTION,	MSG_STT_FILE
};

const char *
conv_info_type_str(Half mach, unsigned char type)
{
	static char	string[STRSIZE] = { '\0' };


	if (type < STT_NUM)
		return (MSG_ORIG(types[type]));
	else if (((mach == EM_SPARC) || (mach == EM_SPARC32PLUS) ||
	    (mach == EM_SPARCV9)) && (type == STT_SPARC_REGISTER))
		return (MSG_ORIG(MSG_STT_REGISTER));
	else
		return (conv_invalid_str(string, (Lword)type, 0));
}

static const Msg binds[] = {
	MSG_STB_LOCAL,		MSG_STB_GLOBAL,		MSG_STB_WEAK
};

const char *
conv_info_bind_str(unsigned char bind)
{
	static char	string[STRSIZE] = { '\0' };

	if (bind >= STB_NUM)
		return (conv_invalid_str(string, (Lword)bind, 0));
	else
		return (MSG_ORIG(binds[bind]));
}

const char *
conv_shndx_str(Half shndx)
{
	static	char	string[STRSIZE] = { '\0' };

	if (shndx == SHN_UNDEF)
		return (MSG_ORIG(MSG_SHN_UNDEF));
	else if (shndx == SHN_ABS)
		return (MSG_ORIG(MSG_SHN_ABS));
	else if (shndx == SHN_COMMON)
		return (MSG_ORIG(MSG_SHN_COMMON));
	else
		return (conv_invalid_str(string, (Lword)shndx, 1));
}

const char *
conv_sym_value_str(Half mach, uint_t type, Lword value)
{
	static char	string[STRSIZE64] = { '\0' };
	const char *	fmt;

	if (((mach == EM_SPARC) || (mach == EM_SPARC32PLUS) ||
	    (mach == EM_SPARCV9)) && (type == STT_SPARC_REGISTER))
		return (conv_sym_SPARC_value_str(value));

	/*
	 * Should obtain the elf class rather than relying on e_machine here...
	 */
	if (mach == EM_SPARCV9)
		fmt = MSG_ORIG(MSG_FMT_VAL_64);
	else
		fmt = MSG_ORIG(MSG_FMT_VAL);

	(void) sprintf(string, fmt, EC_XWORD(value));
	return (string);
}
