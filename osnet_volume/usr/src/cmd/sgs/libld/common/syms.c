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
#pragma ident	"@(#)syms.c	1.84	99/12/01 SMI"

/*
 * Symbol table management routines
 */
#include	<stdio.h>
#include	<string.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"


/*
 * If, during symbol processing, it is necessary to update a local symbols
 * contents before we have generated the symbol tables in the output image,
 * create a new symbol structure and copy the original symbol contents.  While
 * we are processing the input files, their local symbols are part of the
 * read-only mapped image.  Commonly, these symbols are copied to the new output
 * file image and then updated to reflect their new address and any change in
 * attributes.  However, sometimes during relocation counting, it is necessary
 * to adjust the symbols information.  This routine provides for the generation
 * of a new symbol image so that this update can be performed.
 * All global symbols are copied to an internal symbol table to improve locality
 * of reference and hence performance, and thus this copying is not necessary.
 */
uintptr_t
sym_copy(Sym_desc *sdp)
{
	Sym	*nsym;

	if (sdp->sd_flags & FLG_SY_CLEAN) {
		if ((nsym = (Sym *)libld_malloc(sizeof (Sym))) == 0)
			return (S_ERROR);
		*nsym = *(sdp->sd_sym);
		sdp->sd_sym = nsym;
		sdp->sd_flags &= ~FLG_SY_CLEAN;
	}
	return (1);
}

/*
 * Finds a given name in the link editors internal symbol table.  If no
 * hash value is specified it is calculated.  A pointer to the located
 * Sym_desc entry is returned, or NULL if the symbol is not found.
 */
Sym_desc *
sym_find(const char *name, Word hash, Ofl_desc *ofl)
{
	Word		bkt = 0;
	Sym_cache	*scp;
	Sym_desc	*sdp;

	if (hash == SYM_NOHASH)
		/* LINTED */
		hash = (Word)elf_hash((const char *)name);
	if (hash)
		bkt = (Word)(hash % ofl->ofl_symbktcnt);

	for (scp = ofl->ofl_symbkt[bkt]; scp; scp = scp->sc_next) {
		/*LINTED*/
		for (sdp = (Sym_desc *)(scp + 1); sdp < scp->sc_free; sdp++) {
			const char	*_name = sdp->sd_name;

			/*
			 * We get a performance improvement if we compare the
			 * first couple of characters before calling strcmp().
			 * Only check the first two, as the smallest possible
			 * symbol is a single character followed by a null.
			 */
			if (_name[0] != name[0])
				continue;
			if (_name[1] != name[1])
				continue;
			if ((_name[1] == '\0') ||
			    (strcmp(&_name[2], &name[2]) == 0))
				return (sdp);
		}
	}
	return (NULL);
}


/*
 * Enter a new symbol into the link editors internal symbol table.
 * If the symbol is from an input file, information regarding the input file
 * and input section is also recorded.  Otherwise (file == NULL) the symbol
 * has been internally generated (ie. _etext, _edata, etc.).
 */
Sym_desc *
sym_enter(const char *name, Sym *osym, Word hash, Ifl_desc *ifl,
	Ofl_desc *ofl, Word ndx)
{
	Word		bkt;
	Sym_desc	*sdp;
	Sym_aux		*sap;
	Sym_cache	*scp, *_scp = 0;
	char		*_name;
	Sym		*nsym;
	Half		shndx = osym->st_shndx;
	Half		etype;
	Ehdr		*ehdr;

	if (ifl)
		etype = ifl->ifl_ehdr->e_type;
	else
		etype = ET_NONE;

	/*
	 * From the symbols hash value determine which bucket we need.  Scan the
	 * linked list of symbol cache structures until we find one with space
	 * to add a new identifier.
	 */
	bkt = (Word)(hash % ofl->ofl_symbktcnt);
	for (scp = ofl->ofl_symbkt[bkt]; scp; _scp = scp, scp = scp->sc_next)
		if ((sdp = scp->sc_free) != scp->sc_end)
			break;

	/*
	 * If no symbol cache structures are available allocate a new one and
	 * link it into the bucket list.
	 */
	if (scp == 0) {
		if ((scp = (Sym_cache *)libld_malloc(sizeof (Sym_cache) +
		    (sizeof (Sym_desc) * SYM_IDESCNO))) == 0)
			return ((Sym_desc *)S_ERROR);
		scp->sc_next = 0;
		/*LINTED*/
		scp->sc_free = sdp = (Sym_desc *)(scp + 1);
		scp->sc_end = (Sym_desc *)((unsigned long)scp->sc_free +
			(sizeof (Sym_desc) * SYM_IDESCNO));
		/*
		 * _scp is the parent symbol cache pointer, a null value
		 * indicates this is the first symbol cache to be added.
		 */
		if (_scp == 0)
			ofl->ofl_symbkt[bkt] = scp;
		else
			_scp->sc_next = scp;
	}

	/*
	 * Allocate an auxiliary descriptor.
	 */
	if ((sap = (Sym_aux *)libld_calloc(sizeof (Sym_aux), 1)) == 0)
		return ((Sym_desc *)S_ERROR);
	sdp->sd_file = ifl;
	sdp->sd_aux = sap;
	sap->sa_hash = hash;

	/*
	 * Copy the symbol table entry from the input file into the internal
	 * entry and have the symbol descriptor use it.
	 */
	sdp->sd_sym = nsym = &sap->sa_sym;
	*nsym = *osym;

	if ((_name = (char *)libld_malloc(strlen(name) + 1)) == 0)
		return ((Sym_desc *)S_ERROR);
	sdp->sd_name = (const char *)strcpy(_name, name);
	scp->sc_free++;

	/*
	 * Record the section index.  This is possible because the
	 * `ifl_isdesc' table is filled before we start symbol processing.
	 */
	if ((shndx == SHN_ABS) || (shndx == SHN_COMMON) || (shndx == SHN_UNDEF))
		sdp->sd_isc = NULL;
	else
		sdp->sd_isc = ifl->ifl_isdesc[shndx];

	/*
	 * Establish the symbols reference.
	 */
	if ((etype == ET_NONE) || (etype == ET_REL))
		sdp->sd_ref = REF_REL_NEED;
	else {
		sdp->sd_ref = REF_DYN_SEEN;
		/*
		 * Also record the binding file for this symbol in
		 * the sa_bindto field.  If this symbol is ever overriden
		 * by a REF_REL_NEED then we will use this sa_bindto
		 * when building a 'translator'
		 */
		if (shndx != SHN_UNDEF)
			sdp->sd_aux->sa_bindto = ifl;
	}

	/*
	 * If this is an undefined, or common symbol from a relocatable object
	 * determine whether it is a global or weak reference (see build_osym(),
	 * where REF_DYN_NEED definitions are returned back to undefines).
	 */
	if ((etype == ET_REL) &&
	    ((shndx == SHN_UNDEF) || (shndx == SHN_COMMON)) &&
	    (ELF_ST_BIND(nsym->st_info) == STB_GLOBAL))
		sdp->sd_flags = FLG_SY_GLOBREF;
	else
		sdp->sd_flags = 0;


	/*
	 * Record the input filename on the referenced or defined files list
	 * for possible later diagnostics.  The `sa_rfile' pointer contains the
	 * name of the file that first referenced this symbol and is used to
	 * generate undefined symbol diagnostics (refer to sym_undef_entry()).
	 * Note that this entry can be overridden if a reference from a
	 * relocatable object is found after a reference from a shared object
	 * (refer to sym_override()).
	 * The `sa_dfiles' list is used to maintain the list of files that
	 * define the same symbol.  This list can be used for two reasons:
	 *
	 *   o	To save the first definition of a symbol that is not available
	 *	for this link-edit.
	 *
	 *   o	To save all definitions of a symbol when the -m option is in
	 *	effect.  This is optional as it is used to list multiple
	 *	(interposed) definitions of a symbol (refer to ldmap_out()),
	 *	and can be quite expensive.
	 */
	if (shndx == SHN_UNDEF) {
		sap->sa_rfile = ifl->ifl_name;
	} else {
		if (sdp->sd_ref == REF_DYN_SEEN) {
			/*
			 * A symbol is determined to be unavailable if it
			 * belongs to a version of a shared object that this
			 * user does not wish to use, or if it belongs to an
			 * implicit shared object.
			 */
			if (ifl->ifl_vercnt) {
				Ver_index *	vip;
				Half		vndx = ifl->ifl_versym[ndx];

				sap->sa_verndx = vndx;
				vip = &ifl->ifl_verndx[vndx];
				if (!(vip->vi_flags & FLG_VER_AVAIL)) {
					sdp->sd_flags |= FLG_SY_NOTAVAIL;
					sap->sa_vfile = ifl->ifl_name;
				}
			}
			if (!(ifl->ifl_flags & FLG_IF_NEEDED))
				sdp->sd_flags |= FLG_SY_NOTAVAIL;
		} else if (etype == ET_REL) {
			/*
			 * If this symbol has been obtained from a versioned
			 * input relocatable object then the new symbol must be
			 * promoted to the versioning of the output file.
			 */
			if (ifl->ifl_versym)
				vers_promote(sdp, ndx, ifl, ofl);
		}

		if ((ofl->ofl_flags & FLG_OF_GENMAP) &&
		    (shndx != SHN_COMMON) && (shndx != SHN_ABS))
			if (list_appendc(&sap->sa_dfiles, ifl->ifl_name) == 0)
				return ((Sym_desc *)S_ERROR);
	}

	sdp->sd_FUNCndx = (Xword)-1;

	if (add_regsym(sdp, ofl) == S_ERROR)
		return ((Sym_desc *)S_ERROR);

	if (sdp->sd_file)
		ehdr = sdp->sd_file->ifl_ehdr;
	else
		ehdr = &def_ehdr;
	DBG_CALL(Dbg_syms_entered(ehdr, nsym, sdp));
	return (sdp);
}

/*
 * Add a special symbol to the symbol table.  Takes special symbol name with
 * and without underscores.  This routine is called, after all other symbol
 * resolution has completed, to generate a reserved absolute symbol (the
 * underscore version).  Special symbols are updated with the appropriate
 * values in sym_update().  If the user has already defined this symbol
 * issue a warning and leave the symbol as is.  If the non-underscore symbol
 * is referenced then turn it into a weak alias of the underscored symbol.
 */
uintptr_t
sym_add_spec(const char *name, const char *uname, Word sdaux_id,
    int flags, Ofl_desc *ofl)
{
	Sym_desc	*sdp;
	Sym_desc 	*usdp;
	Sym		*sym;
	Word		hash;

	/* LINTED */
	hash = (Word)elf_hash(uname);
	if (usdp = sym_find(uname, hash, ofl)) {
		/*
		 * If the underscore symbol exists and is undefined, or was
		 * defined in a shared library, convert it to a local symbol.
		 * Otherwise leave it as is and warn the user.
		 */
		if ((usdp->sd_sym->st_shndx == SHN_UNDEF) ||
		    (usdp->sd_ref != REF_REL_NEED)) {
			usdp->sd_ref = REF_REL_NEED;
			usdp->sd_sym->st_shndx = SHN_ABS;
			usdp->sd_sym->st_info =
			    ELF_ST_INFO(STB_GLOBAL, STT_OBJECT);
			usdp->sd_isc = NULL;
			usdp->sd_sym->st_size = 0;
			usdp->sd_sym->st_value = 0;
			/* LINTED */
			usdp->sd_aux->sa_symspec = (Half)sdaux_id;

			/*
			 * If a user hasn't specifically indicated the scope of
			 * this symbol be made local then leave it as global
			 * (ie. prevent automatic scoping).
			 */
			if (!(usdp->sd_flags & FLG_SY_LOCAL) &&
			    (flags & FLG_SY_GLOBAL))
				usdp->sd_aux->sa_verndx = VER_NDX_GLOBAL;
			usdp->sd_flags |= flags;
			DBG_CALL(Dbg_syms_updated((ofl->ofl_ehdr) ?
			    ofl->ofl_ehdr : &def_ehdr, usdp, uname));
		} else
			eprintf(ERR_WARNING, MSG_INTL(MSG_SYM_RESERVE), uname,
			    usdp->sd_file->ifl_name);
	} else {
		/*
		 * If the symbol does not exist create it.
		 */
		if ((sym = (Sym *)libld_calloc(sizeof (Sym), 1)) == 0)
			return (S_ERROR);
		sym->st_shndx = SHN_ABS;
		sym->st_info = ELF_ST_INFO(STB_GLOBAL, STT_OBJECT);
		sym->st_size = 0;
		sym->st_value = 0;
		DBG_CALL(Dbg_syms_created(uname));
		if ((usdp = sym_enter(uname, sym, hash, (Ifl_desc *)NULL,
		    ofl, 0)) == (Sym_desc *)S_ERROR)
			return (S_ERROR);
		usdp->sd_ref = REF_REL_NEED;
		/* LINTED */
		usdp->sd_aux->sa_symspec = (Half)sdaux_id;

		usdp->sd_flags |= flags;
		usdp->sd_aux->sa_verndx = VER_NDX_GLOBAL;

	}

	if (name && (sdp = sym_find(name, SYM_NOHASH, ofl)) &&
	    (sdp->sd_sym->st_shndx == SHN_UNDEF)) {
		unsigned char	bind;

		/*
		 * If the non-underscore symbol exists and is undefined
		 * convert it to be a local.  If the underscore has
		 * sa_symspec set (ie. it was created above) then simulate this
		 * as a weak alias.
		 */
		sdp->sd_ref = REF_REL_NEED;
		sdp->sd_sym->st_shndx = SHN_ABS;
		sdp->sd_isc = NULL;
		sdp->sd_sym->st_size = 0;
		sdp->sd_sym->st_value = 0;
		/* LINTED */
		sdp->sd_aux->sa_symspec = (Half)sdaux_id;
		if (usdp->sd_aux->sa_symspec) {
			usdp->sd_aux->sa_linkndx = 0;
			sdp->sd_aux->sa_linkndx = 0;
			bind = STB_WEAK;
		} else
			bind = STB_GLOBAL;
		sdp->sd_sym->st_info = ELF_ST_INFO(bind, STT_OBJECT);

		/*
		 * If a user hasn't specifically indicated the scope of
		 * this symbol be made local then leave it as global
		 * (ie. prevent automatic scoping).
		 */
		if (!(sdp->sd_flags & FLG_SY_LOCAL) &&
		    (flags & FLG_SY_GLOBAL))
			sdp->sd_aux->sa_verndx = VER_NDX_GLOBAL;
		sdp->sd_flags |= flags;
		DBG_CALL(Dbg_syms_updated((ofl->ofl_ehdr) ? ofl->ofl_ehdr :
		    &def_ehdr, sdp, name));
	}
	return (1);
}


/*
 * Print undefined symbols.
 */
static Boolean	undef_title = TRUE;

void
sym_undef_title()
{
	eprintf(ERR_NONE, MSG_INTL(MSG_SYM_FMT_UNDEF),
		MSG_INTL(MSG_SYM_UNDEF_ITM_11),
		MSG_INTL(MSG_SYM_UNDEF_ITM_21),
		MSG_INTL(MSG_SYM_UNDEF_ITM_12),
		MSG_INTL(MSG_SYM_UNDEF_ITM_22));

	undef_title = FALSE;
}

/*
 * Undefined symbols can fall into one of four types:
 *
 *  o	the symbol is really undefined (SHN_UNDEF).
 *
 *  o	versioning has been enabled, however this symbol has not been assigned
 *	to one of the defined versions.
 *
 *  o	the symbol has been defined by an implicitly supplied library, ie. one
 *	which was encounted because it was NEEDED by another library, rather
 * 	than from a command line supplied library which would become the only
 *	dependency of the output file being produced.
 *
 *  o	the symbol has been defined by a version of a shared object that is
 *	not permitted for this link-edit.
 *
 * In all cases the file who made the first reference to this symbol will have
 * been recorded via the `sa_rfile' pointer.
 */
typedef enum {
	UNDEF,		NOVERSION,	IMPLICIT,	NOTAVAIL
} Type;

static const Msg format[] = {
	MSG_SYM_UND_UNDEF,		/* MSG_INTL(MSG_SYM_UND_UNDEF) */
	MSG_SYM_UND_NOVER,		/* MSG_INTL(MSG_SYM_UND_NOVER) */
	MSG_SYM_UND_IMPL,		/* MSG_INTL(MSG_SYM_UND_IMPL) */
	MSG_SYM_UND_NOTA		/* MSG_INTL(MSG_SYM_UND_NOTA) */
};

void
sym_undef_entry(Sym_desc *sdp, Type type)
{
	const char	*name1, *name2, *name3;
	Ifl_desc	*ifl = sdp->sd_file;
	Sym_aux		*sap = sdp->sd_aux;

	if (undef_title)
		sym_undef_title();

	switch (type) {
	case UNDEF:
		name1 = sap->sa_rfile;
		break;
	case NOVERSION:
		name1 = ifl->ifl_name;
		break;
	case IMPLICIT:
		name1 = sap->sa_rfile;
		name2 = ifl->ifl_name;
		break;
	case NOTAVAIL:
		name1 = sap->sa_rfile;
		name2 = sap->sa_vfile;
		name3 = ifl->ifl_verndx[sap->sa_verndx].vi_name;
		break;
	default:
		return;
	}

	eprintf(ERR_NONE, MSG_INTL(format[type]), sdp->sd_name, name1, name2,
	    name3);
}

/*
 * At this point all symbol input processing has been completed, therefore
 * complete the symbol table entries by generating any necessary internal
 * symbols.
 */
uintptr_t
sym_spec(Ofl_desc *ofl)
{
	Word		flags = ofl->ofl_flags;

	if (!(flags & FLG_OF_RELOBJ)) {

		DBG_CALL(Dbg_syms_spec_title());

		if (sym_add_spec(MSG_ORIG(MSG_SYM_ETEXT),
		    MSG_ORIG(MSG_SYM_ETEXT_U), SDAUX_ID_ETEXT,
		    FLG_SY_GLOBAL, ofl) == S_ERROR)
			return (S_ERROR);
		if (sym_add_spec(MSG_ORIG(MSG_SYM_EDATA),
		    MSG_ORIG(MSG_SYM_EDATA_U), SDAUX_ID_EDATA,
		    FLG_SY_GLOBAL, ofl) == S_ERROR)
			return (S_ERROR);
		if (sym_add_spec(MSG_ORIG(MSG_SYM_END),
		    MSG_ORIG(MSG_SYM_END_U), SDAUX_ID_END,
		    FLG_SY_GLOBAL, ofl) == S_ERROR)
			return (S_ERROR);
		if (sym_add_spec(MSG_ORIG(MSG_SYM_L_END),
		    MSG_ORIG(MSG_SYM_L_END_U), SDAUX_ID_END,
		    FLG_SY_LOCAL, ofl) == S_ERROR)
			return (S_ERROR);
		if (sym_add_spec(MSG_ORIG(MSG_SYM_L_START),
		    MSG_ORIG(MSG_SYM_L_START_U), SDAUX_ID_START,
		    FLG_SY_LOCAL, ofl) == S_ERROR)
			return (S_ERROR);

		/*
		 * Historically we've always produced a _DYNAMIC symbol, even
		 * for static executables (in which case its value will be 0).
		 */
		if (sym_add_spec(MSG_ORIG(MSG_SYM_DYNAMIC),
		    MSG_ORIG(MSG_SYM_DYNAMIC_U), SDAUX_ID_DYN,
		    FLG_SY_GLOBAL, ofl) == S_ERROR)
			return (S_ERROR);

		if ((flags & (FLG_OF_DYNAMIC | FLG_OF_RELOBJ)) ==
		    FLG_OF_DYNAMIC)
			if (sym_add_spec(MSG_ORIG(MSG_SYM_PLKTBL),
			    MSG_ORIG(MSG_SYM_PLKTBL_U), SDAUX_ID_PLT,
			    FLG_SY_GLOBAL, ofl) == S_ERROR)
				return (S_ERROR);

		/*
		 * The _GLOBAL_OFFSET_TABLE_ is always created
		 * for a IA64 dynamic object.
		 */
		if (((ofl->ofl_e_machine == EM_IA_64) &&
		    (ofl->ofl_flags & FLG_OF_RELOBJ) == 0) ||
		    sym_find(MSG_ORIG(MSG_SYM_GOFTBL_U), SYM_NOHASH, ofl))
			if (sym_add_spec(MSG_ORIG(MSG_SYM_GOFTBL),
			    MSG_ORIG(MSG_SYM_GOFTBL_U), SDAUX_ID_GOT,
			    FLG_SY_GLOBAL, ofl) == S_ERROR)
				return (S_ERROR);
	}
	return (1);
}

/*
 * After all symbol table input processing has been finished, and all relocation
 * counting has been carried out (ie. no more symbols will be read, generated,
 * or modified), validate and count the relevant entries:
 *
 *	o	check and print any undefined symbols remaining.  Note that
 *		if a symbol has been defined by virtue of the inclusion of
 *		an implicit shared library, it is still classed as undefined.
 *
 * 	o	count the number of global needed symbols together with the
 *		size of their associated name strings (if scoping has been
 *		indicated these symbols may be reduced to locals).
 *
 *	o	establish the size and alignment requirements for the global
 *		.bss section (the alignment of this section is based on the
 *		first symbol that it will contain).
 */
uintptr_t
sym_validate(Ofl_desc *ofl)
{
	Word		bkt;
	Sym_desc	*sdp;
	Sym		*sym;
	Word		flags = ofl->ofl_flags;
	Word		undef = 0, needed = 0, verdesc = 0;
	Xword		align = 0;
	Xword		size = 0;
	Ifl_desc	*ifl;

	if ((flags & (FLG_OF_SHAROBJ | FLG_OF_SYMBOLIC)) ==
	    (FLG_OF_SHAROBJ | FLG_OF_SYMBOLIC))
		undef = FLG_OF_WARN;
	if (flags & FLG_OF_NOUNDEF)
		undef = FLG_OF_FATAL;
	if ((flags & FLG_OF_NOUNDEF) || !(flags & FLG_OF_SHAROBJ))
		needed = FLG_OF_FATAL;
	if ((flags & FLG_OF_VERDEF) && (ofl->ofl_vercnt > VER_NDX_GLOBAL))
		verdesc = FLG_OF_FATAL;


	/*
	 * Collect the globals from the internal symbol table.  Two validity
	 * checks are carried out:
	 *
	 *  o	If the symbol is undefined and this link-edit calls for no
	 *	undefined symbols to remain (this is the default case when
	 *	generating an executable but can be forced by using -z defs),
	 *	the symbol will be printed as undefined and a fatal error
	 * 	condition is indicated.
	 *	If the symbol is undefined and we're creating a shared object
	 *	with the -Bsymbolic flag set then the symbol will be printed as
	 *	undefined and a warning condition is indicated.
	 *
	 *  o	If the symbol is referenced from an implicitly included shared
	 *	library (ie. it's not on the NEEDED list) then the symbol is
	 *	also considered undefined and a fatal error condition exists.
	 */
	for (bkt = 0; bkt < ofl->ofl_symbktcnt; bkt++) {
		Sym_cache *	scp;

		for (scp = ofl->ofl_symbkt[bkt]; scp; scp = scp->sc_next) {
			/*LINTED*/
			for (sdp = (Sym_desc *)(scp + 1);
			    sdp < scp->sc_free; sdp++) {
				int	shndx, undeferr = 0;
				size_t	len;

				/*
				 * If undefined symbols are allowed we can
				 * ignore any symbols that are not needed.
				 */
				if (!(flags & FLG_OF_NOUNDEF) &&
				    (sdp->sd_ref == REF_DYN_SEEN))
					continue;

				sym = sdp->sd_sym;
				ifl = sdp->sd_file;
				shndx = sym->st_shndx;

				/*
				 * If scoping is enabled reduce any nonversioned
				 * global symbols (any symbol that has been
				 * processed for relocations will have already
				 * had this same reduction test applied).
				 * Indicate that the symbol has been reduced as
				 * it may be necessary to print these symbols
				 * later.
				 */
				if (((flags & FLG_OF_AUTOLCL) ||
				    (ofl->ofl_flags1 & FLG_OF1_AUTOELM)) &&
				    ((!(sdp->sd_flags & MSK_SY_DEFINED)) &&
				    (sdp->sd_ref == REF_REL_NEED) &&
				    (shndx != SHN_UNDEF))) {
					sdp->sd_flags |=
					    (FLG_SY_LOCAL | FLG_SY_REDUCED);
					if (ofl->ofl_flags1 & FLG_OF1_AUTOELM)
						sdp->sd_flags |=
						    FLG_SY_ELIM;
				}

				if ((sdp->sd_flags & FLG_SY_REDUCED) &&
				    (flags & FLG_OF_PROCRED)) {
					DBG_CALL(Dbg_syms_reduce((ofl->ofl_ehdr)
					    ? ofl->ofl_ehdr : &def_ehdr, sdp));
				}

				/*
				 * Test for undefined symbols.  A symbol is
				 * naturally determined to be undefined if its
				 * state remains UNDEF.  However, a symbol that
				 * is referenced by the user (either via a
				 * mapfile version reference or with -u) is
				 * also considered undefined if a definition is
				 * not found within the relocatable objects that
				 * make up the object being created - ie. if
				 * a definition is found in a shared object, the
				 * reference is still undefined.
				 */
				if (undef) {
					/*
					 * If an non-weak reference remains
					 * undefined, or if a mapfile reference
					 * is not bound to the relocatable
					 * objects that make up the object
					 * being built, we have a fatal error.
					 *
					 * The exceptions are symbols which are
					 * defined to be found in the parent
					 * (FLG_SY_PARENT), which is really only
					 * meaningful for direct binding, or are
					 * defined external (FLG_SY_EXTERN) so
					 * as to suppress -zdefs errors.
					 *
					 * Register symbols are always allowed
					 * to be UNDEF.
					 *
					 * Note that we don't include references
					 * created via -u in the same shared
					 * object binding test.  This is for
					 * backward compatability, in that a
					 * number of archive makefile rules used
					 * -u to cause archive extraction.
					 * These same rules have been cut and
					 * pasted to apply to shared objects,
					 * and thus although the -u reference
					 * is redundent, flagging it as fatal
					 * could cause some build to fail.
					 * Also we have documented the use of -u
					 * as a mechanism to cause binding to
					 * weak version definitions, thus giving
					 * users an error condition would be
					 * incorrect.
					 */
					if (!(sdp->sd_flags & FLG_SY_REGSYM) &&
					    ((shndx == SHN_UNDEF) &&
					    ((ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK) && ((sdp->sd_flags &
					    (FLG_SY_PARENT | FLG_SY_EXTERN)) ==
					    0))) ||
					    ((sdp->sd_flags & FLG_SY_MAPREF) &&
					    (sdp->sd_ref == REF_DYN_NEED))) {
						sym_undef_entry(sdp, UNDEF);
						ofl->ofl_flags |= undef;
						undeferr = 1;
					}
				} else {
					/*
					 * For building things like shared
					 * objects (or anything -znodefs),
					 * undefined symbols are allowed.
					 *
					 * If a mapfile reference remains
					 * undefined the user would probably
					 * like a warning at least (they've
					 * usually misspelt the reference).
					 * Refer to the above comments for
					 * discussion on -u references, which
					 * are not tested for in the same
					 * manner.
					 */
					if ((sdp->sd_flags & FLG_SY_MAPREF) &&
					    (((shndx == SHN_UNDEF) &&
					    (ELF_ST_BIND(sym->st_info) !=
					    STB_WEAK)) ||
					    (sdp->sd_ref == REF_DYN_NEED))) {
						sym_undef_entry(sdp, UNDEF);
						ofl->ofl_flags |= FLG_OF_WARN;
						undeferr = 1;
					}
				}

				/*
				 * If this symbol comes from a dependency mark
				 * the dependency as required (-z ignore can
				 * result in unused dependencies being dropped).
				 * If we need to record dependency versioning
				 * information indicate what version of the
				 * needed shared object this symbol is part of.
				 * Flag the symbol as undefined if it has not
				 * been made available to us.
				 */
				if ((sdp->sd_ref == REF_DYN_NEED) &&
				    (!(sdp->sd_flags & FLG_SY_REFRSD))) {
					sdp->sd_file->ifl_flags |=
					    FLG_IF_DEPREQD;

				    if (ifl->ifl_vercnt) {
					int		vndx;
					Ver_index *	vip;

					vndx = sdp->sd_aux->sa_verndx;
					vip = &sdp->sd_file->ifl_verndx[vndx];
					if (vip->vi_flags & FLG_VER_AVAIL) {
						vip->vi_flags |= FLG_VER_REFER;
					} else {
						sym_undef_entry(sdp, NOTAVAIL);
						ofl->ofl_flags |= FLG_OF_FATAL;
						continue;
					}
				    }
				}

				/*
				 * Test that we have no binding to symbols
				 * supplied from implicit shared objects.  If
				 * a binding is from a weak reference it can be
				 * ignored.
				 */
				if (needed && !undeferr &&
				    (sdp->sd_flags & FLG_SY_GLOBREF) &&
				    (sdp->sd_ref == REF_DYN_NEED) &&
				    (sdp->sd_flags & FLG_SY_NOTAVAIL)) {
					sym_undef_entry(sdp, IMPLICIT);
					ofl->ofl_flags |= needed;
					continue;
				}

				/*
				 * If the output image is to be versioned then
				 * all symbol definitions must be associated
				 * with a version.
				 */
				if (verdesc && (sdp->sd_ref == REF_REL_NEED) &&
				    (shndx != SHN_UNDEF) &&
				    (!(sdp->sd_flags & FLG_SY_LOCAL)) &&
				    (sdp->sd_aux->sa_verndx == 0)) {
					sym_undef_entry(sdp, NOVERSION);
					ofl->ofl_flags |= verdesc;
					continue;
				}

				/*
				 * If we don't need the symbol there's no need
				 * to process it any further.
				 */
				if (sdp->sd_ref == REF_DYN_SEEN)
					continue;

				/*
				 * Calculate the size and alignment requirements
				 * for the global .bss section.  If we're build-
				 * ing a relocatable object then only account
				 * for scoped COMMON symbols (these will be
				 * converted to bss references).
				 *
				 * For partially initialized symbol,
				 *  if it is expanded, it goes to sunwdata1.
				 *  if it is local, it goes to .bss.
				 *  if the output is shared object, it goes
				 *	to .sunwbss.
				 * Also refer to make_mvsections() in
				 *  sunwmove.c
				 */
				if ((shndx == SHN_COMMON) &&
				    ((!(flags & FLG_OF_RELOBJ)) ||
				    ((sdp->sd_flags & FLG_SY_LOCAL) &&
				    (flags & FLG_OF_PROCRED)))) {
					int countbss = 0;
					if (sdp->sd_psyminfo ==
					    (Psym_info *) NULL) {
						countbss = 1;
					} else if (
					    (sdp->sd_flags &
					    FLG_SY_PAREXPN) != 0) {
						countbss = 0;
					} else if (
					    ELF_ST_BIND(sym->st_info) ==
					    STB_LOCAL) {
						countbss = 1;
					} else if (
					    (ofl->ofl_flags &
					    FLG_OF_SHAROBJ) != 0) {
						countbss = 0;
					} else
						countbss = 1;
					if (countbss == 1) {
					    size = (Xword)S_ROUND(size,
						sym->st_value) +
						sym->st_size;
					    if (sym->st_value > align)
						align = sym->st_value;
					}
				}

				/*
				 * Update the symbol count and the associated
				 * name string size.  If scoping is in effect
				 * for this symbol assign it will be assigned
				 * to the .symtab/.strtab sections.
				 */
				len = strlen(sdp->sd_name) + 1;
				if ((sdp->sd_flags & FLG_SY_LOCAL) &&
				    (flags & FLG_OF_PROCRED)) {
					/*
					 * If local symbol reduction is active
					 * then these symbols will not be
					 * propogated to the output file.
					 */
					if (!((sdp->sd_flags & FLG_SY_ELIM) &&
					    (flags & FLG_OF_PROCRED))) {
						ofl->ofl_scopecnt++;
						ofl->ofl_locsstrsz +=
						    (Xword)len;
					}
				} else {
					ofl->ofl_globcnt++;
					ofl->ofl_globstrsz += (Xword)len;
				}
			}
		}
	}

	/*
	 * If we've encountered a fatal error during symbol validation then
	 * return now.
	 */
	if (ofl->ofl_flags & FLG_OF_FATAL)
		return (1);

	/*
	 * Generate the .bss section now that we know its size and alignment.
	 */
	if (size || !(flags & FLG_OF_RELOBJ)) {
		if (make_bss(ofl, size, align) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Determine what entry point symbol we need, and if found save its
	 * symbol descriptor so that we can update the ELF header entry with the
	 * symbols value later (see update_oehdr).  Make sure the symbol is
	 * tagged to insure its update in case -s is in effect.  Use any -e
	 * option first, or the default entry points `_start' and `main'.
	 */
	if (ofl->ofl_entry) {
		if (((sdp = sym_find(ofl->ofl_entry, SYM_NOHASH, ofl)) ==
		    NULL) || (sdp->sd_ref != REF_REL_NEED)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_SYM_ENTRY),
			    (char *)ofl->ofl_entry);
			return (S_ERROR);
		}
		ofl->ofl_entry = (void *)sdp;
		sdp->sd_flags |= FLG_SY_UPREQD;
	} else if (((sdp = sym_find(MSG_ORIG(MSG_SYM_START),
	    SYM_NOHASH, ofl)) != NULL) && (sdp->sd_ref == REF_REL_NEED)) {
		ofl->ofl_entry = (void *)sdp;
		sdp->sd_flags |= FLG_SY_UPREQD;
	} else if (((sdp = sym_find(MSG_ORIG(MSG_SYM_MAIN),
	    SYM_NOHASH, ofl)) != NULL) && (sdp->sd_ref == REF_REL_NEED)) {
		ofl->ofl_entry = (void *)sdp;
		sdp->sd_flags |= FLG_SY_UPREQD;
	}

	/*
	 * If we're required to record any needed dependencies versioning
	 * information calculate it now that all symbols have been validated.
	 */
	if (flags & FLG_OF_VERNEED)
		return (vers_check_need(ofl));
	else
		return (1);
}

/*
 * Process the symbol table for the specified input file.  At this point all
 * input sections from this input file have been assigned an input section
 * descriptor which is saved in the `ifl_isdesc' array.
 *
 *	o	local symbols are saved (as is) if the input file is a
 *		relocatable object
 *
 *	o	global symbols are added to the linkers internal symbol
 *		table if they are not already present, otherwise a symbol
 *		resolution function is called upon to resolve the conflict.
 */
uintptr_t
sym_process(Is_desc *isc, Ifl_desc *ifl, Ofl_desc *ofl)
{
	Sym		*sym = (Sym *)isc->is_indata->d_buf;
	Shdr		*shdr = isc->is_shdr;
	Sym_desc	*sdp;
	int		ndx, shndx;
	size_t		total, local;
	char		*str;
	unsigned char	type, bind;
	Word		hash;
	Half		etype = ifl->ifl_ehdr->e_type;

	/*
	 * Its possible that a file may contain more that one symbol table,
	 * ie. .dynsym and .symtab in a shared library.  Only process the first
	 * table (here, we assume .dynsym comes before .symtab).
	 */
	if (ifl->ifl_symscnt)
		return (1);

	DBG_CALL(Dbg_syms_process(ifl));

	/*
	 * From the symbol tables section header information determine which
	 * strtab table is needed to locate the actual symbol names.
	 */
	ndx = shdr->sh_link;
	str = ifl->ifl_isdesc[ndx]->is_indata->d_buf;

	/*
	 * Determine the number of local symbols together with the total
	 * number we have to process.
	 */
	total = shdr->sh_size / shdr->sh_entsize;
	local = shdr->sh_info;

	/*
	 * Allocate a symbol table index array and a local symbol array
	 * (global symbols are processed and added to the ofl->ofl_symbkt[]
	 * array).  If we are not dealing with a relocatable object there
	 * is no need to process any local symbols.
	 */
	if ((ifl->ifl_oldndx = libld_malloc((size_t)(total *
	    sizeof (Sym_desc *)))) == 0)
		return (S_ERROR);
	if ((etype == ET_REL) && local) {
		if ((ifl->ifl_locs = libld_calloc(sizeof (Sym_desc),
		    local)) == 0)
			return (S_ERROR);
		/* LINTED */
		ifl->ifl_locscnt = (Word)local;
	}
	/* LINTED */
	ifl->ifl_symscnt = (Word)total;

	/*
	 * Do any machine-specific processing on any locals
	 * found in a dynamic object.
	 */
	if (local && (etype == ET_DYN)) {
		if (mach_dyn_locals(sym, str, local, ifl, ofl) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * If there are local symbols to save add them to the symbol table
	 * index array.
	 */
	if (ifl->ifl_locscnt) {
		for (sym++, ndx = 1; ndx < local; sym++, ndx++) {
			sdp = &(ifl->ifl_locs[ndx]);
			sdp->sd_sym = sym;
			sdp->sd_name = str + sym->st_name;

			/*
			 * Determine the symbols section index.
			 */
			type = ELF_ST_TYPE(sym->st_info);
			shndx = sym->st_shndx;

			sdp->sd_ref = REF_REL_NEED;
			sdp->sd_flags |= FLG_SY_CLEAN;
			sdp->sd_file = ifl;
			sdp->sd_FUNCndx = (Xword)-1;
			ifl->ifl_oldndx[ndx] = sdp;
			DBG_CALL(Dbg_syms_entry(ndx, sdp));

			/*
			 * Assign an input section.
			 */
			if ((shndx != SHN_ABS) && (shndx != SHN_COMMON) &&
			    (shndx != SHN_UNDEF))
				sdp->sd_isc = ifl->ifl_isdesc[shndx];

			/*
			 * Skip any section symbols as new versions of these
			 * will be created.
			 */
			if (type == STT_SECTION) {
				if (shndx == SHN_UNDEF) {
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_SYM_INVSHNDX),
					    sdp->sd_name, ifl->ifl_name,
					    conv_shndx_str(shndx));
				}
				continue;
			}

			/*
			 * If this symbol falls within the range of
			 * a section being discarded, then discard the
			 * symbol itself.  There is no reason to keep
			 * a local symbol.
			 */
			if (sdp->sd_isc &&
			    (sdp->sd_isc->is_flags & FLG_IS_DISCARD)) {
				DBG_CALL(Dbg_syms_discarded(sdp, sdp->sd_isc));
				continue;
			}

			/*
			 * Carry our some basic sanity checks (these are just
			 * some of the erroneous symbol entries we've come
			 * across, there's probably a lot more).  The symbol
			 * will not be carried forward to the output file, which
			 * won't be a problem unless a relocation is required
			 * against it.
			 */
			if ((shndx == SHN_COMMON) ||
			    ((type == STT_FILE) && (shndx != SHN_ABS)) ||
			    (sdp->sd_isc && (sdp->sd_isc->is_osdesc == 0))) {
				eprintf(ERR_WARNING, MSG_INTL(MSG_SYM_INVSHNDX),
				    sdp->sd_name, ifl->ifl_name,
				    conv_shndx_str(shndx));
				sdp->sd_isc = NULL;
				sdp->sd_flags |= FLG_SY_INVALID;
				continue;
			}

			if (add_regsym(sdp, ofl) == S_ERROR)
				return (S_ERROR);

			/*
			 * As these local symbols will become part of the output
			 * image record their number and name string size.
			 * Globals are counted after all input file processing
			 * (and hence symbol resolution) is complete during
			 * sym_validate().
			 */
			if (!(ofl->ofl_flags1 & FLG_OF1_REDLSYM)) {
				ofl->ofl_locscnt++;
				if (sdp->sd_name)
					ofl->ofl_locsstrsz += (Xword)
						(strlen(sdp->sd_name) + 1);
			}
		}
	}

	/*
	 * Now scan the global symbols entering them in the internal symbol
	 * table or resolving them as necessary.
	 */
	sym = (Sym *)isc->is_indata->d_buf;
	sym += local;
	/* LINTED */
	for (ndx = (int)local; ndx < total; sym++, ndx++) {
		char	*name = (char *)str + sym->st_name;

		bind = ELF_ST_BIND(sym->st_info);
		if ((bind != STB_GLOBAL) && (bind != STB_WEAK)) {
			eprintf(ERR_WARNING, MSG_INTL(MSG_SYM_NONGLOB), name,
				ifl->ifl_name, conv_info_bind_str(bind));
			continue;
		}
		shndx = sym->st_shndx;

		/*
		 * The linker itself will generate symbols for _end, _etext,
		 * _edata, _DYNAMIC and _PROCEDURE_LINKAGE_TABLE_, so don't
		 * bother entering these symbols from shared objects (causes
		 * wasted resolution processing).
		 */
		if ((ELF_ST_TYPE(sym->st_info) == STT_OBJECT) &&
		    (sym->st_size == 0) && (etype == ET_DYN) &&
		    (name[0] == '_') && ((name[1] == 'e') ||
		    (name[1] == 'D') || (name[1] == 'P')) &&
		    ((strcmp(name, MSG_ORIG(MSG_SYM_ETEXT_U)) == 0) ||
		    (strcmp(name, MSG_ORIG(MSG_SYM_EDATA_U)) == 0) ||
		    (strcmp(name, MSG_ORIG(MSG_SYM_END_U)) == 0) ||
		    (strcmp(name, MSG_ORIG(MSG_SYM_DYNAMIC_U)) == 0) ||
		    (strcmp(name, MSG_ORIG(MSG_SYM_PLKTBL_U)) == 0))) {
			ifl->ifl_oldndx[ndx] = 0;
			continue;
		}

		/*
		 * If this symbol falls within the range of
		 * a section being discarded, then discard the
		 * symbol itself.
		 */
		if ((shndx != SHN_ABS) && (shndx != SHN_COMMON) &&
		    (shndx != SHN_UNDEF)) {
			Is_desc *	isp;
			isp = ifl->ifl_isdesc[shndx];
			if (isp &&
			    (isp->is_flags & FLG_IS_DISCARD)) {
				if ((sdp = libld_calloc(sizeof (Sym_desc), 1))
				    == 0)
					return (S_ERROR);
				/*
				 * We create a dummy symbol entry so
				 * that if we find any references to this
				 * discarded symbol we can compensate.
				 */
				sdp->sd_name = name;
				sdp->sd_sym = sym;
				sdp->sd_file = ifl;
				sdp->sd_isc = isp;
				sdp->sd_flags = FLG_SY_ISDISC;
				ifl->ifl_oldndx[ndx] = sdp;
				DBG_CALL(Dbg_syms_discarded(sdp, sdp->sd_isc));
				continue;
			}
		}

		/*
		 * If the symbol does not already exist in the internal symbol
		 * table add it, otherwise resolve the conflict.  If the symbol
		 * from this file is kept retain its symbol table index for
		 * possible use in associating a global alias.
		 */
		/* LINTED */
		hash = (Word)elf_hash((const char *)name);
		if ((sdp = sym_find(name, hash, ofl)) == NULL) {
			DBG_CALL(Dbg_syms_global(ndx, name));
			if ((sdp = sym_enter(name, sym, hash, ifl, ofl, ndx)) ==
			    (Sym_desc *)S_ERROR)
				return (S_ERROR);
		} else if (sym_resolve(sdp, sym, ifl, ofl, ndx) == S_ERROR)
			return (S_ERROR);

		/*
		 * After we've compared a defined symbol in one shared
		 * object, flag the symbol so we don't compare it again.
		 */
		if ((etype == ET_DYN) && (sym->st_shndx != SHN_UNDEF) &&
		    ((sdp->sd_flags & FLG_SY_SOFOUND) == 0))
			sdp->sd_flags |= FLG_SY_SOFOUND;

		/*
		 * If the symbol is accepted from this file retain the symbol
		 * index for possible use in aliasing.
		 */
		if (sdp->sd_file == ifl)
			sdp->sd_symndx = ndx;

		ifl->ifl_oldndx[ndx] = sdp;
	}

	/*
	 * If this is a shared object scan the globals one more time and
	 * associate any weak/global associations.  This association is needed
	 * should the weak definition satisfy a reference in the dynamic
	 * executable:
	 *
	 *  o	if the symbol is a data item it will be copied to the
	 *	executables address space, thus we must also reassociate the
	 *	alias symbol with its new location in the executable.
	 *
	 *  o	if the symbol is a function then we may need to promote	the
	 *	symbols binding from undefined weak to undefined, otherwise the
	 *	run-time linker will not generate the correct relocation error
	 *	should the symbol not be found.
	 *
	 * Typically, a weak/global symbol pair follows a convention of:
	 *
	 *	#pragma weak foo = _foo
	 *
	 * Therefore we first look for the symbol name proceeded with an `_'
	 * that exists in the same file.  If this fails we then look through
	 * all the global symbols.  This turns out to be faster than simply
	 * searching through the global symbols.
	 */
	if (ifl->ifl_ehdr->e_type != ET_DYN)
		return (1);

	sym = (Sym *)isc->is_indata->d_buf;
	sym += local;
	/* LINTED */
	for (ndx = (int)local; ndx < total; sym++, ndx++) {
		shndx = sym->st_shndx;

		if ((ELF_ST_BIND(sym->st_info) == STB_WEAK) &&
		    (shndx != SHN_UNDEF) && (shndx < SHN_LORESERVE)) {
			int		_ndx;
			char		_name[BUFSIZ];
			Sym_desc *	_sdp;
			Sym *		_sym;

			if (((sdp = ifl->ifl_oldndx[ndx]) == 0) ||
			    (sdp->sd_file != ifl))
				continue;

			_name[0] = '_';
			_name[BUFSIZ - 1] = '\0';
			(void) strncpy(&_name[1], sdp->sd_name, BUFSIZ - 2);
			if (_sdp = sym_find(_name, SYM_NOHASH, ofl)) {
				_sym = _sdp->sd_sym;

				if ((_sdp->sd_file == ifl) &&
				    (_sym->st_value == sym->st_value) &&
				    (_sym->st_size == sym->st_size) &&
				    (_sym->st_shndx != SHN_UNDEF) &&
				    (ELF_ST_BIND(_sym->st_info) != STB_WEAK) &&
				    (_sym->st_shndx < SHN_LORESERVE)) {
					_sdp->sd_aux->sa_linkndx = ndx;
					sdp->sd_aux->sa_linkndx =
					    (Word)_sdp->sd_symndx;
					continue;
				}
			}

			_sym = (Sym *)isc->is_indata->d_buf;
			_sym += local;
			/* LINTED */
			for (_ndx = (int)local; _ndx < total; _sym++, _ndx++) {
				if (_sym == sym)
					continue;

				if (((_sdp = ifl->ifl_oldndx[_ndx]) == 0) ||
				    (_sdp->sd_file != ifl))
					continue;

				if ((_sym->st_value == sym->st_value) &&
				    (_sym->st_size == sym->st_size) &&
				    (_sym->st_shndx != SHN_UNDEF) &&
				    (ELF_ST_BIND(_sym->st_info) != STB_WEAK) &&
				    (_sym->st_shndx < SHN_LORESERVE)) {
					_sdp->sd_aux->sa_linkndx = ndx;
					sdp->sd_aux->sa_linkndx = _ndx;
					break;
				}
			}
		}
	}
	return (1);
}

/*
 * Add an undefined symbol to the symbol table (ie. from -u name option)
 */
Sym_desc *
sym_add_u(const char *name, Ofl_desc *ofl)
{
	Sym		*sym;
	Ifl_desc	*ifl = 0, *_ifl;
	Sym_desc	*sdp;
	Word		hash;
	Listnode	*lnp;
	const char	*cmdline = MSG_INTL(MSG_STR_COMMAND);

	/*
	 * If the symbol reference already exists indicate that a reference
	 * also came from the command line.
	 */
	/* LINTED */
	hash = (Word)elf_hash(name);
	if (sdp = sym_find(name, hash, ofl))
		return (sdp);

	/*
	 * Determine whether a pseudo input file descriptor exists to represent
	 * the command line, as any global symbol needs an input file descriptor
	 * during any symbol resolution (refer to map_ifl() which provides a
	 * similar method for adding symbols from mapfiles).
	 */
	for (LIST_TRAVERSE(&ofl->ofl_objs, lnp, _ifl))
		if (strcmp(_ifl->ifl_name, cmdline) == 0) {
			ifl = _ifl;
			break;
		}

	/*
	 * If no descriptor exists create one.
	 */
	if (ifl == 0) {
		if ((ifl = (Ifl_desc *)libld_calloc(sizeof (Ifl_desc), 1)) ==
		    (Ifl_desc *)0)
			return ((Sym_desc *)S_ERROR);
		ifl->ifl_name = cmdline;
		if ((ifl->ifl_ehdr = (Ehdr *)libld_calloc(sizeof (Ehdr),
		    1)) == 0)
			return ((Sym_desc *)S_ERROR);
		ifl->ifl_ehdr->e_type = ET_REL;

		if (list_appendc(&ofl->ofl_objs, ifl) == 0)
			return ((Sym_desc *)S_ERROR);
	}

	/*
	 * Allocate a symbol structure and add it to the global symbol table.
	 */
	if ((sym = (Sym *)libld_calloc(sizeof (Sym), 1)) == 0)
		return ((Sym_desc *)S_ERROR);
	sym->st_info = ELF_ST_INFO(STB_GLOBAL, STT_NOTYPE);
	sym->st_shndx = SHN_UNDEF;

	DBG_CALL(Dbg_syms_process(ifl));
	DBG_CALL(Dbg_syms_global(0, name));
	sdp = sym_enter(name, sym, hash, ifl, ofl, 0);
	sdp->sd_flags &= ~FLG_SY_CLEAN;

	return (sdp);
}
