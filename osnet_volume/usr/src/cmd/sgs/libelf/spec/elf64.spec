#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)elf64.spec	1.2	99/01/28 SMI"
#
# cmd/sgs/libelf/spec/elf64.spec

function	elf64_checksum
include		<libelf.h>
declaration	long elf64_checksum(Elf *elf)
version		SUNW_1.3
exception	$return == 0
end		

function	elf64_fsize
include		<libelf.h>
declaration	size_t elf64_fsize(Elf_Type type, size_t count, unsigned ver)
version		SUNW_1.2
exception	$return == 0
end		

function	elf64_getehdr
include		<libelf.h>
declaration	Elf64_Ehdr *elf64_getehdr(Elf *elf)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_getphdr
include		<libelf.h>
declaration	Elf64_Phdr *elf64_getphdr(Elf *elf)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_getshdr
include		<libelf.h>
declaration	Elf64_Shdr *elf64_getshdr(Elf_Scn *scn)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_newehdr
include		<libelf.h>
declaration	Elf64_Ehdr *elf64_newehdr(Elf *elf)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_newphdr
include		<libelf.h>
declaration	Elf64_Phdr *elf64_newphdr(Elf *elf, size_t count)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_xlatetof
include		<libelf.h>
declaration	Elf_Data *elf64_xlatetof(Elf_Data *dst, const Elf_Data *src,\
			unsigned encode)
version		SUNW_1.2
exception	$return == NULL
end		

function	elf64_xlatetom
include		<libelf.h>
declaration	Elf_Data *elf64_xlatetom(Elf_Data *dst, const Elf_Data *src, \
			unsigned encode)
version		SUNW_1.2
exception	$return == NULL
end		

