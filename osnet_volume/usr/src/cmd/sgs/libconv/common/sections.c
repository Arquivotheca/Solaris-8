/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sections.c	1.16	99/07/19 SMI"

/*
 * String conversion routines for section attributes.
 */
#include	<string.h>
#include	<sys/param.h>
#include	<sys/elf_ia64.h>
#include	"_conv.h"
#include	"sections_msg.h"

static const Msg secs[] = {
	MSG_SHT_NULL,		MSG_SHT_PROGBITS,	MSG_SHT_SYMTAB,
	MSG_SHT_STRTAB,		MSG_SHT_RELA,		MSG_SHT_HASH,
	MSG_SHT_DYNAMIC,	MSG_SHT_NOTE,		MSG_SHT_NOBITS,
	MSG_SHT_REL,		MSG_SHT_SHLIB,		MSG_SHT_DYNSYM
};

const char *
conv_sectyp_str(Half mach, unsigned int sec)
{
	static char	string[STRSIZE] = { '\0' };

	if (sec >= SHT_NUM) {
		if (mach == EM_IA_64) {
			if (sec == (unsigned int)SHT_IA_64_EXT)
				return (MSG_ORIG(MSG_SHT_IA64_EXT));
			if (sec == (unsigned int)SHT_IA_64_UNWIND)
				return (MSG_ORIG(MSG_SHT_IA64_UNWIND));
		}
		if (sec == (unsigned int)SHT_SUNW_verdef)
			return (MSG_ORIG(MSG_SHT_SUNW_verdef));
		else if (sec == (unsigned int)SHT_SUNW_verneed)
			return (MSG_ORIG(MSG_SHT_SUNW_verneed));
		else if (sec == (unsigned int)SHT_SUNW_versym)
			return (MSG_ORIG(MSG_SHT_SUNW_versym));
		else if (sec == (unsigned int)SHT_SUNW_syminfo)
			return (MSG_ORIG(MSG_SHT_SUNW_syminfo));
		else if (sec == (unsigned int)SHT_SUNW_COMDAT)
			return (MSG_ORIG(MSG_SHT_SUNW_COMDAT));
		else
			return (conv_invalid_str(string, (Lword)sec, 0));
	} else
		return (MSG_ORIG(secs[sec]));
}

#define	FLAGSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_SHF_WRITE_SIZE + \
		MSG_SHF_ALLOC_SIZE + \
		MSG_SHF_EXECINSTR_SIZE + \
		MSG_SHF_EXCLUDE_SIZE + \
		MSG_SHF_ORDERED_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

const char *
conv_secflg_str(Half mach, unsigned int flags)
{
	static	char	string[FLAGSZ] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		unsigned int	flags_handled = 0;
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & SHF_WRITE) {
			(void) strcat(string, MSG_ORIG(MSG_SHF_WRITE));
			flags_handled |= SHF_WRITE;
		}
		if (flags & SHF_ALLOC) {
			(void) strcat(string, MSG_ORIG(MSG_SHF_ALLOC));
			flags_handled |= SHF_ALLOC;
		}
		if (flags & SHF_EXECINSTR) {
			(void) strcat(string, MSG_ORIG(MSG_SHF_EXECINSTR));
			flags_handled |= SHF_EXECINSTR;
		}
		if (flags & SHF_EXCLUDE) {
			(void) strcat(string, MSG_ORIG(MSG_SHF_EXCLUDE));
			flags_handled |= SHF_EXCLUDE;
		}
		if (flags & SHF_ORDERED) {
			(void) strcat(string, MSG_ORIG(MSG_SHF_ORDERED));
			flags_handled |= SHF_ORDERED;
		}
		if (mach == EM_IA_64) {
			if (flags & SHF_IA_64_SHORT) {
				(void) strcat(string,
					MSG_ORIG(MSG_SHF_IA64_SHORT));
				flags_handled |= SHF_IA_64_SHORT;
			}
			if (flags & SHF_IA_64_NORECOV) {
				(void) strcat(string,
					MSG_ORIG(MSG_SHF_IA64_NORECOV));
				flags_handled |= SHF_IA_64_NORECOV;
			}
		}
		/*
		 * Are there any flags that havn't been handled.
		 */
		if ((flags & flags_handled) != flags) {
			char	*str;
			uint_t	len;

			len = strlen(string);
			str = string + len;
			(void) conv_invalid_str(str,
				(Lword)(flags & (~flags_handled)), 0);
		}
		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}
