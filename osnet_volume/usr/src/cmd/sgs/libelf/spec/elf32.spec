#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)elf32.spec	1.2	99/01/28 SMI"
#
# cmd/sgs/libelf/spec/elf32.spec

function	elf32_checksum
include		<libelf.h>
declaration	long elf32_checksum(Elf *elf)
version		SUNW_1.3
exception	$return == 0
end		

function	elf32_fsize
include		<libelf.h>
declaration	size_t elf32_fsize(Elf_Type type, size_t count, unsigned ver)
version		SUNW_0.7
exception	$return == 0
end		

function	elf32_getphdr
include		<libelf.h>
declaration	Elf32_Phdr *elf32_getphdr(Elf *elf)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_newphdr
include		<libelf.h>
declaration	Elf32_Phdr *elf32_newphdr(Elf *elf, size_t count)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_getshdr
include		<libelf.h>
declaration	Elf32_Shdr *elf32_getshdr(Elf_Scn *scn)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_getehdr
include		<libelf.h>
declaration	Elf32_Ehdr *elf32_getehdr(Elf *elf)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_newehdr
include		<libelf.h>
declaration	Elf32_Ehdr *elf32_newehdr(Elf *elf)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_xlatetof
include		<libelf.h>
declaration	Elf_Data *elf32_xlatetof(Elf_Data *dst, const Elf_Data *src,\
			unsigned encode)
version		SUNW_0.7
exception	$return == NULL
end		

function	elf32_xlatetom
include		<libelf.h>
declaration	Elf_Data *elf32_xlatetom(Elf_Data *dst, const Elf_Data *src, \
			unsigned encode)
version		SUNW_0.7
exception	$return == NULL
end		

