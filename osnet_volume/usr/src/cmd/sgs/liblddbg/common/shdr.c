/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)shdr.c	1.11	99/06/01 SMI"

#include	"msg.h"
#include	"_debug.h"


#if !(defined(_ELF64) && defined(lint))


/*
 * Print out a single `section header' entry.
 */
void
Elf_shdr_entry(Half mach, Elf32_Shdr * shdr)
{
	dbg_print(MSG_ORIG(MSG_SHD_ADDR), EC_ADDR(shdr->sh_addr),
	    conv_secflg_str(mach, shdr->sh_flags));
	dbg_print(MSG_ORIG(MSG_SHD_SIZE), EC_XWORD(shdr->sh_size),
	    conv_sectyp_str(mach, shdr->sh_type));
	dbg_print(MSG_ORIG(MSG_SHD_OFFSET), EC_OFF(shdr->sh_offset),
	    EC_XWORD(shdr->sh_entsize));
	dbg_print(MSG_ORIG(MSG_SHD_LINK), EC_WORD(shdr->sh_link),
	    EC_WORD(shdr->sh_info));
	dbg_print(MSG_ORIG(MSG_SHD_ALIGN), EC_XWORD(shdr->sh_addralign));
}

void
Gelf_shdr_entry(Half mach, GElf_Shdr * shdr)
{
	dbg_print(MSG_ORIG(MSG_SHD_ADDR), EC_ADDR(shdr->sh_addr),
	    conv_secflg_str(mach, (Word)shdr->sh_flags));
	dbg_print(MSG_ORIG(MSG_SHD_SIZE), EC_XWORD(shdr->sh_size),
	    conv_sectyp_str(mach, shdr->sh_type));
	dbg_print(MSG_ORIG(MSG_SHD_OFFSET), EC_OFF(shdr->sh_offset),
	    EC_XWORD(shdr->sh_entsize));
	dbg_print(MSG_ORIG(MSG_SHD_LINK), EC_WORD(shdr->sh_link),
	    EC_WORD(shdr->sh_info));
	dbg_print(MSG_ORIG(MSG_SHD_ALIGN), EC_XWORD(shdr->sh_addralign));
}

#endif /* !(defined(_ELF64) && defined(lint)) */
