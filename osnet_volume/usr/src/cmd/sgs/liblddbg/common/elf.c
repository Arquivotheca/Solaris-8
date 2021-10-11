/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.14	98/08/28 SMI"

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"


void
Gelf_elf_data(const char * str, GElf_Addr addr, Elf_Data * data,
	const char * file)
{
	dbg_print(MSG_INTL(MSG_ELF_ENTRY), str, EC_ADDR(addr),
		conv_d_type_str(data->d_type), EC_XWORD(data->d_size),
		EC_OFF(data->d_off), EC_XWORD(data->d_align), file);
}

void
Elf_elf_data(const char * str, Addr addr, Elf_Data * data,
	const char * file)
{
	dbg_print(MSG_INTL(MSG_ELF_ENTRY), str, EC_ADDR(addr),
		conv_d_type_str(data->d_type), EC_XWORD(data->d_size),
		EC_OFF(data->d_off), EC_XWORD(data->d_align), file);
}

void
Gelf_elf_data_title()
{
	dbg_print(MSG_INTL(MSG_ELF_TITLE));
}

void
_Dbg_elf_data_in(Os_desc * osp, Is_desc * isp)
{
	Shdr *		shdr = osp->os_shdr;
	Elf_Data *	data = isp->is_indata;
	const char *	file;

	if (isp->is_file)
		file = isp->is_file->ifl_name;
	else
		file = MSG_ORIG(MSG_STR_EMPTY);

	Elf_elf_data(MSG_INTL(MSG_STR_IN),
	    (Addr)(shdr->sh_addr + data->d_off),
	    data, file);
}

void
_Dbg_elf_data_out(Os_desc * osp)
{
	Shdr *		shdr = osp->os_shdr;
	Elf_Data *	data = osp->os_outdata;

	Elf_elf_data(MSG_INTL(MSG_STR_OUT), shdr->sh_addr,
	    data, MSG_ORIG(MSG_STR_EMPTY));
}

void
Gelf_elf_header(GElf_Ehdr * ehdr)
{
	Byte *		byte =	&(ehdr->e_ident[0]);
	const char *	flgs;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_ELF_HEADER));

	dbg_print(MSG_ORIG(MSG_ELF_MAGIC), byte[EI_MAG0],
	    (byte[EI_MAG1] ? byte[EI_MAG1] : '0'),
	    (byte[EI_MAG2] ? byte[EI_MAG2] : '0'),
	    (byte[EI_MAG3] ? byte[EI_MAG3] : '0'));
	dbg_print(MSG_ORIG(MSG_ELF_CLASS),
	    conv_eclass_str(ehdr->e_ident[EI_CLASS]),
	    conv_edata_str(ehdr->e_ident[EI_DATA]));
	dbg_print(MSG_ORIG(MSG_ELF_MACHINE),
	    conv_emach_str(ehdr->e_machine), conv_ever_str(ehdr->e_version));
	dbg_print(MSG_ORIG(MSG_ELF_TYPE), conv_etype_str(ehdr->e_type));

	/*
	 * Line up the flags differently depending on wether we
	 * received a numeric (e.g. "0x200") or text represent-
	 * ation (e.g. "[ EF_SPARC_SUN_US1 ]").
	 */
	flgs = conv_eflags_str(ehdr->e_machine, ehdr->e_flags);
	if (flgs[0] == '[')
		dbg_print(MSG_ORIG(MSG_ELF_FLAGS_FMT), flgs);
	else
		dbg_print(MSG_ORIG(MSG_ELF_FLAGS), flgs);

	dbg_print(MSG_ORIG(MSG_ELF_ESIZE), EC_ADDR(ehdr->e_entry),
	    ehdr->e_ehsize, ehdr->e_shstrndx);
	dbg_print(MSG_ORIG(MSG_ELF_SHOFF), EC_OFF(ehdr->e_shoff),
	    ehdr->e_shentsize, ehdr->e_shnum);
	dbg_print(MSG_ORIG(MSG_ELF_PHOFF), EC_OFF(ehdr->e_phoff),
	    ehdr->e_phentsize, ehdr->e_phnum);
}
