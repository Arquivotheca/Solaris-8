/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)relocate.c	1.88	99/10/04 SMI"

/*
 * set-up for relocations
 */
#include	<string.h>
#include	<stdio.h>
#include	"debug.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_libld.h"


uintptr_t
reloc_GOT_relative(Boolean local, Rel_desc *rsp, Rel *reloc, Ofl_desc *ofl)
{
	Sym_desc	*sdp;
	Word		flags = ofl->ofl_flags;
	Gotndx		*gnp;

	sdp = rsp->rel_sym;

	/*
	 * If this is the first time we've seen this symbol in a GOT
	 * relocation we need to assign it a GOT token.  Once we've got
	 * all of the GOT's assigned we can assign the actual indexes.
	 */
	if ((gnp = find_gotndx(&sdp->sd_GOTndxs, reloc)) == 0) {
		Word	rtype = rsp->rel_rtype;

		if (assign_got_ndx(&sdp->sd_GOTndxs, rsp, reloc, sdp, 0,
		    ofl) == S_ERROR)
			return (S_ERROR);

		/*
		 * Now we initialize the GOT table entry.
		 *
		 * Pseudo code to describe the the decisions below:
		 *
		 * If (local)
		 * then
		 *	enter symbol value in GOT table entry
		 *	if (Shared Object)
		 *	then
		 *		create Relative relocation against symbol
		 *	fi
		 * else
		 *	clear GOT table entry
		 *	create a GLOB_DAT relocation against symbol
		 * fi
		 */
		if (local == TRUE) {

			/* rsp->rel_sym->sd_GOTndx = sdp->sd_GOTndx; */

			if (IS_FPTR(rsp->rel_rtype)) {
				if (reloc_local_fptr(rsp, reloc, ofl) ==
				    S_ERROR)
					return (S_ERROR);
			}

			if (flags & FLG_OF_SHAROBJ) {
				if (add_actrel(FLG_REL_GOT | FLG_REL_GOTCL,
				    rsp, reloc, ofl) == S_ERROR)
					return (S_ERROR);

				rsp->rel_rtype = M_R_RELATIVE;
				if (add_outrel(FLG_REL_GOT | FLG_REL_ADVAL,
				    rsp, reloc, ofl) == S_ERROR)
					return (S_ERROR);
				rsp->rel_rtype = rtype;
			} else {
				if (add_actrel(FLG_REL_GOT, rsp,
				    reloc, ofl) == S_ERROR)
					return (S_ERROR);
			}
		} else {
			if (IS_FPTR(rsp->rel_rtype))
				rsp->rel_rtype = M_R_FPTR;
			else
				rsp->rel_rtype = M_R_GLOB_DAT;
			if (add_outrel(FLG_REL_GOT, rsp, reloc, ofl) ==
			    S_ERROR)
				return (S_ERROR);
			rsp->rel_rtype = rtype;
		}
	} else {
		if (assign_got_ndx(&sdp->sd_GOTndxs,
			rsp, reloc, sdp, gnp, ofl) == S_ERROR)
				return (S_ERROR);
	}

	/*
	 * perform relocation to GOT table entry.
	 */
	return (add_actrel(NULL, rsp, reloc, ofl));
}


/*
 * Perform relocations for PLT's
 */
uintptr_t
reloc_plt(Rel_desc *rsp, Rel *reloc, Ofl_desc *ofl)
{
	Sym_desc	*sdp = rsp->rel_sym;

	/*
	 * if (not PLT yet assigned)
	 * then
	 *	assign PLT index to symbol
	 *	build output JMP_SLOT relocation
	 * fi
	 */
	if (sdp->sd_aux->sa_PLTndx == 0) {
		Word	ortype = rsp->rel_rtype;

		assign_plt_ndx(sdp, ofl);

		/*
		 * If this symbol is binding to a LAZYLOADED object then
		 * set the LAZYLD symbol flag.
		 */
		if ((sdp->sd_aux->sa_bindto &&
		    (sdp->sd_aux->sa_bindto->ifl_flags & FLG_IF_LAZYLD)) ||
		    (sdp->sd_file &&
		    (sdp->sd_file->ifl_flags & FLG_IF_LAZYLD)))
			sdp->sd_flags |= FLG_SY_LAZYLD;

		rsp->rel_rtype = M_R_JMP_SLOT;
		if (add_outrel(FLG_REL_PLT, rsp, reloc, ofl) ==
		    S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
	}
	/*
	 * perform relocation to PLT table entry
	 */
	if ((ofl->ofl_flags & FLG_OF_SHAROBJ) &&
	    IS_ADD_RELATIVE(rsp->rel_rtype)) {
		Word	ortype	= rsp->rel_rtype;

		rsp->rel_rtype = M_R_RELATIVE;
		if (add_outrel(FLG_REL_ADVAL,
		    rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
		return (1);
	} else
		return (add_actrel(NULL, rsp, reloc, ofl));
}


/*
 * process GLOBAL undefined and ref_dyn_need symbols.
 */
uintptr_t
reloc_exec(Rel_desc *rsp, Rel *reloc, Ofl_desc *ofl)
{
	Sym_desc	*_sdp, *sdp = rsp->rel_sym;
	Sym_aux		*sap = sdp->sd_aux;
	Sym		*sym = sdp->sd_sym;

	/*
	 * Reference is to a function so simply create a plt entry for it.
	 */
	if (ELF_ST_TYPE(sym->st_info) == STT_FUNC)
		return (reloc_plt(rsp, reloc, ofl));

	/*
	 * Catch absolutes - these may cause a text relocation.
	 */
	if (sym->st_shndx == SHN_ABS) {
		if ((ofl->ofl_flags1 & FLG_OF1_ABSEXEC) == 0)
			return (add_outrel(NULL, rsp, reloc, ofl));
		/*
		 * If -zabsexec is set then promote the ABSOLUTE
		 * symbol to current the current object and
		 * perform the relocation now.
		 */
		sdp->sd_ref = REF_REL_NEED;
		return (add_actrel(NULL, rsp, reloc, ofl));
	}

	/*
	 * If the relocation is against a writable section simply compute the
	 * necessary output relocation.  As an optimization, if the symbol has
	 * already been transformed into a copy relocation then we can perform
	 * the relocation directly (copy relocations should only be generated
	 * for references from the text segment and these relocations are
	 * normally carried out before we get to the data segment relocations).
	 */
	if ((ELF_ST_TYPE(sym->st_info) == STT_OBJECT) &&
	    (rsp->rel_osdesc->os_shdr->sh_flags & SHF_WRITE)) {
		if (sdp->sd_flags & FLG_SY_MVTOCOMM)
			return (add_actrel(NULL, rsp, reloc, ofl));
		else
			return (add_outrel(NULL, rsp, reloc, ofl));
	}

	/*
	 * If the reference isn't to an object (normally because some idiot
	 * hasn't defined a .type directive in some assembler source), then
	 * simply apply a generic relocation (this has a tendency to result in
	 * text relocations).
	 */
	if (ELF_ST_TYPE(sym->st_info) != STT_OBJECT) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_REL_UNEXPSYM),
		    conv_info_type_str(ofl->ofl_e_machine,
		    ELF_ST_TYPE(sym->st_info)),
		    rsp->rel_fname, rsp->rel_sname,
		    sdp->sd_file->ifl_name);
		return (add_outrel(NULL, rsp, reloc, ofl));
	}

	/*
	 * Prepare for generating a copy relocation.
	 *
	 * If this symbol is one of an alias pair, we need to insure both
	 * symbols become part of the output (the strong symbol will be used to
	 * maintain the symbols state).  And, if we did raise the precedence of
	 * a symbol we need to check and see if this is a weak symbol.  If it is
	 * we want to use it's strong counter part.
	 *
	 * The results of this logic should be:
	 *	rel_usym: assigned to strong
	 *	 rel_sym: assigned to symbol to perform
	 *		  copy_reloc against (weak or strong).
	 */
	if (sap->sa_linkndx) {
		_sdp = sdp->sd_file->ifl_oldndx[sap->sa_linkndx];

		if (_sdp->sd_ref < sdp->sd_ref) {
			_sdp->sd_ref = sdp->sd_ref;
			_sdp->sd_flags |= FLG_SY_REFRSD;

			/*
			 * As we're going to replicate a symbol from a shared
			 * object, retain its correct binding status.
			 */
			if (ELF_ST_BIND(_sdp->sd_sym->st_info) == STB_GLOBAL)
				_sdp->sd_flags |= FLG_SY_GLOBREF;

		} else if (_sdp->sd_ref > sdp->sd_ref) {
			sdp->sd_ref = _sdp->sd_ref;
			sdp->sd_flags |= FLG_SY_REFRSD;

			/*
			 * As we're going to replicate a symbol from a shared
			 * object, retain its correct binding status.
			 */
			if (ELF_ST_BIND(sym->st_info) == STB_GLOBAL)
				sdp->sd_flags |= FLG_SY_GLOBREF;
		}

		/*
		 * If this is a weak symbol then we want to move the strong
		 * symbol into local .bss.  If their is a copy_reloc to be
		 * performed that should still occur against the WEAK symbol.
		 */
		if ((ELF_ST_BIND(sdp->sd_sym->st_info) == STB_WEAK) ||
		    (sdp->sd_flags & FLG_SY_WEAKDEF))
			rsp->rel_usym = _sdp;
	} else
		_sdp = 0;

	/*
	 * If the reference is to an object then allocate space for the object
	 * within the executables .bss.  Relocations will now be performed from
	 * this new location.  If the original shared objects data is
	 * initialized the generate a copy relocation that will copy the data to
	 * the executables .bss at runtime.
	 */
	if (!(rsp->rel_usym->sd_flags & FLG_SY_MVTOCOMM)) {
		/*
		 * Indicate that the symbol(s) against which we're relocating
		 * have been moved to the executables common.  Also, insure that
		 * the symbol(s) remain marked as global, as the shared object
		 * from which they are copied must be able to relocate to the
		 * new common location within the executable.
		 *
		 * Note that even though a new symbol has been generated in the
		 * output files' .bss, the symbol must remain REF_DYN_NEED and
		 * not be promoted to REF_REL_NEED.  sym_validate() still needs
		 * to carry out a number of checks against the symbols binding
		 * that are triggered by the REF_DYN_NEED state.
		 */
		sdp->sd_flags |= (FLG_SY_MVTOCOMM | FLG_SY_GLOBAL);
		sdp->sd_flags &= ~FLG_SY_LOCAL;
		if (_sdp) {
			_sdp->sd_flags |= (FLG_SY_MVTOCOMM | FLG_SY_GLOBAL);
			_sdp->sd_flags &= ~FLG_SY_LOCAL;

			/*
			 * Make sure the symbol has a reference in case of any
			 * error diagnostics against it (perhaps this belongs
			 * to a version that isn't allowable for this build).
			 * The resulting diagnostic (see sym_undef_entry())
			 * might seem a little bogus, as the symbol hasn't
			 * really been referenced by this file, but has been
			 * promoted as a consequence of its alias reference.
			 */
			if (!(_sdp->sd_aux->sa_rfile))
				_sdp->sd_aux->sa_rfile = sdp->sd_aux->sa_rfile;
		}

		/*
		 * Assign the symbol to the bss and insure sufficient alignment
		 * (we don't know the real alignment so we have to make the
		 * worst case guess).
		 */
		_sdp = rsp->rel_usym;
		if (sym_copy(_sdp) == S_ERROR)
			return (S_ERROR);
		_sdp->sd_sym->st_shndx = SHN_COMMON;
		_sdp->sd_sym->st_value =
		    (_sdp->sd_sym->st_size < (M_WORD_ALIGN * 2)) ?
		    M_WORD_ALIGN : M_WORD_ALIGN * 2;

		/*
		 * If the symbol references initialized data indicate that we
		 * need copy relocations at startup.  This is the method by
		 * which global, initialized data is exported from a .so.
		 */
		if (_sdp->sd_isc->is_shdr->sh_type != SHT_NOBITS) {
			Word	rtype = rsp->rel_rtype;

			rsp->rel_rtype = M_R_COPY;
			if (add_outrel(FLG_REL_BSS, rsp, reloc, ofl) == S_ERROR)
				return (S_ERROR);
			rsp->rel_rtype = rtype;
			DBG_CALL(Dbg_syms_reloc(sdp->sd_file->ifl_ehdr,
			    sdp, TRUE));
		} else
			DBG_CALL(Dbg_syms_reloc(sdp->sd_file->ifl_ehdr,
			    sdp, FALSE));
	}
	return (add_actrel(NULL, rsp, reloc, ofl));
}

/*
 * All relocations should have been handled by the other routines.  This
 * routine is hear as a catch all, if we do enter it we've goofed - but
 * we'll try and to the best we can.
 */
uintptr_t
reloc_generic(Rel_desc *rsp, Rel *reloc, Ofl_desc *ofl)
{
	Word		flags = ofl->ofl_flags;

	eprintf(ERR_WARNING, MSG_INTL(MSG_REL_UNEXPREL),
	    conv_reloc_type_str(ofl->ofl_e_machine, rsp->rel_rtype),
	    rsp->rel_isdesc->is_file->ifl_name, rsp->rel_sname);

	/*
	 * If building a shared object then put the relocation off
	 * until runtime.
	 */
	if (flags & FLG_OF_SHAROBJ)
		return (add_outrel(NULL, rsp, reloc, ofl));

	/*
	 * else process relocation now.
	 */
	return (add_actrel(NULL, rsp, reloc, ofl));
}

/*
 * Generate relocation descriptor and dispatch
 */
static uintptr_t
process_reld(Ofl_desc *ofl, Is_desc *rsect,
	Rel_desc *reld, Rel *reloc)
{
	Word		flags = ofl->ofl_flags;
	Sym_desc	*sdp;
	Sym_aux		*sap;
	Word		rstndx;	/* Relocation index of current entry. */
	Word		rtype;	/* Relocation type of current entry. */
	Xword		roffset; /* Relocation offset of current entry. */
	Ifl_desc	*file = rsect->is_file;
	Boolean		local;

	/* LINTED */
	rtype = (Word) ELF_R_TYPE(reloc->r_info);
	/* LINTED */
	rstndx = (Word) ELF_R_SYM(reloc->r_info);
	roffset = reloc->r_offset;


	/*
	 * Special case, a register symbol associated with symbol
	 * index 0 is initialized (i.e. relocated) to a constant
	 * in the r_addend field rather than to a symbol value.
	 */
	if (IS_REGISTER(rtype) && (rstndx == 0)) {
		reld->rel_rtype = rtype;
		reld->rel_sym = NULL;
		reld->rel_roffset = roffset;
		reld->rel_sname = MSG_ORIG(MSG_STR_EMPTY);
		DBG_CALL(Dbg_reloc_in(M_MACH, rsect->is_shdr->sh_type,
			(void *)reloc, reld->rel_sname, rsect->is_name));
		if (reloc_register(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
		return (1);
	}


	/*
	 * Determine whether we're dealing with a global symbol.
	 */
	if (((sdp = file->ifl_oldndx[rstndx]) != 0) &&
	    sdp->sd_name && *sdp->sd_name) {
		/*
		 * It's possible that this symbol is a candidate for
		 * reduction to local. (This test is also carried out
		 * sym_validate()).
		 */
		if (((flags & FLG_OF_AUTOLCL) ||
		    (ofl->ofl_flags1 & FLG_OF1_AUTOELM)) &&
		    (ELF_ST_BIND(sdp->sd_sym->st_info) != STB_LOCAL) &&
		    ((!(sdp->sd_flags & MSK_SY_DEFINED)) &&
		    (sdp->sd_ref == REF_REL_NEED) &&
		    (sdp->sd_sym->st_shndx != SHN_UNDEF))) {
			sdp->sd_flags |=
			    (FLG_SY_LOCAL | FLG_SY_REDUCED);
			if (ofl->ofl_flags1 & FLG_OF1_AUTOELM)
				sdp->sd_flags |= FLG_SY_ELIM;
		}

		/*
		 * If this symbol is part of a DISCARDED section
		 * we attempt to find another definition. of the
		 * symbol.  If we are unable to find a additional
		 * definition then it's a fatal error.
		 */
		if (sdp->sd_flags & FLG_SY_ISDISC) {
			Sym_desc *	nsdp;
			Word		h;

			/* LINTED */
			h = (Word)elf_hash(sdp->sd_name);
			if ((ELF_ST_BIND(sdp->sd_sym->st_info) ==
			    STB_LOCAL) || ((nsdp = sym_find(
			    sdp->sd_name, h, ofl))
			    == 0)) {
				eprintf(ERR_FATAL,
				    MSG_INTL(MSG_REL_SYMDISC),
				    file->ifl_name,
				    rsect->is_name,
				    sdp->sd_name,
				    sdp->sd_isc->is_name);
				return (S_ERROR);
			}
			file->ifl_oldndx[rstndx] = sdp = nsdp;
		}
		reld->rel_sname = sdp->sd_name;
	} else {
		static char *strunknown;
		if (strunknown == NULL)
			strunknown = (char *)MSG_INTL(MSG_STR_UNKNOWN);
		reld->rel_sname = strunknown;
	}

	DBG_CALL(Dbg_reloc_in(M_MACH, rsect->is_shdr->sh_type,
		(void *)reloc, reld->rel_sname, rsect->is_name));

	/*
	 * If for some reason we have a null relocation record issue a
	 * warning and continue (the compiler folks can get into this
	 * state some time).  Normal users should never see this error.
	 */
	if (rtype == M_R_NONE) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_REL_NULL),
			file->ifl_name, rsect->is_name);
		return (1);
	}

	if (((flags & FLG_OF_RELOBJ) == 0) && IS_NOTSUP(rtype)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOTSUP),
			file->ifl_name, rsect->is_name,
			conv_reloc_type_str(file->ifl_ehdr->e_machine, rtype));
		return (S_ERROR);
	}

	/*
	 * If we have a relocation against a section that will
	 * not be in the output file (has been stripped) then
	 * ignore the relocation.
	 */
	if ((sdp->sd_isc == 0) &&
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))
		return (1);

	/*
	 * If the symbol for this relocation is invalid (which should
	 * have generated a message during symbol processing), or the
	 * relocation record's symbol reference is in any other way
	 * invalid, then its about time we gave up.
	 */
	if ((sdp->sd_flags & FLG_SY_INVALID) ||
	    (rstndx == 0) || (rstndx >= file->ifl_symscnt)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNKNWSYM),
		    M_REL_CONTYPSTR(rtype), file->ifl_name,
		    rsect->is_name, reld->rel_sname,
		    EC_XWORD(roffset), EC_WORD(rstndx));
		return (S_ERROR);
	}

	/*
	 * Indicate that this symbol is being used for relocation and
	 * therefore it must have its output address updated
	 * accordingly (refer to update_osym()).
	 */
	sdp->sd_flags |= FLG_SY_UPREQD;

	reld->rel_sym = reld->rel_usym = sdp;
	reld->rel_rtype = rtype;

	/*
	 * For all symbols check if this symbol is actually an alias to
	 * another symbol.  If so, and the symbol we are aliased to is
	 * not REF_DYN_SEEN then we will set rel_usym to point to the
	 * weaks strong counter-part.
	 * The one exception is if the FLG_SY_MVTOCOMM flag is set on
	 * the WEAK symbol.  If this is the case then the strong is only
	 * here because of its promotion and we should still use the
	 * WEAK for the relocation reference (see reloc_exec()).
	 */
	sap = sdp->sd_aux;
	if (sap && sap->sa_linkndx &&
	    ((ELF_ST_BIND(sdp->sd_sym->st_info) == STB_WEAK) ||
	    (sdp->sd_flags & FLG_SY_WEAKDEF)) &&
	    (!(sdp->sd_flags & FLG_SY_MVTOCOMM))) {
		Sym_desc *	_sdp;

		_sdp = sdp->sd_file->ifl_oldndx[sap->sa_linkndx];
		if (_sdp->sd_ref != REF_DYN_SEEN)
			reld->rel_usym = _sdp;
	}

	/*
	 * Determine whether this symbol should be bound locally or not.
	 * Symbols will be bound locally if one of the following is
	 * true:
	 *
	 *  o	the symbol is of type STB_LOCAL
	 *
	 *  o	the output image is not a relocatable object and the
	 *	relocation is relative to the .got
	 *
	 *  o	ia64: the output image is not a relocatable object and
	 *	the relocation is a 'segment relative' relocation.  These
	 *	relcoations must be bound at link-edit time.
	 *
	 *  o	the symbol has been reduced (scoped to a local or
	 *	symbolic) and reductions are being processed
	 *
	 *  o	the -Bsymbolic flag is in use when building a shared
	 *	object or an executable (fixed address) is being created
	 *
	 *  o	Building an executable and the symbol is defined
	 *	in the executable.
	 *
	 *  o	the relocation is against a segment which will not
	 *	be loaded into memory.  If that is the case we need
	 *	to resolve the relocation now because ld.so.1 won't
	 *	be able to.
	 */
	local = FALSE;
	if (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL)
		local = TRUE;
	else if (!(reld->rel_flags & FLG_REL_LOAD))
		local = TRUE;
	else if (sdp->sd_sym->st_shndx != SHN_UNDEF) {
		if (!(flags & FLG_OF_RELOBJ) &&
		    (IS_GOT_PC(rtype) || IS_SEG_RELATIVE(rtype)))
			local = TRUE;
		else if (sdp->sd_ref == REF_REL_NEED) {
			if ((sdp->sd_flags &
			    (FLG_SY_LOCAL | FLG_SY_SYMBOLIC)))
				local = TRUE;
			else if (flags &
			    (FLG_OF_SYMBOLIC | FLG_OF_EXEC))
				local = TRUE;
		}
	}

	/*
	 * Select the relocation to perform.
	 */
	if (IS_REGISTER(rtype)) {
		if (reloc_register(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	} else if (flags & FLG_OF_RELOBJ) {
		if (reloc_relobj(local, reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	} else if (IS_GOT_RELATIVE(rtype)) {
		if (reloc_GOT_relative(local, reld, reloc, ofl) ==
		    S_ERROR)
			return (S_ERROR);
	} else if (local) {
		if (reloc_local(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	} else if ((IS_PLT(rtype)) && ((flags & FLG_OF_BFLAG) == 0)) {
		if (reloc_plt(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	} else if ((sdp->sd_ref == REF_REL_NEED) ||
	    (flags & FLG_OF_BFLAG) || (flags & FLG_OF_SHAROBJ) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_NOTYPE)) {
		if (add_outrel(NULL, reld, reloc, ofl) ==
		    S_ERROR)
			return (S_ERROR);
	} else if (sdp->sd_ref == REF_DYN_NEED) {
		if (reloc_exec(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	} else {
		/*
		 * IS_NOT_REL(rtype)
		 */
		if (reloc_generic(reld, reloc, ofl) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}

uintptr_t
process_reloc(Ofl_desc *ofl, Is_desc *isect, Is_desc *rsect, Os_desc *osect)
{
	Rel		*rend;	/* Just past end of relocation section data. */
	Rel		*reloc;	/* Current entry in relocation section data. */
	Xword		rsize;	/* Size of relocation section data. */
	Word		entsize; /* Size of relocation entry */
	Rel_desc	reld;	/* holder for add_{out|in}rel() processing */
	Word		flags = ofl->ofl_flags;
	Ifl_desc	*file = rsect->is_file;

	rsize = rsect->is_shdr->sh_size;
	reloc = (Rel *)rsect->is_indata->d_buf;

	/*
	 * Decide entry size
	 */
	entsize = rsect->is_shdr->sh_entsize;
	if ((entsize == 0) ||
	    (entsize > rsect->is_shdr->sh_size)) {
		if (rsect->is_shdr->sh_type == SHT_RELA)
			entsize = sizeof (Rela);
		else
			entsize = sizeof (Rel);
	}

	/*
	 * Build up the basic information in for the Rel_desc structure.
	 */
	reld.rel_osdesc = osect;
	reld.rel_isdesc = isect;
	reld.rel_risdesc = rsect;
	reld.rel_fname = file->ifl_name;
	reld.rel_flags = 0;
	if (((!(flags & FLG_OF_RELOBJ)) && osect &&
	    (osect->os_sgdesc->sg_phdr.p_type == PT_LOAD)) ||
	    (flags & FLG_OF_RELOBJ))
		reld.rel_flags |= FLG_REL_LOAD;
	if (rsect->is_shdr->sh_info == 0)
		reld.rel_flags |= FLG_REL_NOINFO;
	reld.rel_mventry = (Move *)NULL;

	DBG_CALL(Dbg_reloc_proc(osect, isect, rsect));
	for (rend = (Rel *)((uintptr_t)reloc + (uintptr_t)rsize);
	    reloc < rend;
	    reloc = (Rel *)((uintptr_t)reloc + entsize)) {
		/*
		 * Generate Relocation Descriptor.
		 */
		if (process_reld(ofl, rsect, &reld, reloc) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}


uintptr_t
reloc_segments(int wr_flag, Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Sg_desc		*sgp;
	Is_desc		*isp;

	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Os_desc		*osp;
		Listnode	*lnp2;

		if ((sgp->sg_phdr.p_flags & PF_W) != wr_flag)
			continue;

		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			Is_desc		*risp;
			Listnode	*lnp3;

			osp->os_szoutrels = 0;
			for (LIST_TRAVERSE(&(osp->os_relisdescs), lnp3, risp)) {
				Word		indx;

				/*
				 * Determine the input section that this
				 * relocation information refers to.
				 */
				indx = risp->is_shdr->sh_info;
				isp = risp->is_file->ifl_isdesc[indx];

				/*
				 * Do not process relocations against sections
				 * which are being discarded (COMDAT)
				 */
				if (isp->is_flags & FLG_IS_DISCARD)
					continue;

				if (process_reloc(ofl, isp, risp, osp) ==
				    S_ERROR)
					return (S_ERROR);
			}
			ofl->ofl_relocsz += osp->os_szoutrels;


			/*
			 * Create output relocation section if necessary.
			 */
			if (osp->os_szoutrels) {
				if (!(ofl->ofl_flags1 & FLG_OF1_RELCNT))
					if (make_reloc(ofl, osp) == S_ERROR)
						return (S_ERROR);

				/*
				 * Check for relocations against non-writable
				 * allocatable sections.
				 */
				if ((sgp->sg_phdr.p_type == PT_LOAD) &&
				    ((sgp->sg_phdr.p_flags & PF_W) == 0))
					ofl->ofl_flags |= FLG_OF_TEXTREL;
			}
		}
	}

	return (1);
}

/*
 * Move Section related function
 * Get move entry
 */
static Move *
get_move_entry(Is_desc *rsect, Xword roffset)
{
	Ifl_desc	*ifile = rsect->is_file;
	Shdr		*rshdr = rsect->is_shdr;
	Is_desc		*misp;
	Shdr		*mshdr;
	Xword 		midx;
	Move		*ret;

	/*
	 * Set info for the target move section
	 */
	misp = ifile->ifl_isdesc[rshdr->sh_info];
	mshdr = (ifile->ifl_isdesc[rshdr->sh_info])->is_shdr;
	midx = roffset / mshdr->sh_entsize;

	ret = (Move *)misp->is_indata->d_buf;
	ret += midx;

	/*
	 * If this is an illgal entry, retun NULL and
	 * it will be ignored.
	 */
	if (midx*mshdr->sh_entsize >= mshdr->sh_size)
		return ((Move *)0);
	return (ret);
}

/*
 * Relocation against Move Table.
 */
static uintptr_t
process_movereloc(Ofl_desc *ofl, Is_desc *rsect)
{
	Ifl_desc	*file = rsect->is_file;
	Rel		*rend;
	Rel		*reloc;
	Xword 		rsize;
	Word		entsize;
	static Rel_desc reld_zero;
	Rel_desc 	reld;

	rsize = rsect->is_shdr->sh_size;
	reloc = (Rel *)rsect->is_indata->d_buf;

	reld = reld_zero;

	/*
	 * Decide entry size
	 */
	entsize = rsect->is_shdr->sh_entsize;
	if ((entsize == 0) ||
	    (entsize > rsect->is_shdr->sh_size)) {
		if (rsect->is_shdr->sh_type == SHT_RELA)
			entsize = sizeof (Rela);
		else
			entsize = sizeof (Rel);
	}

	/*
	 * Go through the relocation entries.
	 */
	for (rend = (Rel *)((uintptr_t)reloc + (uintptr_t)rsize);
	    reloc < rend;
	    reloc = (Rel *)((uintptr_t)reloc + entsize)) {
		Sym_desc *	psymd;	/* partially initialized sybol used */
		Move *		mventry;
		int i;

		reld.rel_flags = FLG_REL_LOAD;

		mventry = get_move_entry(rsect, reloc->r_offset);
		if (mventry == (Move *) 0) {
			/*
			 * Ignore the illegal entries.
			 */
			eprintf(ERR_WARNING, MSG_INTL(MSG_PSYM_WARN2));
			continue;
		}
		psymd = file->ifl_oldndx[ELF_M_SYM(mventry->m_info)];
		reld.rel_psym = psymd;		/* save the partial */
		reld.rel_mventry = mventry;	/* save move entry */
		reld.rel_risdesc = rsect;

		if (psymd->sd_flags & FLG_SY_PAREXPN) {
			int num;
			reld.rel_osdesc = ofl->ofl_issunwdata1->is_osdesc;
			reld.rel_isdesc = ofl->ofl_issunwdata1;
			num = mventry->m_repeat;
			reld.rel_roffset = mventry->m_poffset;
			for (i = 0; i < num; i++) {
				reld.rel_roffset +=
					/* LINTED */
					i*ELF_M_SIZE(mventry->m_info);
				/*
				 * Generate Reld
				 */
				if (process_reld(ofl,
				    rsect, &reld, reloc) == S_ERROR)
					return (S_ERROR);
			}
		} else {
			/*
			 * Generate Reld
			 */
			reld.rel_flags |= FLG_REL_MOVETAB;
			reld.rel_osdesc = ofl->ofl_osmove;
			reld.rel_isdesc =
				ofl->ofl_osmove->os_isdescs.head->data;
			if (process_reld(ofl,
			    rsect, &reld, reloc) == S_ERROR)
				return (S_ERROR);
		}
	}
	return (1);
}

/*
 * This function is similar to reloc_init().
 *
 * This function is called when the SHT_SUNW_move table is expanded
 * and there were relocation against the SHT_SUNW_move section.
 */
static uintptr_t
reloc_movesections(Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Is_desc		*risp;
	Os_desc		*osp;

	/*
	 * Generate/Expand relocation entries
	 */
	for (LIST_TRAVERSE(&ofl->ofl_mvrelisdescs, lnp1, risp)) {
		if (process_movereloc(ofl, risp) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Create output relocation section if necessary.
	 * Check both for .SUNW_move and .sunwdata1.
	 */
	osp = ofl->ofl_osmove;
	if (osp && osp->os_szoutrels) {
		if (!(ofl->ofl_flags1 & FLG_OF1_RELCNT))
			ofl->ofl_relocsz += osp->os_szoutrels;
			if (make_reloc(ofl, osp) == S_ERROR)
				return (S_ERROR);
	}
	if (ofl->ofl_issunwdata1) {
		osp = ofl->ofl_issunwdata1->is_osdesc;
		if (!(ofl->ofl_flags1 & FLG_OF1_RELCNT) &&
		    (osp->os_szoutrels != 0)) {
			ofl->ofl_relocsz += osp->os_szoutrels;
			if (make_reloc(ofl, osp) == S_ERROR)
				return (S_ERROR);
		}
	}
	return (1);
}

/*
 * Count the number of output relocation entries and global offset table or
 * procedure linkage table entries.  This function searches the segment and
 * outsect lists and passes each input reloc section to process_reloc().
 * It allocates space for any output relocations needed.  And builds up
 * the relocation structures for latter processing.
 */
uintptr_t
reloc_init(Ofl_desc *ofl)
{
	Listnode	*lnp;
	Is_desc		*isp;

	/*
	 * At this point we have finished processing all input symbols.  Make
	 * sure we add any absolute (internal) symbols before continuing with
	 * any relocation processing.
	 */
	if (sym_spec(ofl) == S_ERROR)
		return (S_ERROR);

	ofl->ofl_gotcnt = M_GOT_XNumber;

	/*
	 * We first process all of the relocations against NON-Writable
	 * segments and then we do them against the writeable segments.
	 *
	 * This is so that when we process the writable segments we
	 * know whether or not a COPYRELOC will be produced for any
	 * symbols.  If we don't do relocations in this order we
	 * might produce both a COPYRELOC and a regular relocation
	 * against the same symbol, the regular relocation is redundant.
	 */
	if (reloc_segments(0, ofl) == S_ERROR)
		return (S_ERROR);

	if (reloc_segments(PF_W, ofl) == S_ERROR)
		return (S_ERROR);

	for (LIST_TRAVERSE(&ofl->ofl_extrarels, lnp, isp)) {
		if (process_reloc(ofl, NULL, isp, NULL) == S_ERROR)
			return (S_ERROR);
	}


	/*
	 * If there were relocation against move table,
	 * process the relocation sections.
	 */
	if (reloc_movesections(ofl) == S_ERROR)
		return (S_ERROR);

	if (ofl->ofl_flags1 & FLG_OF1_RELCNT) {
		/*
		 * If -zcombreloc is enabled then all
		 * of the relocations (except for the PLT's) are coalesced
		 * into a single relocation section for performance reasons.
		 */
		if (ofl->ofl_reloccnt) {
			if (make_reloc(ofl, 0) == S_ERROR)
				return (S_ERROR);
		}
	} else {
		/*
		 * If we're not building a 'zcombreloc' then
		 * build a .rel[a] section if required.
		 */
		if (ofl->ofl_relocrelsz) {
			if (make_reloc(ofl, NULL) == S_ERROR)
				return (S_ERROR);
		}
	}

	/*
	 * Calculate the total relocation size (to be stored in .dynamic).
	 */
	ofl->ofl_relocsz += ofl->ofl_relocgotsz + ofl->ofl_relocpltsz +
		ofl->ofl_relocbsssz + ofl->ofl_relocrelsz;

	/*
	 * Make a got section if:
	 *
	 *	BLDGOT flag set
	 *		or
	 *	ofl_gotcnt != GOT_XNumber
	 *		or
	 *	not-relobj & GOT symbol referenced
	 *
	 */
	if ((ofl->ofl_flags & FLG_OF_BLDGOT) ||
	    (ofl->ofl_gotcnt != M_GOT_XNumber) ||
	    (((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) &&
	    ((sym_find(MSG_ORIG(MSG_SYM_GOFTBL), SYM_NOHASH, ofl) != 0) ||
	    (sym_find(MSG_ORIG(MSG_SYM_GOFTBL_U), SYM_NOHASH, ofl) != 0)))) {
		if (make_got(ofl) == S_ERROR)
			return (S_ERROR);

#if defined(sparc) || defined(__sparcv9)
		if (allocate_got(ofl) == S_ERROR)
			return (S_ERROR);
#elif defined(i386) || defined(__ia64)
/* nothing to do */
#else
#error Unknown architecture!
#endif
	}
	return (1);
}

/*
 * Simple comparison routine to be used by qsort() for
 * the sorting of the output relocation list.
 */
int
reloc_compare(Reloc_list *i, Reloc_list *j)
{
	/* LINTED */
	return (int)((unsigned long)i->rl_key - (unsigned long)j->rl_key);
}


uintptr_t
do_sorted_outrelocs(Ofl_desc *ofl)
{
	Rel_desc	*orsp;
	Rel_cache	*rcp;
	Listnode	*lnp;
	Reloc_list	*sorted_list;
	long		index = 0;
	int		debug = 0;
	uintptr_t	error = 1;

	if ((sorted_list = libld_malloc((size_t)(sizeof (Reloc_list) *
	    ofl->ofl_reloccnt))) == NULL)
		return (S_ERROR);

	/*
	 * All but the PLT output relocations are sorted in the output file
	 * based upon their sym_desc.  By doing this multiple relocations
	 * against the same symbol are grouped together, thus when the object
	 * is later relocated by ld.so.1 it will take advantage of the symbol
	 * cache that ld.so.1 has.  This can significantly reduce the runtime
	 * relocation cost of a dynamic object.
	 *
	 * PLT relocations are not sorted because the order of the PLT
	 * relocations is used by ld.so.1 to determine what symbol a PLT
	 * relocation is against.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_outrels, lnp, rcp)) {
		/*LINTED*/
		for (orsp = (Rel_desc *)(rcp + 1);
		    orsp < rcp->rc_free; orsp++) {
			if (debug == 0) {
				DBG_CALL(Dbg_reloc_dooutrel(orsp));
				debug = 1;
			}

			/*
			 * If it's a PLT relocation we output it now in the
			 * order that it was originally processed.
			 */
			if (orsp->rel_flags & FLG_REL_PLT) {
				if (perform_outreloc(orsp, ofl) == S_ERROR)
					error = S_ERROR;
			} else {
				if (orsp->rel_rtype == M_R_RELATIVE)
					sorted_list[index].rl_key =
					    (Sym_desc *) M_R_RELATIVE;
				else
					sorted_list[index].rl_key =
					    orsp->rel_sym;

				sorted_list[index++].rl_rsp = orsp;
			}
		}
	}

	qsort(sorted_list, (size_t)ofl->ofl_reloccnt, sizeof (Reloc_list),
		(int (*)(const void *, const void *))reloc_compare);

	/*
	 * All output relocations have now been sorted, go through
	 * and process each relocation.
	 */
	for (index = 0; index < ofl->ofl_reloccnt; index++)
		if (perform_outreloc(sorted_list[index].rl_rsp, ofl) ==
		    S_ERROR)
			error = S_ERROR;

	return (error);
}

/*
 * Process relocations.  Finds every input relocation section for each output
 * section and invokes reloc_sec() to relocate that section.
 */
uintptr_t
reloc_process(Ofl_desc *ofl)
{
	Listnode	*lnp1;
	Sg_desc		*sgp;
	Word		ndx = 0;
	Word		flags = ofl->ofl_flags;
	Shdr		*shdr;

	/*
	 * Determine the index of the symbol table that will be referenced by
	 * the relocation entries.
	 */
	if ((flags & (FLG_OF_DYNAMIC|FLG_OF_RELOBJ)) == FLG_OF_DYNAMIC)
		/* LINTED */
		ndx = (Word)elf_ndxscn(ofl->ofl_osdynsym->os_scn);
	else if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ))
		/* LINTED */
		ndx = (Word)elf_ndxscn(ofl->ofl_ossymtab->os_scn);

	/*
	 * Re-initialize counters. These are used to provide relocation
	 * offsets within the output buffers.
	 */
	ofl->ofl_relocpltsz = 0;
	ofl->ofl_relocgotsz = 0;
	ofl->ofl_relocbsssz = 0;

	/*
	 * Now that we have created out output file and updated all of
	 * the symbols in the symbol table we can now process the
	 * relocations that were built up in process_reloc()
	 */
	if (do_sorted_outrelocs(ofl) == S_ERROR)
		return (S_ERROR);
	if (do_activerelocs(ofl) == S_ERROR)
		return (S_ERROR);

	if ((ofl->ofl_flags1 & FLG_OF1_RELCNT) == 0) {
		/*
		 * Process the relocation sections:
		 *
		 *  o	for each relocation section generated for the output
		 *	image update its shdr information to reflect the
		 *	symbol table it needs (sh_link) and the section to
		 *	which the relocation must be applied (sh_info).
		 */
		for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
			Listnode *	lnp2;
			Os_desc *	osp;

			for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
				if (osp->os_relosdesc) {

					shdr = osp->os_relosdesc->
						os_shdr;
					shdr->sh_link = ndx;
					shdr->sh_info =
					    /* LINTED */
					    (Word)elf_ndxscn(osp->os_scn);
				}
			}
		}

		/*
		 * Since the .rel[a] section is not tied to any specific
		 * section, we'd of not found it above.
		 */
		if (ofl->ofl_osrel) {
			shdr = ofl->ofl_osrel->os_shdr;
			shdr->sh_link = ndx;
			shdr->sh_info = 0;
		}
	} else {
		/*
		 * We only have two relocation sections here, (PLT's,
		 * coalesced) so just hit them directly instead of stepping
		 * over the output sections.
		 */
		if (ofl->ofl_osrelhead) {
			shdr = ofl->ofl_osrelhead->os_shdr;
			shdr->sh_link = ndx;
			shdr->sh_info = 0;
		}
		if (ofl->ofl_osplt && ofl->ofl_osplt->os_relosdesc) {
			shdr = ofl->ofl_osplt->os_relosdesc->os_shdr;
			shdr->sh_link = ndx;
			/* LINTED */
			shdr->sh_info = (Word)elf_ndxscn(
			    ofl->ofl_osplt->os_scn);
		}
	}

	/*
	 * If the -z text option was given, and we have output relocations
	 * against a non-writable, allocatable section, issue a diagnostic and
	 * return (the actual entries that caused this error would have been
	 * output during the relocating section phase).
	 */
	if ((flags & (FLG_OF_PURETXT | FLG_OF_TEXTREL)) ==
	    (FLG_OF_PURETXT | FLG_OF_TEXTREL)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_REMAIN_3));
		return (S_ERROR);
	}

	/*
	 * Finally, initialize the first got entry with the address of the
	 * .dynamic section (_DYNAMIC).
	 */
	if (flags & FLG_OF_DYNAMIC)
		fillin_gotplt1(ofl);

	return (1);
}

/*
 * If the -z text option was given, and we have output relocations against a
 * non-writable, allocatable section, issue a diagnostic. Print offending
 * symbols in tabular form similar to the way undefined symbols are presented.
 * Called from reloc_count().  The actual fatal error condition is triggered on
 * in reloc_process() above.
 *
 * Note.  For historic reasons -ztext is not a default option (however all OS
 * shared object builds use this option).  It can be argued that this option
 * should also be default when generating an a.out (see 1163979).  However, if
 * an a.out contains text relocations it is either because the user is creating
 * something pretty weird (they've used the -b or -znodefs options), or because
 * the library against which they're building wasn't constructed correctly (ie.
 * a function has a NOTYPE type, in which case the a.out won't generate an
 * associated plt).  In the latter case the builder of the a.out can't do
 * anything to fix the error - thus we've chosen not to give the user an error,
 * or warning, for this case.
 */

void
reloc_remain_title(int warning)
{
	const char	*str1;

	if (warning)
		str1 = MSG_INTL(MSG_REL_RMN_ITM_13);
	else
		str1 = MSG_INTL(MSG_REL_RMN_ITM_11);

	eprintf(ERR_NONE, MSG_INTL(MSG_REL_REMAIN_FMT_1),
		str1,
		MSG_INTL(MSG_REL_RMN_ITM_31),
		MSG_INTL(MSG_REL_RMN_ITM_12),
		MSG_INTL(MSG_REL_RMN_ITM_2),
		MSG_INTL(MSG_REL_RMN_ITM_32));

}

void
reloc_remain_entry(Rel_desc *orsp, Os_desc *osp, Ofl_desc *ofl)
{
	static Boolean	reloc_title = TRUE;

	/*
	 * -ztextoff
	 */
	if (ofl->ofl_flags1 & FLG_OF1_TEXTOFF)
		return;

	/*
	 * Only give relocation errors against loadable read-only segments.
	 */
	if ((orsp->rel_rtype == M_R_REGISTER) ||
	    (osp->os_sgdesc->sg_phdr.p_type != PT_LOAD) ||
	    (osp->os_sgdesc->sg_phdr.p_flags & PF_W))
		return;

	/*
	 * If we are in -ztextwarn mode, it's a silent error if a relocation is
	 * due to a 'WEAK REFERENCE'.  This is because if the symbol is not
	 * provided at run-time we will not perform a text-relocation.
	 */
	if (((ofl->ofl_flags & FLG_OF_PURETXT) == 0) &&
	    (ELF_ST_BIND(orsp->rel_sym->sd_sym->st_info) == STB_WEAK) &&
	    (orsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF))
		return;

	if (reloc_title) {
		/*
		 * If building with '-ztext' then emit a fatal error.  If
		 * building a executable then only emit a 'warning'.
		 */
		if (ofl->ofl_flags & FLG_OF_PURETXT)
			reloc_remain_title(0);
		else
			reloc_remain_title(1);
		reloc_title = FALSE;
	}

	eprintf(ERR_NONE, MSG_INTL(MSG_REL_REMAIN_2), orsp->rel_sname,
	    EC_XWORD(orsp->rel_roffset), orsp->rel_fname);
}

/*
 * The following functions are called from
 * machine functions defined in {sparc,i386,sparcv9}/machrel.c
 */

/*
 * Move Section related function
 */
static uintptr_t
newroffset_for_move(Sym_desc *symd,
	Move *mventry, Xword offset1, Xword *offset2)
{
	Psym_info	*psym = symd->sd_psyminfo;
	Mv_itm		*itm;
	Listnode	*lnp1;
	int 		found = 0;

	/*
	 * Search for matching move entry
	 */
	found = 0;
	for (LIST_TRAVERSE(&psym->psym_mvs, lnp1, itm)) {
		if (itm->mv_ientry == mventry) {
			found = 1;
			break;
		}
	}
	if (found == 0) {
		/*
		 * This should never happen.
		 */
		return (S_ERROR);
	}

	/*
	 * Update r_offset
	 */
	*offset2 = (Xword)((itm->mv_oidx - 1)*sizeof (Move) +
		offset1 % sizeof (Move));
	return (1);
}

/*
 *
 */
void
adj_expandreloc(Ofl_desc *ofl, Rel_desc *arsp)
{
	arsp->rel_osdesc = ofl->ofl_issunwdata1->is_osdesc;
	arsp->rel_roffset =
		(arsp->rel_psym->sd_sym->st_value -
		ofl->ofl_issunwdata1->is_osdesc->os_shdr->sh_addr) +
		arsp->rel_roffset;
	DBG_CALL(Dbg_move_adjexpandreloc(
		arsp->rel_roffset,
		arsp->rel_psym->sd_name));
}

/*
 *
 */
void
adj_movereloc(Ofl_desc *ofl, Rel_desc *arsp)
{
	Sym_desc	*psdp = arsp->rel_psym;
	Xword 		newoffset;

	arsp->rel_osdesc = ofl->ofl_osmove;
	(void) newroffset_for_move(psdp, arsp->rel_mventry,
		arsp->rel_roffset, &newoffset);
	DBG_CALL(Dbg_move_adjmovereloc(
		arsp->rel_roffset,
		newoffset,
		arsp->rel_psym->sd_name));
	arsp->rel_roffset = newoffset;
}

/*
 * Because of the combinations of 32-bit lib providing 64-bit support, and
 * visa-versa, the use of krtld's dorelocs can result in differing message
 * requirements that make msg.c/msg.h creation and chkmsg "interesting".
 * Thus the actual message files contain a couple of entries to satisfy
 * each architectures build.  Here we add dummy calls to quieten chkmsg.
 *
 * chkmsg: MSG_INTL(MSG_REL_NOFIT)
 * chkmsg: MSG_INTL(MSG_REL_NONALIGN)
 */
