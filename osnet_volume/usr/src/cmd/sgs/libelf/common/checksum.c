/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)checksum.c	1.1	99/01/20 SMI"

#include "syn.h"
#include <errno.h>
#include <libelf.h>
#include <sum.h>
#include "decl.h"
#include "msg.h"

/*
 * Routines for generating a checksum for an elf image. Typically used to create
 * a DT_CHECKSUM entry.  This checksum is intended to remain constant after
 * operations such as strip(1)/mcs(1), thus only allocatable sections are
 * processed, and of those, any that might be modified by these external
 * commands are skipped.
 */
#define	MSW(l)	(((l) >> 16) & 0x0000ffffL)
#define	LSW(l)	((l) & 0x0000ffffL)


/*
 * update and epilogue sum functions (stolen from libcmd)
 */
static long
sumupd(long sum, char * cp, unsigned long cnt)
{
	if ((cp == 0) || (cnt == 0))
		return (sum);

	while (cnt--)
		sum += *cp++ & 0x00ff;

	return (sum);
}

static long
sumepi(long sum)
{
	long	_sum;

	_sum = LSW(sum) + MSW(sum);
	return ((ushort) (LSW(_sum) + MSW(_sum)));
}

long
elf32_checksum(Elf * elf)
{
	long		sum = 0;
	Elf32_Ehdr *	ehdr;
	Elf32_Shdr *	shdr;
	Elf_Scn *	scn;
	Elf_Data *	data, * (* getdata)(Elf_Scn *, Elf_Data *);
	size_t		shnum;

	if ((ehdr = elf32_getehdr(elf)) == 0)
		return (0);

	/*
	 * Determine the data information to retrieve.  When called from ld()
	 * we're processing an ELF_C_IMAGE (memory) image and thus need to use
	 * elf_getdata(), as there is not yet a file image (or raw data) backing
	 * this.  When called from utilities like elfdump(1) we're processing a
	 * file image and thus using the elf_rawdata() allows the same byte
	 * stream to be processed from different architectures - presently this
	 * is irrelevant, as the checksum simply sums the data bytes, their
	 * order doesn't matter.  But being uncooked is slightly less overhead.
	 */
	if (elf->ed_myflags & EDF_MEMORY)
		getdata = elf_getdata;
	else
		getdata = elf_rawdata;

	for (shnum = 1; shnum < ehdr->e_shnum; shnum++) {
		if ((scn = elf_getscn(elf, shnum)) == 0)
			return (0);
		if ((shdr = elf32_getshdr(scn)) == 0)
			return (0);

		if (!(shdr->sh_flags & SHF_ALLOC))
			continue;

		if ((shdr->sh_type == SHT_DYNSYM) ||
		    (shdr->sh_type == SHT_DYNAMIC))
			continue;

		data = 0;
		while ((data = (*getdata)(scn, data)) != 0)
			sum = sumupd(sum, data->d_buf, data->d_size);

	}
	return (sumepi(sum));
}

long
elf64_checksum(Elf * elf)
{
	long		sum = 0;
	Elf64_Ehdr *	ehdr;
	Elf64_Shdr *	shdr;
	Elf_Scn *	scn;
	Elf_Data *	data;
	size_t		shnum;

	if ((ehdr = elf64_getehdr(elf)) == 0)
		return (0);

	for (shnum = 1; shnum < ehdr->e_shnum; shnum++) {
		if ((scn = elf_getscn(elf, shnum)) == 0)
			return (0);
		if ((shdr = elf64_getshdr(scn)) == 0)
			return (0);

		if (!(shdr->sh_flags & SHF_ALLOC))
			continue;

		if ((shdr->sh_type == SHT_DYNSYM) ||
		    (shdr->sh_type == SHT_DYNAMIC))
			continue;

		data = 0;
		while ((data = elf_getdata(scn, data)) != 0)
			sum = sumupd(sum, data->d_buf, data->d_size);

	}
	return (sumepi(sum));
}
