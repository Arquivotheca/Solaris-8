/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	CONV_DOT_H
#define	CONV_DOT_H

#pragma ident	"@(#)conv.h	1.29	99/10/12 SMI"

/*
 * Global include file for conversion library.
 */

#include <stdlib.h>
#include <libelf.h>
#include <dlfcn.h>
#include "libld.h"
#include "sgs.h"
#include "machdep.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Variables
 */
extern	const char	*conv_arch64_name; /* 64-bit ISA name */


/*
 * Functions
 */

extern	void		conv_check_native(char **, char **, char *);
extern	const char	*conv_d_type_str(Elf_Type);
extern	const char	*conv_deftag_str(Symref);
extern	const char	*conv_dlflag_str(int);
extern	const char	*conv_dlmode_str(int);
extern	const char	*conv_dyntag_str(int, Half);
extern	const char	*conv_dynflag_1_str(uint_t);
extern	const char	*conv_dynposflag_1_str(uint_t);
extern	const char	*conv_dynfeature_1_str(uint_t);
extern	const char	*conv_config_str(int);
extern	const char	*conv_config_obj(Half);
extern	const char	*conv_eclass_str(Byte);
extern	const char	*conv_edata_str(Byte);
extern	const char	*conv_emach_str(Half);
extern	const char	*conv_ever_str(uint_t);
extern	const char	*conv_etype_str(Half);
extern	const char	*conv_eflags_str(Half, uint_t);
extern	const char	*conv_info_bind_str(unsigned char);
extern	const char	*conv_info_type_str(Half, unsigned char);
extern	Isa_desc	*conv_isalist(void);
extern	const char	*conv_phdrflg_str(unsigned int);
extern	const char	*conv_phdrtyp_str(Half, unsigned int);
extern	const char	*conv_reloc_type_str(Half, uint_t);
extern	const char	*conv_reloc_ia64_type_str(uint_t);
extern	const char	*conv_reloc_SPARC_type_str(uint_t);
extern	const char	*conv_reloc_386_type_str(uint_t);
extern	const char	*conv_sym_value_str(Half, uint_t, Lword);
extern	const char	*conv_sym_SPARC_value_str(Lword);
extern	const char	*conv_secflg_str(Half, unsigned int);
extern	const char	*conv_sectyp_str(Half, unsigned int);
extern	const char	*conv_segaflg_str(unsigned int);
extern	const char	*conv_shndx_str(Half);
extern	int		conv_sys_eclass();
extern	Uts_desc	*conv_uts(void);
extern	const char	*conv_verflg_str(Half);

#ifdef	__cplusplus
}
#endif

#endif /* CONV_DOT_H */
