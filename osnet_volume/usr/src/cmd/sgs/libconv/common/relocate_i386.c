/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All Rights Reserved.
 */
#pragma ident	"@(#)relocate_i386.c	1.10	98/08/31 SMI"

/*
 * String conversion routine for relocation types.
 */
#include	<stdio.h>
#include	<sys/elf_386.h>
#include	"_conv.h"
#include	"relocate_i386_msg.h"

/*
 * Intel386 specific relocations.
 */
static const Msg rels[] = {
	MSG_R_386_NONE,		MSG_R_386_32,		MSG_R_386_PC32,
	MSG_R_386_GOT32,	MSG_R_386_PLT32,	MSG_R_386_COPY,
	MSG_R_386_GLOB_DAT,	MSG_R_386_JMP_SLOT,	MSG_R_386_RELATIVE,
	MSG_R_386_GOTOFF,	MSG_R_386_GOTPC,	MSG_R_386_32PLT
};

const char *
conv_reloc_386_type_str(uint_t rel)
{
	static char	string[STRSIZE] = { '\0' };

	if (rel >= R_386_NUM)
		return (conv_invalid_str(string, (Lword)rel, 0));
	else
		return (MSG_ORIG(rels[rel]));
}
