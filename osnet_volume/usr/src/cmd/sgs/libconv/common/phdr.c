/*
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)phdr.c	1.7	99/06/01 SMI"

/*
 * String conversion routines for program header attributes.
 */
#include	<string.h>
#include	<sys/elf_ia64.h>
#include	"_conv.h"
#include	"phdr_msg.h"

static const Msg phdrs[] = {
	MSG_PT_NULL,		MSG_PT_LOAD,		MSG_PT_DYNAMIC,
	MSG_PT_INTERP,		MSG_PT_NOTE,		MSG_PT_SHLIB,
	MSG_PT_PHDR,
};

const char *
conv_phdrtyp_str(Half mach, unsigned phdr)
{
	static char	string[STRSIZE] = { '\0' };

	if (phdr >= PT_NUM) {
		if (mach == EM_IA_64) {
			if (phdr == PT_IA_64_ARCHEXT)
				return (MSG_ORIG(MSG_PT_IA64_ARCHEXT));
			if (phdr == PT_IA_64_UNWIND)
				return (MSG_ORIG(MSG_PT_IA64_UNWIND));
		}
		return (conv_invalid_str(string, (Lword)phdr, 0));
	} else
		return (MSG_ORIG(phdrs[phdr]));
}

#define	PHDRSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_PF_X_SIZE + \
		MSG_PF_W_SIZE + \
		MSG_PF_R_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

const char *
conv_phdrflg_str(unsigned int flags)
{
	static	char	string[PHDRSZ] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & PF_X)
			(void) strcat(string, MSG_ORIG(MSG_PF_X));
		if (flags & PF_W)
			(void) strcat(string, MSG_ORIG(MSG_PF_W));
		if (flags & PF_R)
			(void) strcat(string, MSG_ORIG(MSG_PF_R));
		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}
