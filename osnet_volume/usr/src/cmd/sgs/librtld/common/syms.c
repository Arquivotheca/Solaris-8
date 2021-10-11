/*
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Update the symbol table entries:
 *
 *  o	If addr is non-zero then every symbol entry is updated to indicate the
 *	new location to which the object will be mapped.
 *
 *  o	The address of the `_edata' and `_end' symbols, and their associated
 *	section, is updated to reflect any new heap addition.
 */
#pragma ident	"@(#)syms.c	1.9	97/06/05 SMI"

#include	<libelf.h>
#include	<string.h>
#include	"sgs.h"
#include	"machdep.h"
#include	"msg.h"
#include	"_rtld.h"

void
update_sym(Cache * cache, Cache * _cache, Addr edata, Half endx, Addr addr)
{
	char *	strs;
	Sym *	syms;
	Shdr *	shdr;
	Xword	symn, cnt;

	/*
	 * Set up to read the symbol table and its associated string table.
	 */
	shdr = _cache->c_shdr;
	syms = (Sym *)_cache->c_data->d_buf;
	symn = shdr->sh_size / shdr->sh_entsize;

	strs = (char *)cache[shdr->sh_link].c_data->d_buf;

	/*
	 * Loop through the symbol table looking for `_end' and `_edata'.
	 */
	for (cnt = 0; cnt < symn; cnt++, syms++) {
		char *	name = strs + syms->st_name;

		if (addr) {
			if (syms->st_value)
				syms->st_value += addr;
		}

		if ((name[0] != '_') || (name[1] != 'e'))
			continue;
		if (strcmp(name, MSG_ORIG(MSG_SYM_END)) &&
		    strcmp(name, MSG_ORIG(MSG_SYM_EDATA)))
			continue;

		syms->st_value = edata + addr;
		if (endx)
			syms->st_shndx = endx;
	}
}
