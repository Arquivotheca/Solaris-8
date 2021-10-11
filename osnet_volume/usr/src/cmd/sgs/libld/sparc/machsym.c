/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machsym.c	1.13	99/06/23 SMI"

#include	<stdio.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sys/procfs_isa.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"


/*
 * Register symbols that don't go in the output image,
 * e.g. local register symbols.
 */
static List	extregsyms;


/*
 *
 *  Matrix of legal combinations of usage of a given register:
 *
 *	Obj1\Obj2       Scratch Named
 *	Scratch          OK      NO
 *	Named            NO      *
 *
 *  * OK if the symbols are identical, NO if they are not.  Two symbols
 *  are identical if and only if one of the following is true:
 *        A. They are both global and have the same name.
 *        B. They are both local, have the same name, and are defined in
 *        the same object.  (Note that a local symbol in one object is
 *        never identical to a local symbol in another object, even if the
 *        name is the same.)
 *
 *  Matrix of legal combinations of st_shndx for the same register symbol:
 *
 *	Obj1\Obj2       UNDEF   ABS
 *	UNDEF            OK      OK
 *	ABS              OK      NO
 *
 */
static uintptr_t
check_regsyms(Sym * sym1, const char * name1, Sym * sym2, const char * name2)
{
	if ((sym1->st_name == 0) && (sym2->st_name == 0))
		return (0);	/* scratches are always compatible */

	if ((ELF_ST_BIND(sym1->st_info) == STB_LOCAL) ||
	    (ELF_ST_BIND(sym2->st_info) == STB_LOCAL)) {
		if (sym1->st_value == sym2->st_value)
			/* local symbol incompat */
			return ((uintptr_t)MSG_INTL(MSG_SYM_INCOMPREG));
		return (0);		/* no other prob from locals */
	}

	if (sym1->st_value == sym2->st_value) {
		/* NOTE this just avoids the below strcmp */
		if ((sym1->st_name == 0) || (sym2->st_name == 0))
			/* can't match scratch to named */
			return ((uintptr_t)MSG_INTL(MSG_SYM_INCOMPREG));

		if (strcmp(name1, name2) != 0)
			/* diff name, same register value */
			return ((uintptr_t)MSG_INTL(MSG_SYM_INCOMPREG));

		if ((sym1->st_shndx == SHN_ABS) && (sym2->st_shndx == SHN_ABS))
			/* multiply defined */
			return ((uintptr_t)MSG_INTL(MSG_SYM_MULTINIREG));

	} else if (strcmp(name1, name2) == 0)
		/* same name, diff register value */
		return ((uintptr_t)MSG_INTL(MSG_SYM_INCOMPREG));

	return (0);
}

uintptr_t
mach_sym_typecheck(Sym_desc * sdp, Sym * nsym, Ifl_desc * ifl, Ofl_desc * ofl)
{
	Byte otype = ELF_ST_TYPE(sdp->sd_sym->st_info);
	Byte ntype = ELF_ST_TYPE(nsym->st_info);

	if (otype != ntype) {
		if ((otype == STT_SPARC_REGISTER) ||
		    (ntype == STT_SPARC_REGISTER)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYM_DIFFTYPE),
				sdp->sd_name);
			eprintf(ERR_NONE, MSG_INTL(MSG_SYM_FILETYPES),
				sdp->sd_file->ifl_name,
				conv_info_type_str(ofl->ofl_e_machine,
				otype), ifl->ifl_name,
				conv_info_type_str(ofl->ofl_e_machine, ntype));
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (S_ERROR);
		}
	} else if (otype == STT_SPARC_REGISTER) {
		uintptr_t msg;

		if ((msg = check_regsyms(sdp->sd_sym, sdp->sd_name, nsym,
		    sdp->sd_name)) != NULL) {
			const char * name = (*sdp->sd_name) ? sdp->sd_name :
				MSG_INTL(MSG_STR_UNKNOWN);
			eprintf(ERR_FATAL, (const char *)msg, name,
			    conv_sym_value_str(ofl->ofl_e_machine,
				STT_SPARC_REGISTER,
				(Lword)sdp->sd_sym->st_value),
			    sdp->sd_file->ifl_name, name,
			    conv_sym_value_str(ofl->ofl_e_machine,
				STT_SPARC_REGISTER, (Lword)nsym->st_value),
			    ifl->ifl_name);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (S_ERROR);
		}
	}

	return (1);
}

uintptr_t
add_regsym(Sym_desc * sdp, Ofl_desc * ofl)
{
	Listnode *	lnp;
	Sym_desc *	_sdp;
	uintptr_t	msg;


	/*
	 * Only do something if this is a REGISTER symbol
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) != STT_SPARC_REGISTER)
		return (1);

	/*
	 * Check for bogus scratch register definitions.
	 */
	if ((sdp->sd_sym->st_name == 0) &&
	    ((ELF_ST_BIND(sdp->sd_sym->st_info) != STB_GLOBAL) ||
	    (sdp->sd_sym->st_shndx != SHN_UNDEF))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYM_BADSCRATCH),
			conv_sym_value_str(ofl->ofl_e_machine,
			    STT_SPARC_REGISTER, (Lword)sdp->sd_sym->st_value),
			sdp->sd_file->ifl_name);
		return (S_ERROR);
	}

	/*
	 * Check for bogus register number.
	 */
	if ((sdp->sd_sym->st_value < R_G1) || (sdp->sd_sym->st_value > R_G7)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_SYM_BADREG),
			conv_sym_value_str(ofl->ofl_e_machine,
			    STT_SPARC_REGISTER, (Lword)sdp->sd_sym->st_value),
			sdp->sd_file->ifl_name);
		return (S_ERROR);
	}

	/*
	 * We verify at this point that we do not use the
	 * same register incompatibly.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_regsyms, lnp, _sdp)) {
		/*
		 * We only need one record of a scratch register in
		 * the output image.  Named registers get handled by
		 * the symbol resolution mechanism.
		 */
		if ((sdp->sd_sym->st_name == NULL) &&
		    (_sdp->sd_sym->st_name == NULL) &&
		    (sdp->sd_sym->st_value == _sdp->sd_sym->st_value)) {
			sdp->sd_ref = REF_DYN_SEEN;
			break;
		}

		if ((msg = check_regsyms(_sdp->sd_sym, _sdp->sd_name,
		    sdp->sd_sym, sdp->sd_name)) != NULL) {
			const char * name = (*_sdp->sd_name) ? _sdp->sd_name :
				MSG_INTL(MSG_STR_UNKNOWN);
			eprintf(ERR_FATAL, (const char *)msg, name,
				conv_sym_value_str(ofl->ofl_e_machine,
				    STT_SPARC_REGISTER,
				    (Lword)_sdp->sd_sym->st_value),
				_sdp->sd_file->ifl_name,
				sdp->sd_name,
				conv_sym_value_str(ofl->ofl_e_machine,
				    STT_SPARC_REGISTER,
				    (Lword)sdp->sd_sym->st_value),
				sdp->sd_file->ifl_name);
			return (S_ERROR);
		}
	}

	/*
	 * There is a separate list for regsyms that exist
	 * in the dependencies, but aren't part of our new
	 * output image.  These can conflict as well.
	 */
	for (LIST_TRAVERSE(&extregsyms, lnp, _sdp)) {
		if ((msg = check_regsyms(_sdp->sd_sym, _sdp->sd_name,
		    sdp->sd_sym, sdp->sd_name)) != NULL) {
			const char * name = (*_sdp->sd_name) ? _sdp->sd_name :
				MSG_INTL(MSG_STR_UNKNOWN);
			eprintf(ERR_FATAL, (const char *)msg, name,
				conv_sym_value_str(ofl->ofl_e_machine,
				    STT_SPARC_REGISTER,
				    (Lword)_sdp->sd_sym->st_value),
				_sdp->sd_file->ifl_name,
				sdp->sd_name,
				conv_sym_value_str(ofl->ofl_e_machine,
				    STT_SPARC_REGISTER,
				    (Lword)sdp->sd_sym->st_value),
				sdp->sd_file->ifl_name);
			return (S_ERROR);
		}
	}

	if ((sdp->sd_file->ifl_ehdr->e_type == ET_DYN) &&
	    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL)) {
		if (list_appendc(&extregsyms, sdp) == 0)
			return (S_ERROR);
	} else {
		if (list_appendc(&ofl->ofl_regsyms, sdp) == 0)
			return (S_ERROR);
		sdp->sd_flags |= FLG_SY_REGSYM;
		if (sdp->sd_ref > REF_DYN_SEEN)
			ofl->ofl_regsymcnt++;
	}
	return (1);
}


/*
 * Machine-specific processing of the local symbols in the
 * dynamic symbol table.
 */
uintptr_t
mach_dyn_locals(Sym * syms, const char * strs, size_t locals,
	Ifl_desc * ifl, Ofl_desc * ofl)
{
	int ndx;

	for (ndx = 1; ndx < locals; ++ndx) {
		Byte st = ELF_ST_TYPE(syms[ndx].st_info);
		Sym_desc * sd;

		if (st == STT_SPARC_REGISTER) {
			if ((sd = libld_calloc(sizeof (Sym_desc), 1)) == 0)
				return (S_ERROR);
			sd->sd_name = strs + syms[ndx].st_name;
			sd->sd_file = ifl;
			sd->sd_sym = &syms[ndx];

			if (add_regsym(sd, ofl) == S_ERROR)
				return (S_ERROR);
		}
	}

	return (0);
}
