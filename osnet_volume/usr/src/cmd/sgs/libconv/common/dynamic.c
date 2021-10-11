/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dynamic.c	1.33	99/09/21 SMI"

/*
 * String conversion routine for .dynamic tag entries.
 */
#include	<stdio.h>
#include	<string.h>
#include	<sys/elf_SPARC.h>
#include	<sys/elf_ia64.h>
#include	"_conv.h"
#include	"dynamic_msg.h"

#define	POSSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_DFP_LAZYLOAD_SIZE + \
		MSG_DFP_GROUPPERM_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

const char *
conv_dynposflag_1_str(uint_t flags)
{
	static char	string[POSSZ] = { '\0' };
	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));

	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

	if (flags & DF_P1_LAZYLOAD)
		(void) strcat(string, MSG_ORIG(MSG_DFP_LAZYLOAD));
	if (flags & DF_P1_GROUPPERM)
		(void) strcat(string, MSG_ORIG(MSG_DFP_GROUPPERM));

	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

	return ((const char *)string);
}

#define	FLAGSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_DF_NOW_SIZE + \
		MSG_DF_GROUP_SIZE + \
		MSG_DF_NODELETE_SIZE + \
		MSG_DF_LOADFLTR_SIZE + \
		MSG_DF_INITFIRST_SIZE + \
		MSG_DF_NOOPEN_SIZE + \
		MSG_DF_ORIGIN_SIZE + \
		MSG_DF_DIRECT_SIZE + \
		MSG_DF_TRANS_SIZE + \
		MSG_DF_INTERPOSE_SIZE + \
		MSG_DF_NODEFLIB_SIZE + \
		MSG_DF_NODUMP_SIZE + \
		MSG_DF_CONFALT_SIZE + \
		MSG_DF_ENDFILTEE_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

const char *
conv_dynflag_1_str(uint_t flags)
{
	static char	string[FLAGSZ] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

		if (flags & DF_1_NOW)
			(void) strcat(string, MSG_ORIG(MSG_DF_NOW));
		if (flags & DF_1_GROUP)
			(void) strcat(string, MSG_ORIG(MSG_DF_GROUP));
		if (flags & DF_1_NODELETE)
			(void) strcat(string, MSG_ORIG(MSG_DF_NODELETE));
		if (flags & DF_1_LOADFLTR)
			(void) strcat(string, MSG_ORIG(MSG_DF_LOADFLTR));
		if (flags & DF_1_INITFIRST)
			(void) strcat(string, MSG_ORIG(MSG_DF_INITFIRST));
		if (flags & DF_1_NOOPEN)
			(void) strcat(string, MSG_ORIG(MSG_DF_NOOPEN));
		if (flags & DF_1_ORIGIN)
			(void) strcat(string, MSG_ORIG(MSG_DF_ORIGIN));
		if (flags & DF_1_DIRECT)
			(void) strcat(string, MSG_ORIG(MSG_DF_DIRECT));
		if (flags & DF_1_TRANS)
			(void) strcat(string, MSG_ORIG(MSG_DF_TRANS));
		if (flags & DF_1_INTERPOSE)
			(void) strcat(string, MSG_ORIG(MSG_DF_INTERPOSE));
		if (flags & DF_1_NODEFLIB)
			(void) strcat(string, MSG_ORIG(MSG_DF_NODEFLIB));
		if (flags & DF_1_NODUMP)
			(void) strcat(string, MSG_ORIG(MSG_DF_NODUMP));
		if (flags & DF_1_CONFALT)
			(void) strcat(string, MSG_ORIG(MSG_DF_CONFALT));
		if (flags & DF_1_ENDFILTEE)
			(void) strcat(string, MSG_ORIG(MSG_DF_ENDFILTEE));

		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}

#define	FEATSZ	MSG_GBL_OSQBRKT_SIZE + \
		MSG_DTF_PARINIT_SIZE + \
		MSG_DTF_CONFEXP_SIZE + \
		MSG_GBL_CSQBRKT_SIZE

const char *
conv_dynfeature_1_str(uint_t flags)
{
	static char	string[FEATSZ] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));

		if (flags & DTF_1_PARINIT)
			(void) strcat(string, MSG_ORIG(MSG_DTF_PARINIT));
		if (flags & DTF_1_CONFEXP)
			(void) strcat(string, MSG_ORIG(MSG_DTF_CONFEXP));

		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}

static const Msg tags[] = {
	MSG_DYN_NULL,		MSG_DYN_NEEDED,		MSG_DYN_PLTRELSZ,
	MSG_DYN_PLTGOT,		MSG_DYN_HASH,		MSG_DYN_STRTAB,
	MSG_DYN_SYMTAB,		MSG_DYN_RELA,		MSG_DYN_RELASZ,
	MSG_DYN_RELAENT,	MSG_DYN_STRSZ,		MSG_DYN_SYMENT,
	MSG_DYN_INIT,		MSG_DYN_FINI,		MSG_DYN_SONAME,
	MSG_DYN_RPATH,		MSG_DYN_SYMBOLIC,	MSG_DYN_REL,
	MSG_DYN_RELSZ,		MSG_DYN_RELENT,		MSG_DYN_PLTREL,
	MSG_DYN_DEBUG,		MSG_DYN_TEXTREL,	MSG_DYN_JMPREL
};

const char *
conv_dyntag_str(int tag, Half mach)
{
	static char	string[STRSIZE] = { '\0' };

	if (tag < DT_MAXPOSTAGS)
		return (MSG_ORIG(tags[tag]));
	else {
		if (tag == DT_USED)
			return (MSG_ORIG(MSG_DYN_USED));
		else if (tag == DT_FILTER)
			return (MSG_ORIG(MSG_DYN_FILTER));
		else if (tag == DT_AUXILIARY)
			return (MSG_ORIG(MSG_DYN_AUXILIARY));
		else if (tag == DT_CONFIG)
			return (MSG_ORIG(MSG_DYN_CONFIG));
		else if (tag == DT_VERDEF)
			return (MSG_ORIG(MSG_DYN_VERDEF));
		else if (tag == DT_VERDEFNUM)
			return (MSG_ORIG(MSG_DYN_VERDEFNUM));
		else if (tag == DT_VERNEED)
			return (MSG_ORIG(MSG_DYN_VERNEED));
		else if (tag == DT_VERNEEDNUM)
			return (MSG_ORIG(MSG_DYN_VERNEEDNUM));
		else if (tag == DT_FLAGS_1)
			return (MSG_ORIG(MSG_DYN_FLAGS_1));
		else if (tag == DT_RELACOUNT)
			return (MSG_ORIG(MSG_DYN_RELACOUNT));
		else if (tag == DT_RELCOUNT)
			return (MSG_ORIG(MSG_DYN_RELCOUNT));
		else if (tag == DT_SYMINFO)
			return (MSG_ORIG(MSG_DYN_SYMINFO));
		else if (tag == DT_SYMINSZ)
			return (MSG_ORIG(MSG_DYN_SYMINSZ));
		else if (tag == DT_SYMINENT)
			return (MSG_ORIG(MSG_DYN_SYMINENT));
		else if (tag == DT_POSFLAG_1)
			return (MSG_ORIG(MSG_DYN_POSFLAG_1));
		else if (tag == DT_FEATURE_1)
			return (MSG_ORIG(MSG_DYN_FEATURE_1));
		else if (tag == DT_MOVESZ)
			return (MSG_ORIG(MSG_DYN_MOVESZ));
		else if (tag == DT_MOVEENT)
			return (MSG_ORIG(MSG_DYN_MOVEENT));
		else if (tag == DT_MOVETAB)
			return (MSG_ORIG(MSG_DYN_MOVETAB));
		else if (tag == DT_PLTPAD)
			return (MSG_ORIG(MSG_DYN_PLTPAD));
		else if (tag == DT_PLTPADSZ)
			return (MSG_ORIG(MSG_DYN_PLTPADSZ));
		else if (tag == DT_CHECKSUM)
			return (MSG_ORIG(MSG_DYN_CHECKSUM));
		else if (tag == DT_DEPAUDIT)
			return (MSG_ORIG(MSG_DYN_DEPAUDIT));
		else if (tag == DT_AUDIT)
			return (MSG_ORIG(MSG_DYN_AUDIT));
		else if (((mach == EM_SPARC) || (mach == EM_SPARCV9) ||
		    (mach == EM_SPARC32PLUS)) && (tag == DT_SPARC_REGISTER))
			/* this is so x86 can display a sparc binary */
			return (MSG_ORIG(MSG_DYN_REGISTER));
		else if (tag == DT_DEPRECATED_SPARC_REGISTER)
			return (MSG_ORIG(MSG_DYN_REGISTER));
		else if ((mach == EM_IA_64) && (tag == DT_IA_64_PLT_RESERVE))
			return (MSG_ORIG(MSG_DYN_PLTRESERVE));
		else
			return (conv_invalid_str(string, (Lword)tag, 0));
	}
}
