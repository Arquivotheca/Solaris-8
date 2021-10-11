/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights resreved.
 */
#pragma ident	"@(#)data.c	1.8	98/08/31 SMI"

/*
 * String conversion routine for Elf data buffer types.
 */
#include	<stdio.h>
#include	"_conv.h"
#include	"data_msg.h"

static const Msg types[] = {
	MSG_DATA_BYTE,		MSG_DATA_ADDR,		MSG_DATA_DYN,
	MSG_DATA_EHDR,		MSG_DATA_HALF,		MSG_DATA_OFF,
	MSG_DATA_PHDR,		MSG_DATA_RELA,		MSG_DATA_REL,
	MSG_DATA_SHDR,		MSG_DATA_SWORD,		MSG_DATA_SYM,
	MSG_DATA_WORD,		MSG_DATA_VDEF,		MSG_DATA_VNEED,
	MSG_DATA_SXWORD,	MSG_DATA_XWORD
};

const char *
conv_d_type_str(Elf_Type type)
{
	static char	string[STRSIZE] = { '\0' };

	if (type >= ELF_T_NUM)
		return (conv_invalid_str(string, (Lword)type, 0));
	else
		return (MSG_ORIG(types[type]));
}
