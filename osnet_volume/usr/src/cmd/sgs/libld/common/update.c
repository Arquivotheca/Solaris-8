/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 */

/*
 * Copyright (c) 1990-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)update.c	1.113	99/11/03 SMI"

/*
 * Update the new output file image, perform virtual address, offset and
 * displacement calculations on the program headers and sections headers,
 * and generate any new output section information.
 */
#include	<string.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"



/*
 * Define global pointers into the string table data areas.  Notice that these
 * are initialized in update_osym() and are referenced in update_odynamic() and
 * update_over[s](), this makes it important that the symbol tables are done
 * before these other sections can be completed.
 */
static char	*dynstr, *_dynstr;
static char	*strtab, *_strtab;

/*
 * Build and update any output symbol tables.  Here we work on all the symbol
 * tables at once to reduce the duplication of symbol and string manipulation.
 * Symbols and their associated strings are copied from the read-only input
 * file images to the output image and their values and index's updated in the
 * output image.
 */
Addr
update_osym(Ofl_desc * ofl, Half * symbssndx, Half * sunwbssndx)
{
	Listnode *	lnp1, * lnp2;
	Sym_desc *	sdp;
	Sg_desc *	sgp, * tsgp = 0, * dsgp = 0;
	Os_desc *	osp;
	Xword		bkt;
	Ifl_desc *	ifl;
	Half		bssndx, etext_ndx, edata_ndx = 0, end_ndx, start_ndx;
	Addr		bssaddr, etext = 0, edata = 0, end = 0, start = 0;
	Half		sunwdata1ndx;
	Addr		sunwdata1addr;
	Addr 		sunwbssaddr = 0;
	int		start_set = 0;
	Sym		_sym = {0};
	Sym		*sym, *symtab = 0, *scopetab, *dynsym = 0;
	char		*shdrstr, *_shdrstr;
	Word		symndx = 0;	/* Symbol index (for relocation use) */
	Word		scopendx = 0;	/* Symbol index for scoped symbols */
	Word *		hashtab;	/* hash table pointer */
	Word *		hashbkt;	/* hash table bucket pointer */
	Word *		hashchain;	/* hash table chain pointer */
	Word		hashval;	/* value of hash function */
	Word		hashndx;	/* index of first free chain slot */
	Wk_desc *	wkp;
	List		weak = {NULL, NULL};
	Word		flags = ofl->ofl_flags;
	Word		dtflags = ofl->ofl_dtflags;
	Versym *	versym;
	Gottable *	gottable;	/* used for display got debugging */
					/*	information */
	Gottable *	_gottable;
	Syminfo *	syminfo;

	/*
	 * Initialize pointers to the symbol table entries and the symbol
	 * table strings.  Skip the first symbol entry and the first string
	 * table byte.  Note that if we are not generating any output symbol
	 * tables we must still generate and update an internal copies so
	 * that the relocation phase has the correct information.
	 */
	if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ)) {
		symtab = (Sym *)ofl->ofl_ossymtab->os_outdata->d_buf;
		*symtab = _sym;
		symtab++;
		_strtab = ofl->ofl_osstrtab->os_outdata->d_buf;
		strtab = (char *)_strtab;
		*strtab = '\0';
		strtab++;
	}
	if (flags & FLG_OF_DYNAMIC) {
		_dynstr = ofl->ofl_osdynstr->os_outdata->d_buf;
		dynstr = (char *)_dynstr;
		*dynstr = '\0';
		dynstr++;

		if (!(flags & FLG_OF_RELOBJ)) {
			dynsym = (Sym *)ofl->ofl_osdynsym->os_outdata->d_buf;
			*dynsym = _sym;
			dynsym++;
			/*
			 * Initialize the hash table.
			 */
			hashtab = (Word *)(ofl->ofl_oshash->os_outdata->d_buf);
			hashbkt = &hashtab[2];
			hashchain = &hashtab[2 + ofl->ofl_hashbkts];
			hashtab[0] = ofl->ofl_hashbkts;
			hashtab[1] = ofl->ofl_shdrcnt + ofl->ofl_globcnt +
				ofl->ofl_lregsymcnt + 1;
			hashndx = ofl->ofl_shdrcnt + ofl->ofl_lregsymcnt + 1;
		}
	}

	/*
	 * If we have version definitions initialize the version symbol index
	 * table.  There is one entry for each symbol which contains the symbols
	 * version index.
	 */
	if ((flags & (FLG_OF_VERDEF | FLG_OF_NOVERSEC)) == FLG_OF_VERDEF) {
		versym = (Versym *)ofl->ofl_osversym->os_outdata->d_buf;
		versym[0] = 0;
	} else
		versym = 0;

	/*
	 * If we have a syminfo section be prepared to fill it in.
	 */
	if (ofl->ofl_ossyminfo) {
		syminfo = ofl->ofl_ossyminfo->os_outdata->d_buf;
		syminfo[0].si_flags = SYMINFO_CURRENT;
	} else
		syminfo = 0;

	/*
	 * Initialize pointers to the .shstrtab table.
	 */
	_shdrstr = (char *)ofl->ofl_osshstrtab->os_outdata->d_buf;
	shdrstr = (char *)_shdrstr;
	*shdrstr = '\0';
	shdrstr++;

	/*
	 * Provide each symbol an index for use by those that must reference
	 * the symbol (ie. relocations).  If we're building a dynamic executable
	 * or shared library, all references will be to the .dynsym table,
	 * otherwise references will be to the standard .symtab table.  The
	 * first symbol table entry is null, and the second entry of the .symtab
	 * will hold the file symbol.
	 */
	symndx = dynsym ? 1 : 2;

	DBG_CALL(Dbg_syms_sec_title());

	/*
	 * Add the output file name to the first .symtab symbol.
	 */
	if (symtab) {
		/* LINTED */
		symtab->st_name = (Word)(strtab - _strtab);
		symtab->st_value = 0;
		symtab->st_size = 0;
		symtab->st_info = ELF_ST_INFO(STB_LOCAL, STT_FILE);
		symtab->st_other = 0;
		symtab->st_shndx = SHN_ABS;

		(void) strcpy(strtab, ofl->ofl_name);
		strtab += strlen(ofl->ofl_name) + 1;
		symtab++;

		if (versym && !dynsym)
			versym[1] = 0;
	}

	/*
	 * If we are to display GOT summary information, then allocate
	 * the buffer to 'cache' the GOT symbols into now.
	 */
	if ((_gottable = gottable = libld_calloc(ofl->ofl_gotcnt,
	    sizeof (Gottable))) == 0)
		return ((Addr)S_ERROR);

	/*
	 * Traverse the program headers.  Determine the last executable segment
	 * and the last data segment so that we can update etext and edata.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Phdr *		phd = &(sgp->sg_phdr);

		if (phd->p_type == PT_LOAD) {
			if (sgp->sg_osdescs.head != NULL) {
				Word	_flags = phd->p_flags & (PF_W | PF_R);
				if (_flags == PF_R)
					tsgp = sgp;
				else if (_flags == (PF_W | PF_R))
					dsgp = sgp;
			}
		}

		/*
		 * Generate a section symbol for each output section.
		 */
		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			sym = &_sym;

			sym->st_value = osp->os_shdr->sh_addr;
			sym->st_info = ELF_ST_INFO(STB_LOCAL, STT_SECTION);
			/* LINTED */
			sym->st_shndx = (Half)elf_ndxscn(osp->os_scn);

			if (dynsym) {
				*dynsym = *sym;
				dynsym++;
			}
			if (symtab) {
				*symtab = *sym;
				symtab++;
			}
			if (versym)
				versym[symndx] = 0;

			DBG_CALL(Dbg_syms_sec_entry(symndx, sgp, osp));
			osp->os_scnsymndx = symndx;
			symndx++;

			/*
			 * Generate the .shstrtab for this section.
			 */
			/* LINTED */
			osp->os_shdr->sh_name = (Word)(shdrstr - _shdrstr);
			(void) strcpy(shdrstr, osp->os_name);
			shdrstr += strlen(osp->os_name) + 1;

			/*
			 * find the section index for our
			 * special symbols.
			 */
			if (sgp == tsgp)
				etext_ndx = (Half)osp->os_scnsymndx;
			else if (dsgp == sgp) {
				if (osp->os_shdr->sh_type != SHT_NOBITS)
					edata_ndx = (Half)osp->os_scnsymndx;
			}

			if (start_set == 0) {
				start = sgp->sg_phdr.p_vaddr;
				start_ndx = (Half)osp->os_scnsymndx;
				start_set++;
			}
		}
	}

	/*
	 * Add local REGISTER symbols to the .dynsym if required.
	 * These are required because the DT_REGISTER .dynamic
	 * entries must have a symbol to reference.
	 */
	if (ofl->ofl_regsymcnt && dynsym) {
		Listnode *	lnp;
		Sym_desc *	rsdp;
		for (LIST_TRAVERSE(&ofl->ofl_regsyms, lnp, rsdp)) {
			if ((rsdp->sd_flags & FLG_SY_LOCAL) ||
			    (ELF_ST_BIND(rsdp->sd_sym->st_info) ==
			    STB_LOCAL)) {
				*dynsym = *(rsdp->sd_sym);
				rsdp->sd_symndx = symndx++;
				if (*rsdp->sd_name != '\0') {
					dynsym->st_name = (Word)(dynstr -
					    _dynstr);
					(void) strcpy(dynstr,
					    rsdp->sd_name);
					dynstr += strlen(rsdp->sd_name) + 1;
				} else
					dynsym->st_name = 0;
				dynsym++;
			}
		}
	}

	/*
	 * Having traversed all the output segments, warn the user if the
	 * traditional text or data segments don't exist.  Otherwise from these
	 * segments establish the values for `etext', `edata', `end', `END',
	 * and `START'.
	 */
	if (!(flags & FLG_OF_RELOBJ)) {
		Sg_desc *	sgp;

		if (tsgp)
			etext = tsgp->sg_phdr.p_vaddr + tsgp->sg_phdr.p_filesz;
		else {
			etext = (Addr)0;
			etext_ndx = SHN_ABS;
			eprintf(ERR_WARNING, MSG_INTL(MSG_UPD_NOREADSEG));
		}
		if (dsgp) {
			edata = dsgp->sg_phdr.p_vaddr + dsgp->sg_phdr.p_filesz;
		} else {
			edata = (Addr)0;
			edata_ndx = SHN_ABS;
			eprintf(ERR_WARNING, MSG_INTL(MSG_UPD_NORDWRSEG));
		}
		if (dsgp == 0) {
			if (tsgp)
				sgp = tsgp;
			else
				sgp = 0;
		} else if (tsgp == 0)
			sgp = dsgp;
		else if (dsgp->sg_phdr.p_vaddr > tsgp->sg_phdr.p_vaddr)
			sgp = dsgp;
		else if (dsgp->sg_phdr.p_vaddr < tsgp->sg_phdr.p_vaddr)
			sgp = tsgp;
		else {
			/*
			 * One of the segments must be of zero size.
			 */
			if (tsgp->sg_phdr.p_memsz)
				sgp = tsgp;
			else
				sgp = dsgp;
		}

		if (sgp) {
			Os_desc *	tosp;
			end = sgp->sg_phdr.p_vaddr + sgp->sg_phdr.p_memsz;
			tosp = (Os_desc *)sgp->sg_osdescs.tail->data;
			end_ndx = (Half)tosp->os_scnsymndx;
		} else {
			end = (Addr) 0;
			end_ndx = SHN_ABS;
			eprintf(ERR_WARNING,  MSG_INTL(MSG_UPD_NOSEG));
		}
	}

	DBG_CALL(Dbg_syms_up_title(ofl->ofl_ehdr));

	/*
	 * Initialize the scoped symbol table entry point.  This is for all
	 * the global symbols that have been scoped to locals and will be
	 * filled in during global symbol processing so that we don't have
	 * to traverse the globals symbol hash array more than once.
	 */
	if (symtab) {
		scopetab = symtab;
		symtab += ofl->ofl_scopecnt;
		if (!dynsym) {
			scopendx = symndx;
			symndx += ofl->ofl_scopecnt;
		}
	}

	/*
	 * Assign .sunwdata1 information
	 */
	if (ofl->ofl_issunwdata1) {
		osp = ofl->ofl_issunwdata1->is_osdesc;
		sunwdata1addr = (Addr)(osp->os_shdr->sh_addr +
			ofl->ofl_issunwdata1->is_indata->d_off);
		/* LINTED */
		sunwdata1ndx = (Half) elf_ndxscn(osp->os_scn);
		ofl->ofl_sunwdata1ndx = osp->os_scnsymndx;
	}

	/*
	 * If we are generating a .symtab collect all the local symbols,
	 * assigning a new virtual address or displacement (value).
	 */
	for (LIST_TRAVERSE(&ofl->ofl_objs, lnp1, ifl)) {
		Xword		lndx;
		Xword		local;
		Is_desc *	isc;

		/*
		 * Check that we have local symbols to process.  If the user
		 * has indicated scoping then scan the global symbols also
		 * looking for entries from this file to reduce to locals.
		 */
		if ((local = ifl->ifl_locscnt) == 0)
			continue;

		for (lndx = 1; lndx < local; lndx++) {
			Listnode *	lnp2;
			Gotndx *	gnp;
			unsigned char	type;

			sdp = ifl->ifl_oldndx[lndx];
			sym = sdp->sd_sym;

#if defined(sparc) || defined(__sparcv9)
			/*
			 * Assign a got offset if necessary.
			 */
			if (assign_got(sdp) == S_ERROR)
				return ((Addr)S_ERROR);
#elif defined(i386) || defined(__ia64)
/* nothing to do */
#else
#error Unknown architecture!
#endif
			for (LIST_TRAVERSE(&sdp->sd_GOTndxs, lnp2, gnp)) {
				_gottable->gt_sym = sdp;
				_gottable->gt_gotndx = gnp->gn_gotndx;
				_gottable->gt_addend = gnp->gn_addend;
				_gottable++;
			}
			if ((int)(sdp->sd_FUNCndx) != -1) {
				_gottable->gt_sym = sdp;
				_gottable->gt_gotndx = (Word)sdp->sd_FUNCndx;
				_gottable++;
			}

			if ((type = ELF_ST_TYPE(sym->st_info)) == STT_SECTION)
				continue;

			/*
			 * Ignore any symbols that have been marked as invalid
			 * during input processing.  Providing these aren't used
			 * for relocation they'll just be dropped from the
			 * output image.
			 */
			if (sdp->sd_flags & FLG_SY_INVALID)
				continue;


			/*
			 * Generate an output symbol to represent this input
			 * symbol.  Even if the symbol table is to be stripped
			 * we still need to update any local symbols that are
			 * used during relocation.
			 */
			if (symtab && !(ofl->ofl_flags1 & FLG_OF1_REDLSYM)) {
				if (!dynsym)
					sdp->sd_symndx = symndx++;
				*symtab = *sym;
				/* LINTED */
				if (*sdp->sd_name != '\0') {
					symtab->st_name =
						(Word)(strtab - _strtab);
					(void) strcpy(strtab, sdp->sd_name);
					strtab += strlen(sdp->sd_name) + 1;
				} else
					symtab->st_name = 0;

				sdp->sd_flags &= ~FLG_SY_CLEAN;
				sdp->sd_sym = sym = symtab++;

				if (sym->st_shndx == SHN_ABS)
					continue;
			} else {
				/*
				 * If this symbol needs to used for relocation
				 * or updating move table, virtual address
				 * needs to be assigned.
				 */
				if (!(sdp->sd_flags & FLG_SY_UPREQD) &&
				    !(sdp->sd_psyminfo))
					continue;
				if (sym->st_shndx == SHN_ABS)
					continue;

				if (sym_copy(sdp) == S_ERROR)
					return ((Addr)S_ERROR);
				sym = sdp->sd_sym;
			}

			/*
			 * Update the symbols contents if necessary.
			 */
			if (type == STT_FILE) {
				sym->st_shndx = SHN_ABS;
				continue;
			}

			/*
			 * If we are expanding the locally bound partially
			 * initilized symbols, then update the address here.
			 */
			if (ofl->ofl_issunwdata1 &&
			    (sdp->sd_flags & FLG_SY_PAREXPN)) {
				sym->st_shndx = sunwdata1ndx;
				sdp->sd_isc = ofl->ofl_issunwdata1;
				if (ofl->ofl_flags & FLG_OF_RELOBJ)
					sym->st_value = sunwdata1addr;
				sunwdata1addr += sym->st_size;
			}

			/*
			 * If this isn't an UNDEF symbol (ie. an input section
			 * is associated), update the symbols value and index.
			 */
			if ((isc = sdp->sd_isc) != 0) {
				osp = isc->is_osdesc;

				sym->st_value += (Off)_elf_getxoff(
				    isc->is_indata);
				if (!(flags & FLG_OF_RELOBJ))
					sym->st_value += osp->os_shdr->sh_addr;
				/* LINTED */
				sym->st_shndx = (Half)elf_ndxscn(osp->os_scn);
			}
		}
	}

	/*
	 * Two special symbols are `_init' and `_fini'.  These are supplied
	 * by crti.o and are used to represent the total concatenation of the
	 * `.init' and `.fini' sections.  Determine the size of these sections
	 * and updated the symbols value accordingly.
	 */
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_INIT_U), SYM_NOHASH,
	    ofl)) != NULL) && (sdp->sd_ref == REF_REL_NEED) && sdp->sd_isc) {
		if (sym_copy(sdp) == S_ERROR)
			return ((Addr)S_ERROR);
		sdp->sd_sym->st_size =
			sdp->sd_isc->is_osdesc->os_shdr->sh_size;
	}
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_FINI_U), SYM_NOHASH,
	    ofl)) != NULL) && (sdp->sd_ref == REF_REL_NEED) && sdp->sd_isc) {
		if (sym_copy(sdp) == S_ERROR)
			return ((Addr)S_ERROR);
		sdp->sd_sym->st_size =
			sdp->sd_isc->is_osdesc->os_shdr->sh_size;
	}

	/*
	 * Assign .bss information for use with updating COMMON symbols.
	 */
	if (ofl->ofl_isbss) {
		osp = ofl->ofl_isbss->is_osdesc;

		bssaddr = osp->os_shdr->sh_addr +
			(Off)_elf_getxoff(ofl->ofl_isbss->is_indata);
		/* LINTED */
		*symbssndx = bssndx = (Half)elf_ndxscn(osp->os_scn);
	}

	/*
	 * Assign .SUNWbss information for use with updating COMMON symbols.
	 */
	if (ofl->ofl_issunwbss) {
		osp = ofl->ofl_issunwbss->is_osdesc;
		sunwbssaddr = (Addr)(osp->os_shdr->sh_addr +
			ofl->ofl_issunwbss->is_indata->d_off);
		/* LINTED */
		*sunwbssndx = (Half) elf_ndxscn(osp->os_scn);
	}

	/*
	 * Traverse the internal symbol table updating information and
	 * allocating common.
	 */
	for (bkt = 0; bkt < ofl->ofl_symbktcnt; bkt++) {
		Sym_cache *	scp;

		for (scp = ofl->ofl_symbkt[bkt]; scp; scp = scp->sc_next) {
			/*LINTED*/
			for (sdp = (Sym_desc *)(scp + 1);
			    sdp < scp->sc_free; sdp++) {
				const char	*name;
				Sym *		sym;
				Sym_aux *	sap;
				Half		spec;
				size_t		len;
				int		local = 0;
				int		enter_in_symtab;
				int		restore;
				Listnode *	lnp2;
				Gotndx *	gnp;

				if (symtab)
					enter_in_symtab = 1;
				else
					enter_in_symtab = 0;

				/*
				 * Assign a got offset if necessary.
				 */
#if defined(sparc) || defined(__sparcv9)
				if (assign_got(sdp) == S_ERROR)
					return ((Addr)S_ERROR);
#elif defined(i386) || defined(__ia64)
/* nothing to do */
#else
#error Unknown architecture!
#endif
				for (LIST_TRAVERSE(&sdp->sd_GOTndxs,
				    lnp2, gnp)) {
					_gottable->gt_sym = sdp;
					_gottable->gt_gotndx = gnp->gn_gotndx;
					_gottable->gt_addend = gnp->gn_addend;
					_gottable++;
				}

				if (sdp->sd_aux &&
				    sdp->sd_aux->sa_PLTGOTndx) {
					_gottable->gt_sym = sdp;
					_gottable->gt_gotndx =
						sdp->sd_aux->sa_PLTGOTndx;
					_gottable++;
				}

				/*
				 * Only needed symbols will be copied to the
				 * output symbol table.
				 */
				if (sdp->sd_ref == REF_DYN_SEEN)
					continue;

				/*
				 * If this symbol has been marked as being
				 * reduced to local scope then it will have to
				 * be placed in the scoped portion of the
				 * .symtab.  Retain the appropriate index for
				 * use in version symbol indexing and
				 * relocation.
				 */
				if ((sdp->sd_flags & FLG_SY_LOCAL) &&
				    (flags & FLG_OF_PROCRED)) {
					local = 1;
					if (!(sdp->sd_flags & FLG_SY_ELIM) &&
					    !dynsym)
						sdp->sd_symndx = scopendx++;
					else
						sdp->sd_symndx = 0;

					if (sdp->sd_flags & FLG_SY_ELIM)
						enter_in_symtab = 0;
				} else
					sdp->sd_symndx = symndx++;

				/*
				 * Copy basic symbol and string information.
				 */
				name = sdp->sd_name;
				len = strlen(name) + 1;
				sap = sdp->sd_aux;

				/*
				 * If we require to record version symbol
				 * indexes, update the associated version symbol
				 * information for all defined symbols.  If
				 * a version definition is required any zero
				 * value symbol indexes would have been flagged
				 * as undefined symbol errors, however if we're
				 * just scoping these need to fall into the base
				 * of global symbols.
				 */
				if (sdp->sd_symndx && versym) {
					Half	vndx = 0;

					if (sdp->sd_flags & FLG_SY_MVTOCOMM)
						vndx = VER_NDX_GLOBAL;
					else if (sdp->sd_ref == REF_REL_NEED) {
						Half f = (Half)sdp->sd_flags;
						vndx = sap->sa_verndx;

						if ((vndx == 0) &&
						    (sdp->sd_sym->st_shndx !=
						    SHN_UNDEF)) {
							if (f & FLG_SY_ELIM)
							    vndx =
							    VER_NDX_ELIMINATE;
							else if (f &
							    FLG_SY_LOCAL)
								vndx =
								VER_NDX_LOCAL;
							else
								vndx =
								VER_NDX_GLOBAL;
						}
					}
					versym[sdp->sd_symndx] = vndx;
				}

				/*
				 * If we are creating the .syminfo
				 * section then set per symbol flags
				 * here.
				 */
				if (sdp->sd_symndx && syminfo &&
				    !(sdp->sd_flags & FLG_SY_NOTAVAIL)) {
					int	ndx = sdp->sd_symndx;

					if (sdp->sd_flags & FLG_SY_PASSTHRU)
						syminfo[ndx].si_flags |=
							SYMINFO_FLG_PASSTHRU;
					if (sdp->sd_flags & FLG_SY_MVTOCOMM)
						syminfo[ndx].si_flags |=
							SYMINFO_FLG_COPY;
					if (sdp->sd_ref == REF_DYN_NEED) {
						syminfo[ndx].si_flags |=
						    SYMINFO_FLG_DIRECT;
						if (list_appendc(
						    &ofl->ofl_syminfsyms,
						    sdp) == 0)
							return (0);
						if (sdp->sd_flags &
						    FLG_SY_LAZYLD) {
							syminfo[ndx].si_flags |=
							SYMINFO_FLG_LAZYLOAD;
						}
					} else if (sdp->sd_flags &
					    FLG_SY_PARENT) {
						syminfo[ndx].si_flags |=
							SYMINFO_FLG_DIRECT;
						syminfo[ndx].si_boundto =
							SYMINFO_BT_PARENT;
					} else if ((sdp->sd_ref ==
					    REF_REL_NEED) &&
					    (sdp->sd_sym->st_shndx !=
					    SHN_UNDEF)) {
						syminfo[ndx].si_flags |=
							SYMINFO_FLG_DIRECT;
						syminfo[ndx].si_boundto =
							SYMINFO_BT_SELF;
					}
					if ((dtflags & DF_1_TRANS) &&
					    sdp->sd_aux->sa_bindto) {
						syminfo[ndx].si_flags |=
						    SYMINFO_FLG_DIRECT;
						if (list_appendc(
						    &ofl->ofl_syminfsyms,
						    sdp) == 0)
							return (0);
					}
				}

				/*
				 * Note that the `sym' value is reset to be one
				 * of the new symbol table entries.  This symbol
				 * will be updated further depending on the type
				 * of the symbol.  Process the .symtab first,
				 * followed by the .dynsym, thus the `sym' value
				 * will remain as the .dynsym value when the
				 * .dynsym is present.  This insures that any
				 * versioning symbols st_name value will be
				 * appropriate for the string table used to
				 * by version entries.
				 */
				if (enter_in_symtab) {
					/* LINTED */
					Word	_name = (Word)(strtab -
					    _strtab);

					if (local) {
						*scopetab = *sdp->sd_sym;
						sym = scopetab;
					} else {
						*symtab = *sdp->sd_sym;
						sym = symtab;
					}

					sdp->sd_sym = sym;
					if (*sdp->sd_name != '\0') {
						sym->st_name = _name;
						(void) strcpy(strtab, name);
						strtab += len;
					} else
						sym->st_name = 0;
				}
				if (dynsym && !local) {
					*dynsym = *sdp->sd_sym;
					if (*sdp->sd_name != '\0') {
						/* LINTED */
						dynsym->st_name = (Word)
						    (dynstr - _dynstr);
						(void) strcpy(dynstr, name);
						dynstr += len;
						hashval = sap->sa_hash %
							ofl->ofl_hashbkts;
						hashchain[hashndx] =
						    hashbkt[hashval];
						hashbkt[hashval] = hashndx++;
					} else {
						dynsym->st_name = 0;
						hashndx++;
					}

					sdp->sd_sym = sym = dynsym;
				}
				if (!enter_in_symtab && (!dynsym || local)) {
					if (!(sdp->sd_flags & FLG_SY_UPREQD))
						continue;
					sym = sdp->sd_sym;
				} else
					sdp->sd_flags &= ~FLG_SY_CLEAN;

				/*
				 * If we have a weak data symbol for which we
				 * need the real symbol also, save this
				 * processing until later.
				 *
				 * The exception to this is if the weak/strong
				 * have PLT's assigned to them.  In that case
				 * we don't do the post-weak processing because
				 * the PLT's must be maintained so that
				 * we can do 'interpositioning' on both of
				 * the symbols.
				 */
				if ((sap->sa_linkndx) &&
				    (ELF_ST_BIND(sym->st_info) == STB_WEAK) &&
				    (!sap->sa_PLTndx)) {
					Sym_desc *	_sdp =
					    sdp->sd_file->ifl_oldndx
						[sap->sa_linkndx];

					if (_sdp->sd_ref != REF_DYN_SEEN) {
						if ((wkp = libld_calloc(
						    sizeof (Wk_desc), 1)) == 0)
							return ((Addr)S_ERROR);

						if (enter_in_symtab)
							if (local)
								wkp->wk_symtab =
								    scopetab;
							else
								wkp->wk_symtab =
								    symtab;
						if (dynsym && !local)
							wkp->wk_dynsym = dynsym;
						wkp->wk_weak = sdp;
						wkp->wk_alias = _sdp;

						if (!(list_appendc(&weak, wkp)))
							return ((Addr)S_ERROR);

						if (enter_in_symtab)
							if (local)
								scopetab++;
							else
								symtab++;
						if (dynsym && !local)
							dynsym++;
						continue;
					}
				}

				DBG_CALL(Dbg_syms_old(ofl->ofl_ehdr, sdp));

				spec = NULL;
				/*
				 * assign new symbol value.
				 */
				switch (sym->st_shndx) {
				case SHN_UNDEF:
					if (((sdp->sd_flags &
					    FLG_SY_REGSYM) == 0) &&
					    (sym->st_value != 0)) {
						eprintf(ERR_WARNING,
						    MSG_INTL(MSG_SYM_NOTNULL),
						    name,
						    sdp->sd_file->ifl_name);
					}

					/*
					 * Undefined weak global, if we are
					 * generating a static executable,
					 * output as an absolute zero.
					 * Otherwise leave it as is, ld.so.1
					 * will skip symbols of this type (this
					 * technique allows applications and
					 * libraries to test for the existence
					 * of a symbol as an indication of the
					 * presence or absence of certain
					 * functionality).
					 */
					if (((flags &
					    (FLG_OF_STATIC | FLG_OF_EXEC)) ==
					    (FLG_OF_STATIC | FLG_OF_EXEC)) &&
					    (ELF_ST_BIND(sym->st_info) ==
					    STB_WEAK))
						sym->st_shndx = SHN_ABS;
					break;

				case SHN_COMMON:
					/*
					 * If this this is an expanded symbol,
					 * 	it goes to sunwdata1.
					 *
					 * If this is a partial initialized
					 * global symbol and the output is a
					 * shared object,
					 *	it goes to sunwbss.
					 *
					 * If allocating common assign it an
					 * address in the .bss section,
					 *
					 * otherwise leave it as is.
					 */
					restore = 0;
					if (sdp->sd_flags & FLG_SY_PAREXPN) {
						restore = 1;
						sym->st_shndx =
							sunwdata1ndx;
						sym->st_value = (Xword)
							S_ROUND(sunwdata1addr,
							sym->st_value);
						sunwdata1addr =
							sym->st_value +
							sym->st_size;
						sdp->sd_isc =
							ofl->ofl_issunwdata1;
					} else if (
					    (sdp->sd_psyminfo !=
					    (Psym_info *)NULL) &&
					    (ofl->ofl_flags & FLG_OF_SHAROBJ) &&
					    (ELF_ST_BIND(sym->st_info) !=
					    STB_LOCAL)) {
						restore = 1;
						sym->st_shndx =
							*sunwbssndx;
						sym->st_value = (Xword)
							S_ROUND(sunwbssaddr,
							sym->st_value);
						sunwbssaddr =
							sym->st_value +
							sym->st_size;
						sdp->sd_isc =
							ofl->ofl_issunwbss;
					} else if (local ||
					    !(flags & FLG_OF_RELOBJ)) {
						restore = 1;
						sym->st_shndx = bssndx;
						sym->st_value = (Xword)
							S_ROUND(bssaddr,
							sym->st_value);
						bssaddr =
							sym->st_value +
							sym->st_size;
						sdp->sd_isc =
							ofl->ofl_isbss;
					}

					if (restore != 0) {
						unsigned char type, bind;
						/*
						 * Make sure this COMMON
						 * symbol is returned to the
						 * same binding as was defined
						 * in the original relocatable
						 * object reference.
						 */
						type = ELF_ST_TYPE(sym->
						    st_info);
						if (sdp->sd_flags &
						    FLG_SY_GLOBREF)
							bind = STB_GLOBAL;
						else
							bind = STB_WEAK;

						sym->st_info =
						    ELF_ST_INFO(bind, type);
					}
					break;

				case SHN_ABS:
					spec = sdp->sd_aux->sa_symspec;
					/* FALLTHROUGH */

				default:
					if (sdp->sd_ref == REF_DYN_NEED) {
						unsigned char	type, bind;

						sym->st_shndx = SHN_UNDEF;
						sym->st_value = 0;
						sym->st_size = 0;

						/*
						 * Make sure this undefined
						 * symbol is returned to the
						 * same binding as was defined
						 * in the original relocatable
						 * object reference.
						 */
						type = ELF_ST_TYPE(sym->
						    st_info);
						if (sdp->sd_flags &
						    FLG_SY_GLOBREF)
							bind = STB_GLOBAL;
						else
							bind = STB_WEAK;

						sym->st_info =
						    ELF_ST_INFO(bind, type);

					} else if ((sym->st_shndx != SHN_ABS) &&
					    (sdp->sd_ref == REF_REL_NEED)) {

						osp = sdp->sd_isc->is_osdesc;
						/* LINTED */
						sym->st_shndx = (Half)
						    elf_ndxscn(osp->os_scn);

						/*
						 * In an executable, the new
						 * symbol value is the old value
						 * (offset into defining
						 * section) plus virtual address
						 * of defining section.  In a
						 * relocatable, the new value is
						 * the old value plus the
						 * displacement of the section
						 * within the file.
						 */
						sym->st_value += (Off)
						    _elf_getxoff(sdp->
							sd_isc->is_indata);

						if (!(flags & FLG_OF_RELOBJ))
							sym->st_value += osp->
							    os_shdr->sh_addr;
					}
					break;
				}

				if (spec) {
					switch (spec) {
					case SDAUX_ID_ETEXT:
						sym->st_value = etext;
						sym->st_shndx = etext_ndx;
						break;
					case SDAUX_ID_EDATA:
						sym->st_value = edata;
						sym->st_shndx = edata_ndx;
						break;
					case SDAUX_ID_END:
						sym->st_value = end;
						sym->st_shndx = end_ndx;
						break;
					case SDAUX_ID_START:
						sym->st_value = start;
						sym->st_shndx = start_ndx;
						break;
					case SDAUX_ID_DYN:
						if (flags & FLG_OF_DYNAMIC) {
							sym->st_value = ofl->
							    ofl_osdynamic->
							    os_shdr->sh_addr;
							sym->st_shndx = (Half)
							    ofl->ofl_osdynamic->
							    os_scnsymndx;
						}
						break;
					case SDAUX_ID_PLT:
						if (ofl->ofl_osplt) {
							sym->st_value = ofl->
							    ofl_osplt->os_shdr->
							    sh_addr;
							sym->st_shndx = (Half)
							    ofl->ofl_osplt->
							    os_scnsymndx;
						}
						break;
					case SDAUX_ID_GOT:
						/*
						 * Symbol bias for negative
						 * growing tables is stored in
						 * symbol's value during
						 * allocate_got().
						 */
						sym->st_value += ofl->
						    ofl_osgot->os_shdr->sh_addr;
						sym->st_shndx = (Half)ofl->
						    ofl_osgot->os_scnsymndx;
						break;
					default:
						/* NOTHING */
						;
					}
				}

				/*
				 * If a plt index has been assigned to an
				 * undefined function, update the symbols value
				 * to the appropriate .plt address.
				 */
				if ((flags & FLG_OF_DYNAMIC) &&
				    (flags & FLG_OF_EXEC) &&
				    (sdp->sd_file) &&
				    (sdp->sd_file->ifl_ehdr->e_type ==
				    ET_DYN) &&
				    (ELF_ST_TYPE(sym->st_info) == STT_FUNC) &&
				    !(flags & FLG_OF_BFLAG)) {
					if (sap->sa_PLTndx)
						sym->st_value =
						    ofl->ofl_osplt->
						    os_shdr->sh_addr +
						    M_PLT_RESERVSZ +
						    (sap->sa_PLTndx - 1) *
						    M_PLT_ENTSIZE;
				}

				/*
				 * Finish updating the symbols.  If both the
				 * .symtab and .dynsym are present then we've
				 * actually updated the information in the
				 * .dynsym, therefore copy this same
				 * information to the .symtab entry.
				 */
				if (local)
					sym->st_info = ELF_ST_INFO(STB_LOCAL,
					    ELF_ST_TYPE(sym->st_info));

				if (enter_in_symtab && dynsym && !local) {
					symtab->st_value = sym->st_value;
					symtab->st_size = sym->st_size;
					symtab->st_info = sym->st_info;
					symtab->st_other = sym->st_other;
					symtab->st_shndx = sym->st_shndx;
				}
				if (enter_in_symtab)
					if (local)
						scopetab++;
					else
						symtab++;
				if (dynsym && !local)
					dynsym++;

				DBG_CALL(Dbg_syms_new(ofl->ofl_ehdr, sym, sdp));
			}
		}
	}

	/*
	 * Now that all the symbols have been processed update any weak symbols
	 * information (ie. copy all information except `st_name').  As both
	 * symbols will be represented in the output, return the weak symbol to
	 * its correct type.
	 */
	for (LIST_TRAVERSE(&weak, lnp1, wkp)) {
		Sym_desc *	sdp, * _sdp;
		Sym *		sym, * _sym, * __sym;
		unsigned char	bind;

		sdp = wkp->wk_weak;
		_sdp = wkp->wk_alias;
		_sym = _sdp->sd_sym;

		sdp->sd_flags |= FLG_SY_WEAKDEF;

		/*
		 * If the symbol definition has been scoped then assign it to
		 * be local, otherwise if it's from a shared object then we need
		 * to maintain the binding of the original reference.
		 */
		if (sdp->sd_flags & FLG_SY_LOCAL) {
			if (flags & FLG_OF_PROCRED)
				bind = STB_LOCAL;
			else
				bind = STB_WEAK;
		} else if ((sdp->sd_ref == REF_DYN_NEED) &&
		    (sdp->sd_flags & FLG_SY_GLOBREF))
			bind = STB_GLOBAL;
		else
			bind = STB_WEAK;

		DBG_CALL(Dbg_syms_old(ofl->ofl_ehdr, sdp));
		if ((sym = wkp->wk_symtab) != 0) {
			sym = wkp->wk_symtab;
			sym->st_value = _sym->st_value;
			sym->st_size = _sym->st_size;
			sym->st_other = _sym->st_other;
			sym->st_shndx = _sym->st_shndx;
			sym->st_info = ELF_ST_INFO(bind,
			    ELF_ST_TYPE(sym->st_info));
			__sym = sym;
		}
		if ((sym = wkp->wk_dynsym) != 0) {
			sym = wkp->wk_dynsym;
			sym->st_value = _sym->st_value;
			sym->st_size = _sym->st_size;
			sym->st_other = _sym->st_other;
			sym->st_shndx = _sym->st_shndx;
			sym->st_info = ELF_ST_INFO(bind,
			    ELF_ST_TYPE(sym->st_info));
			__sym = sym;
		}
		DBG_CALL(Dbg_syms_new(ofl->ofl_ehdr, __sym, sdp));
	}

	/*
	 * Now display GOT debugging information if required
	 */
	if (gottable)
		DBG_CALL(Dbg_got_display(gottable, ofl));

	/*
	 * Update the section headers information.
	 */
	if (symtab) {
		Shdr *	shdr = ofl->ofl_ossymtab->os_shdr;

		shdr->sh_info = ofl->ofl_shdrcnt + ofl->ofl_locscnt +
			ofl->ofl_scopecnt + 2;
		/* LINTED */
		shdr->sh_link = (Word)elf_ndxscn(ofl->ofl_osstrtab->os_scn);
	}
	if (dynsym) {
		Shdr *	shdr = ofl->ofl_osdynsym->os_shdr;

		shdr->sh_info = ofl->ofl_shdrcnt + ofl->ofl_lregsymcnt + 1;
		/* LINTED */
		shdr->sh_link = (Word)elf_ndxscn(ofl->ofl_osdynstr->os_scn);

		ofl->ofl_oshash->os_shdr->sh_link =
		    /* LINTED */
		    (Word)elf_ndxscn(ofl->ofl_osdynsym->os_scn);
	}

	/*
	 * Used by ld.so.1 only.
	 */
	return (etext);
}

/*
 * Build the dynamic section.
 */
int
update_odynamic(Ofl_desc * ofl)
{
	Listnode *	lnp;
	Ifl_desc *	ifl;
	Sym_desc *	sdp;
	Shdr *		shdr;
	Dyn *		_dyn = (Dyn *)ofl->ofl_osdynamic->os_outdata->d_buf;
	Dyn *		dyn;
	char		*_dynstr = ofl->ofl_osdynstr->os_outdata->d_buf;
	Word		flags = ofl->ofl_flags;

	ofl->ofl_osdynamic->os_shdr->sh_link =
	    /* LINTED */
	    (Word)elf_ndxscn(ofl->ofl_osdynstr->os_scn);

	dyn = _dyn;

	for (LIST_TRAVERSE(&ofl->ofl_sos, lnp, ifl)) {
		if ((ifl->ifl_flags &
		    (FLG_IF_IGNORE | FLG_IF_DEPREQD)) == FLG_IF_IGNORE)
			continue;

		/*
		 * Create and set up the DT_POSFLAG_1 entry here if required.
		 */
		if ((ifl->ifl_flags & (FLG_IF_LAZYLD|FLG_IF_GRPPRM)) &&
		    (ifl->ifl_flags & (FLG_IF_NEEDED))) {
			dyn->d_tag = DT_POSFLAG_1;
			if (ifl->ifl_flags & FLG_IF_LAZYLD)
				dyn->d_un.d_val = DF_P1_LAZYLOAD;
			if (ifl->ifl_flags & FLG_IF_GRPPRM)
				dyn->d_un.d_val |= DF_P1_GROUPPERM;
			dyn++;
		}

		if (ifl->ifl_flags & (FLG_IF_NEEDED | FLG_IF_NEEDSTR))
			dyn->d_tag = DT_NEEDED;
		else
			continue;

		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ifl->ifl_soname);
		dynstr += strlen(ifl->ifl_soname) + 1;
		/* LINTED */
		ifl->ifl_neededndx = (Half)(((uintptr_t)dyn - (uintptr_t)_dyn) /
		    sizeof (Dyn));
		dyn++;
	}
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_INIT_U),
	    SYM_NOHASH, ofl)) != NULL) &&
		sdp->sd_ref == REF_REL_NEED) {
		dyn->d_tag = DT_INIT;
		dyn->d_un.d_ptr = sdp->sd_sym->st_value;
		dyn++;
	}
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_FINI_U),
	    SYM_NOHASH, ofl)) != NULL) &&
		sdp->sd_ref == REF_REL_NEED) {
		dyn->d_tag = DT_FINI;
		dyn->d_un.d_ptr = sdp->sd_sym->st_value;
		dyn++;
	}
	if (ofl->ofl_soname) {
		dyn->d_tag = DT_SONAME;
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_soname);
		dynstr += strlen(ofl->ofl_soname) + 1;
		dyn++;
	}
	if (ofl->ofl_filtees) {
		if (flags & FLG_OF_AUX) {
			dyn->d_tag = DT_AUXILIARY;
		} else {
			dyn->d_tag = DT_FILTER;
		}
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_filtees);
		dynstr += strlen(ofl->ofl_filtees) + 1;
		dyn++;
	}
	if (ofl->ofl_rpath) {
		dyn->d_tag = DT_RPATH;
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_rpath);
		dynstr += strlen(ofl->ofl_rpath) + 1;
		dyn++;
	}
	if (ofl->ofl_config) {
		dyn->d_tag = DT_CONFIG;
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_config);
		dynstr += strlen(ofl->ofl_config) + 1;
		dyn++;
	}
	if (ofl->ofl_depaudit) {
		dyn->d_tag = DT_DEPAUDIT;
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_depaudit);
		dynstr += strlen(ofl->ofl_depaudit) + 1;
		dyn++;
	}
	if (ofl->ofl_audit) {
		dyn->d_tag = DT_AUDIT;
		dyn->d_un.d_val = (Xword)(dynstr - _dynstr);
		(void) strcpy(dynstr, ofl->ofl_audit);
		dynstr += strlen(ofl->ofl_audit) + 1;
		dyn++;
	}

	/*
	 * The following DT_* entries do not apply to relocatable objects.
	 */
	if (!(flags & FLG_OF_RELOBJ)) {

		dyn->d_tag = DT_HASH;
		dyn->d_un.d_ptr = ofl->ofl_oshash->os_shdr->sh_addr;
		dyn++;

		shdr = ofl->ofl_osdynstr->os_shdr;
		dyn->d_tag = DT_STRTAB;
		dyn->d_un.d_ptr = shdr->sh_addr;
		dyn++;

		dyn->d_tag = DT_STRSZ;
		dyn->d_un.d_ptr = shdr->sh_size;
		dyn++;

		shdr = ofl->ofl_osdynsym->os_shdr;
		dyn->d_tag = DT_SYMTAB;
		dyn->d_un.d_ptr = shdr->sh_addr;
		dyn++;

		dyn->d_tag = DT_SYMENT;
		dyn->d_un.d_ptr = shdr->sh_entsize;
		dyn++;

		/*
		 * Reserve the DT_CHECKSUM entry.  Its value will be filled in
		 * after the complete image is built.
		 */
		dyn->d_tag = DT_CHECKSUM;
		ofl->ofl_checksum = &dyn->d_un.d_val;
		dyn++;

		if ((flags & (FLG_OF_VERDEF | FLG_OF_NOVERSEC)) ==
		    FLG_OF_VERDEF) {
			shdr = ofl->ofl_osverdef->os_shdr;
			dyn->d_tag = DT_VERDEF;
			dyn->d_un.d_ptr = shdr->sh_addr;
			dyn++;
			dyn->d_tag = DT_VERDEFNUM;
			dyn->d_un.d_ptr = shdr->sh_info;
			dyn++;
		}
		if ((flags & (FLG_OF_VERNEED | FLG_OF_NOVERSEC)) ==
		    FLG_OF_VERNEED) {
			shdr = ofl->ofl_osverneed->os_shdr;
			dyn->d_tag = DT_VERNEED;
			dyn->d_un.d_ptr = shdr->sh_addr;
			dyn++;
			dyn->d_tag = DT_VERNEEDNUM;
			dyn->d_un.d_ptr = shdr->sh_info;
			dyn++;
		}
		if ((ofl->ofl_flags1 & FLG_OF1_RELCNT) &&
		    ofl->ofl_relocrelcnt) {
			dyn->d_tag = M_REL_DT_COUNT;
			dyn->d_un.d_val = ofl->ofl_relocrelcnt;
			dyn++;
		}
		if (flags & FLG_OF_TEXTREL) {
			/*
			 * Only the presence of this entry is used in this
			 * implementation, not the value stored.
			 */
			dyn->d_tag = DT_TEXTREL;
			dyn->d_un.d_val = 0;
			dyn++;
		}
		if (ofl->ofl_pltcnt) {
			shdr =  ofl->ofl_osplt->os_relosdesc->os_shdr;

			dyn->d_tag = DT_PLTRELSZ;
			dyn->d_un.d_ptr = shdr->sh_size;
			dyn++;
			dyn->d_tag = DT_PLTREL;
			dyn->d_un.d_ptr = M_REL_DT_TYPE;
			dyn++;
			dyn->d_tag = DT_JMPREL;
			dyn->d_un.d_ptr = shdr->sh_addr;
			dyn++;
		}
		if (ofl->ofl_pltpad) {
			shdr =  ofl->ofl_osplt->os_shdr;

			dyn->d_tag = DT_PLTPAD;
			if (ofl->ofl_pltcnt)
				dyn->d_un.d_ptr = shdr->sh_addr +
					M_PLT_RESERVSZ +
					ofl->ofl_pltcnt * M_PLT_ENTSIZE;
			else
				dyn->d_un.d_ptr = shdr->sh_addr;
			dyn++;
			dyn->d_tag = DT_PLTPADSZ;
			dyn->d_un.d_val = ofl->ofl_pltpad *
				M_PLT_ENTSIZE;
			dyn++;
		}
		if (ofl->ofl_relocsz) {
			dyn->d_tag = M_REL_DT_TYPE;
			dyn->d_un.d_ptr = ofl->ofl_osrelhead->os_shdr->sh_addr;
			dyn++;
			dyn->d_tag = M_REL_DT_SIZE;
			dyn->d_un.d_ptr = ofl->ofl_relocsz;
			dyn++;
			dyn->d_tag = M_REL_DT_ENT;
			if (ofl->ofl_osrelhead->os_shdr->sh_type == SHT_REL)
				dyn->d_un.d_ptr = sizeof (Rel);
			else
				dyn->d_un.d_ptr = sizeof (Rela);
			dyn++;
		}
		if (ofl->ofl_ossyminfo) {
			shdr = ofl->ofl_ossyminfo->os_shdr;
			dyn->d_tag = DT_SYMINFO;
			dyn->d_un.d_ptr = shdr->sh_addr;
			dyn++;
			dyn->d_tag = DT_SYMINSZ;
			dyn->d_un.d_val = shdr->sh_size;
			dyn++;
			dyn->d_tag = DT_SYMINENT;
			dyn->d_un.d_val = sizeof (Syminfo);
			dyn++;
		}
		if (ofl->ofl_osmove) {
			Os_desc *	osp;

			dyn->d_tag = DT_MOVEENT;
			osp = ofl->ofl_osmove;
			dyn->d_un.d_val = osp->os_shdr->sh_entsize;
			dyn++;
			dyn->d_tag = DT_MOVESZ;
			dyn->d_un.d_val = osp->os_shdr->sh_size;
			dyn++;
			dyn->d_tag = DT_MOVETAB;
			dyn->d_un.d_val = osp->os_shdr->sh_addr;
			dyn++;
		}
		if (ofl->ofl_regsymcnt) {
			for (LIST_TRAVERSE(&ofl->ofl_regsyms, lnp, sdp)) {
				if (sdp->sd_ref > REF_DYN_SEEN) {
					dyn->d_tag = M_DT_REGISTER;
					dyn->d_un.d_val = sdp->sd_symndx;
					dyn++;
				}
			}
		}
		if (ofl->ofl_osdynamic->os_sgdesc &&
		    (ofl->ofl_osdynamic->os_sgdesc->sg_phdr.p_flags & PF_W)) {
			if (ofl->ofl_osinterp) {
				dyn->d_tag = DT_DEBUG;
				dyn->d_un.d_ptr = 0;
				dyn++;
			}

			dyn->d_tag = DT_FEATURE_1;
			if (ofl->ofl_osmove)
				dyn->d_un.d_val = 0;
			else
				dyn->d_un.d_val = DTF_1_PARINIT;
			dyn++;
		}
	}

	if (flags & (FLG_OF_SYMBOLIC | FLG_OF_BINDSYMB)) {
		dyn->d_tag = DT_SYMBOLIC;
		dyn->d_un.d_val = 0;
		dyn++;
	}

	dyn->d_tag = DT_FLAGS_1;
	dyn->d_un.d_val = ofl->ofl_dtflags;
	dyn++;

	mach_update_odynamic(ofl, &dyn);

	dyn->d_tag = DT_NULL;
	dyn->d_un.d_val = 0;

	return (1);
}

/*
 * Build the version definition section
 */
int
update_overdef(Ofl_desc * ofl)
{
	Listnode *	lnp1, * lnp2;
	Ver_desc *	vdp, * _vdp;
	Verdef *	vdf, * _vdf;
	int		num = 0;
	Os_desc *	strosp, * symosp;

	/*
	 * Traverse the version descriptors and update the version structures
	 * to point to the dynstr name in preparation for building the version
	 * section structure.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_verdesc, lnp1, vdp)) {
		Sym_desc *	sdp;

		if (vdp->vd_flags & VER_FLG_BASE) {
			const char	*name = vdp->vd_name;
			size_t		len = strlen(name) + 1;

			/*
			 * Create a new string table entry to represent the base
			 * version name (there is no corresponding symbol for
			 * this).
			 */
			if (!(ofl->ofl_flags & FLG_OF_DYNAMIC)) {
				vdp->vd_name = (const char *)(strtab - _strtab);
				(void) strcpy(strtab, name);
				strtab += len;
			} else {
				vdp->vd_name = (const char *)(dynstr - _dynstr);
				(void) strcpy(dynstr, name);
				dynstr += len;
			}
		} else {
			sdp = sym_find(vdp->vd_name, vdp->vd_hash, ofl);
			/* LINTED */
			vdp->vd_name = (const char *)sdp->sd_sym->st_name;
		}
	}

	_vdf = vdf = (Verdef *)ofl->ofl_osverdef->os_outdata->d_buf;

	/*
	 * Traverse the version descriptors and update the version section to
	 * reflect each version and its associated dependencies.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_verdesc, lnp1, vdp)) {
		Half		cnt = 1;
		Verdaux *	vdap, * _vdap;

		_vdap = vdap = (Verdaux *)(vdf + 1);

		vdf->vd_version = VER_DEF_CURRENT;
		vdf->vd_flags	= vdp->vd_flags & MSK_VER_USER;
		vdf->vd_ndx	= vdp->vd_ndx;
		vdf->vd_hash	= vdp->vd_hash;

		/* LINTED */
		vdap->vda_name = (Word)vdp->vd_name;
		vdap++;
		/* LINTED */
		_vdap->vda_next = (Word)((uintptr_t)vdap - (uintptr_t)_vdap);

		/*
		 * Traverse this versions dependency list generating the
		 * appropriate version dependency entries.
		 */
		for (LIST_TRAVERSE(&vdp->vd_deps, lnp2, _vdp)) {
			/* LINTED */
			vdap->vda_name = (Word)_vdp->vd_name;
			_vdap = vdap;
			vdap++, cnt++;
			/* LINTED */
			_vdap->vda_next = (Word)((uintptr_t)vdap -
			    (uintptr_t)_vdap);
		}
		_vdap->vda_next = 0;

		/*
		 * Record the versions auxiliary array offset and the associated
		 * dependency count.
		 */
		/* LINTED */
		vdf->vd_aux = (Word)((uintptr_t)(vdf + 1) - (uintptr_t)vdf);
		vdf->vd_cnt = cnt;

		/*
		 * Record the next versions offset and update the version
		 * pointer.  Remember the previous version offset as the very
		 * last structures next pointer should be null.
		 */
		_vdf = vdf;
		vdf = (Verdef *)vdap, num++;
		/* LINTED */
		_vdf->vd_next = (Word)((uintptr_t)vdf - (uintptr_t)_vdf);
	}
	_vdf->vd_next = 0;

	/*
	 * Record the string table association with the version definition
	 * section, and the symbol table associated with the version symbol
	 * table (the actual contents of the version symbol table are filled
	 * in during symbol update).
	 */
	if (ofl->ofl_flags & FLG_OF_RELOBJ) {
		strosp = ofl->ofl_osstrtab;
		symosp = ofl->ofl_ossymtab;
	} else {
		strosp = ofl->ofl_osdynstr;
		symosp = ofl->ofl_osdynsym;
	}
	/* LINTED */
	ofl->ofl_osverdef->os_shdr->sh_link = (Word)elf_ndxscn(strosp->os_scn);
	/* LINTED */
	ofl->ofl_osversym->os_shdr->sh_link = (Word)elf_ndxscn(symosp->os_scn);

	/*
	 * The version definition sections `info' field is used to indicate the
	 * number of entries in this section.
	 */
	ofl->ofl_osverdef->os_shdr->sh_info = num;

	return (1);
}

/*
 * Build the version needed section
 */
int
update_overneed(Ofl_desc * ofl)
{
	Listnode *	lnp;
	Ifl_desc *	ifl;
	Verneed *	vnd, * _vnd;
	char		*_dynstr = ofl->ofl_osdynstr->os_outdata->d_buf;
	Word		num = 0;
	Word		cnt = 0;

	_vnd = vnd = (Verneed *)ofl->ofl_osverneed->os_outdata->d_buf;

	/*
	 * Traverse the shared object list looking for dependencies that have
	 * versions defined within them.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_sos, lnp, ifl)) {
		Half		_cnt;
		Vernaux *	_vnap, * vnap;
		Sdf_desc *	sdf = ifl->ifl_sdfdesc;

		if (!(ifl->ifl_flags & FLG_IF_VERNEED))
			continue;

		vnd->vn_version = VER_NEED_CURRENT;

		(void) strcpy(dynstr, ifl->ifl_soname);
		/* LINTED */
		vnd->vn_file = (Word)(dynstr - _dynstr);
		dynstr += strlen(ifl->ifl_soname) + 1;

		_vnap = vnap = (Vernaux *)(vnd + 1);

		if (sdf && (sdf->sdf_flags & FLG_SDF_SPECVER)) {
			Sdv_desc *	sdv;
			Listnode *	lnp2;

			/*
			 * If version needed definitions were specified in
			 * a mapfile ($VERSION=*) then record those
			 * definitions.
			 */
			for (LIST_TRAVERSE(&sdf->sdf_verneed, lnp2, sdv)) {
				(void) strcpy(dynstr, sdv->sdv_name);
				/* LINTED */
				vnap->vna_name = (Word)(dynstr - _dynstr);
				dynstr += strlen(sdv->sdv_name) + 1;
				/* LINTED */
				vnap->vna_hash = (Word)elf_hash(sdv->sdv_name);
				vnap->vna_flags = 0;
				vnap->vna_other = 0;
				_vnap = vnap;
				vnap++;
				cnt++;
				/* LINTED */
				_vnap->vna_next = (Word)((uintptr_t)vnap -
				    (uintptr_t)_vnap);
			}
		} else {

			/*
			 * Traverse the version index list recording
			 * each version as a needed dependency.
			 */
			for (cnt = _cnt = 0; _cnt <= ifl->ifl_vercnt;
			    _cnt++) {
				Ver_index *	vip = &ifl->ifl_verndx[_cnt];

				if (vip->vi_flags & FLG_VER_REFER) {
					(void) strcpy(dynstr, vip->vi_name);
					/* LINTED */
					vnap->vna_name = (Word)(dynstr -
					    _dynstr);
					dynstr += strlen(vip->vi_name) + 1;
					if (vip->vi_desc) {
					    vnap->vna_hash =
						vip->vi_desc->vd_hash;
					    vnap->vna_flags =
						vip->vi_desc->vd_flags;
					} else {
					    vnap->vna_hash = 0;
					    vnap->vna_flags = 0;
					}
					vnap->vna_other = 0;

					_vnap = vnap;
					vnap++, cnt++;
					_vnap->vna_next =
						/* LINTED */
						(Word)((uintptr_t)vnap -
						    (uintptr_t)_vnap);
				}
			}
		}
		_vnap->vna_next = 0;

		/*
		 * Record the versions auxiliary array offset and
		 * the associated dependency count.
		 */
		/* LINTED */
		vnd->vn_aux = (Word)((uintptr_t)(vnd + 1) - (uintptr_t)vnd);
		/* LINTED */
		vnd->vn_cnt = (Half)cnt;

		/*
		 * Record the next versions offset and update the version
		 * pointer.  Remember the previous version offset as the very
		 * last structures next pointer should be null.
		 */
		_vnd = vnd;
		vnd = (Verneed *)vnap, num++;
		/* LINTED */
		_vnd->vn_next = (Word)((uintptr_t)vnd - (uintptr_t)_vnd);
	}
	_vnd->vn_next = 0;

	/*
	 * Record association on string table section and use the
	 * `info' field to indicate the number of entries in this
	 * section.
	 */
	ofl->ofl_osverneed->os_shdr->sh_link =
	    /* LINTED */
	    (Word)elf_ndxscn(ofl->ofl_osdynstr->os_scn);
	ofl->ofl_osverneed->os_shdr->sh_info = num;

	return (1);
}


/*
 * Update .syminfo section
 */
uintptr_t
update_osyminfo(Ofl_desc * ofl)
{
	Syminfo *	sip = ofl->ofl_ossyminfo->os_outdata->d_buf;
	Shdr *		shdr = ofl->ofl_ossyminfo->os_shdr;
	Os_desc *	symosp;
	char		*strtab;
	size_t		i, cnt;
	Listnode *	lnp;
	Sym_desc *	sdp;

	if (ofl->ofl_flags & FLG_OF_RELOBJ) {
		symosp = ofl->ofl_ossymtab;
		strtab = ofl->ofl_osstrtab->os_outdata->d_buf;
	} else {
		symosp = ofl->ofl_osdynsym;
		strtab = ofl->ofl_osdynstr->os_outdata->d_buf;
	}

	ofl->ofl_ossyminfo->os_shdr->sh_link =
		/* LINTED */
		(Word)elf_ndxscn(symosp->os_scn);
	if (ofl->ofl_osdynamic)
		ofl->ofl_ossyminfo->os_shdr->sh_info =
			/* LINTED */
			(Word)elf_ndxscn(ofl->ofl_osdynamic->os_scn);

	cnt = shdr->sh_size / shdr->sh_entsize;

	DBG_CALL(Dbg_syminfo_title());
	for (LIST_TRAVERSE(&ofl->ofl_syminfsyms, lnp, sdp)) {
		Ifl_desc *	ifl;
		if (sdp->sd_aux && sdp->sd_aux->sa_bindto)
			ifl = sdp->sd_aux->sa_bindto;
		else
			ifl = sdp->sd_file;
		sip[sdp->sd_symndx].si_boundto = ifl->ifl_neededndx;
	}
	/*
	 * Display debugging information about section
	 */
	if (dbg_mask) {
		Dyn *	dyn = 0;
		Sym *	symtab;

		if (ofl->ofl_osdynamic)
			dyn = ofl->ofl_osdynamic->os_outdata->d_buf;

		symtab = symosp->os_outdata->d_buf;
		for (i = 1; i < cnt; i++) {
			if (sip[i].si_flags || sip[i].si_boundto)
				/* LINTED */
				DBG_CALL(Dbg_syminfo_entry((int)i, &sip[i],
					&symtab[i], strtab, dyn));
		}
	}
	return (1);
}

/*
 * Build the output elf header.
 */
uintptr_t
update_oehdr(Ofl_desc * ofl)
{
	Ehdr *		ehdr = ofl->ofl_ehdr;

	/*
	 * If an entry point symbol has already been established (refer
	 * sym_validate()) simply update the elf header entry point with the
	 * symbols value.  If no entry point is defined it will have been filled
	 * with the start address of the first section within the text segment
	 * (refer update_outfile()).
	 */
	if (ofl->ofl_entry)
		ehdr->e_entry =
			((Sym_desc *)(ofl->ofl_entry))->sd_sym->st_value;

	/*
	 * Note. it may be necessary to update the `e_flags' field in the
	 * machine dependent section.
	 */
	ehdr->e_ident[EI_DATA] = M_DATA;
	if (ofl->ofl_e_machine != M_MACH) {
		if (ofl->ofl_e_machine != M_MACHPLUS)
			return (S_ERROR);
		if ((ofl->ofl_e_flags & M_FLAGSPLUS) == 0)
			return (S_ERROR);
	}
	ehdr->e_machine = ofl->ofl_e_machine;
	ehdr->e_flags = ofl->ofl_e_flags;
	ehdr->e_version = ofl->ofl_libver;

	if (ofl->ofl_flags & FLG_OF_SHAROBJ)
		ehdr->e_type = ET_DYN;
	else if (ofl->ofl_flags & FLG_OF_RELOBJ)
		ehdr->e_type = ET_REL;
	else
		ehdr->e_type = ET_EXEC;

	return (1);
}

/*
 * perform expansion
 */
static uintptr_t
expand_move(Ofl_desc *ofl, Sym_desc *sdp, Move *u1)
{
	Move *mv;
	Os_desc *osp;
	unsigned char *taddr, *taddr0;
	Sxword offset;
	int i;
	Addr base1;
	unsigned int stride;

	osp = ofl->ofl_issunwdata1->is_osdesc;
	base1 = (Addr)(osp->os_shdr->sh_addr +
		ofl->ofl_issunwdata1->is_indata->d_off);
	taddr0 = taddr = osp->os_outdata->d_buf;
	mv = u1;

	offset = sdp->sd_sym->st_value - base1;
	taddr += offset;
	taddr = taddr + mv->m_poffset;
	for (i = 0; i < mv->m_repeat; i++) {
		/* LINTED */
		DBG_CALL(Dbg_move_expanding(
		(const unsigned char *) (taddr - taddr0)));
		stride = (unsigned int)mv->m_stride + 1;
		/* LINTED */
		switch (ELF_M_SIZE(mv->m_info)) {
		case 1:
			/* LINTED */
			*taddr = (unsigned char)mv->m_value;
			taddr += stride;
			break;
		case 2:
			/* LINTED */
			*((Half *)taddr) = (Half)mv->m_value;
			taddr += 2*stride;
			break;
		case 4:
			/* LINTED */
			*((Word *)taddr) = (Word)mv->m_value;
			taddr += 4*stride;
			break;
		case 8:
			/* LINTED */
			*((unsigned long long *)taddr) =
				mv->m_value;
			taddr += 8*stride;
			break;
		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_PSYM_FATAL2));
			return (S_ERROR);
		}
	}
	return (1);
}

/*
 * Update Move sections
 */
uintptr_t
update_move(Ofl_desc *ofl, Half symbssndx)
{
	Word		ndx = 0;
	Is_desc *	isp;
	Word		flags = ofl->ofl_flags;
	Move *		mv1;
	Move *		mv2;
	Listnode *	lnp1;
	Psym_info *	psym;

	/*
	 * Determine the index of the symbol table that will be referenced by
	 * the relocation entries.
	 */
	if ((flags & (FLG_OF_DYNAMIC|FLG_OF_RELOBJ)) == FLG_OF_DYNAMIC)
		/* LINTED */
		ndx = (Word) elf_ndxscn(ofl->ofl_osdynsym->os_scn);
	else if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ))
		/* LINTED */
		ndx = (Word) elf_ndxscn(ofl->ofl_ossymtab->os_scn);

	/*
	 * update sh_link and mv pointer for updating move table.
	 */
	if (ofl->ofl_osmove) {
		ofl->ofl_osmove->os_shdr->sh_link = ndx;
		mv1 = (Move *) ofl->ofl_osmove->os_outdata->d_buf;
	}

	/*
	 * Update symbol entry index
	 */
	for (LIST_TRAVERSE(&ofl->ofl_parsym, lnp1, psym)) {
		Listnode *	lnp2;
		Mv_itm *	mvp;
		Sym_desc 	*sdp;

		/*
		 * Expand move table
		 */
		if (psym->psym_symd->sd_flags & FLG_SY_PAREXPN) {
			DBG_CALL(Dbg_move_parexpn(psym->psym_symd->sd_name));
			DBG_CALL(Dbg_move_title(1));
			for (LIST_TRAVERSE(&(psym->psym_mvs), lnp2, mvp)) {
				if ((mvp->mv_flag & FLG_MV_OUTSECT) == 0)
					continue;
				mv2 = mvp->mv_ientry;
				sdp = psym->psym_symd;
				DBG_CALL(Dbg_move_mventry(0, mv2, sdp));
				(void) expand_move(ofl, sdp, mv2);
			}
			continue;
		}

		/*
		 * Process move table
		 */
		DBG_CALL(Dbg_move_outmove((const unsigned char *)
			psym->psym_symd->sd_name));
		DBG_CALL(Dbg_move_title(1));
		for (LIST_TRAVERSE(&(psym->psym_mvs), lnp2, mvp)) {
			if ((mvp->mv_flag & FLG_MV_OUTSECT) == 0)
				continue;
			isp = mvp->mv_isp;
			mv2 = mvp->mv_ientry;
			sdp = isp->is_file->ifl_oldndx[
				ELF_M_SYM(mv2->m_info)];

			DBG_CALL(Dbg_move_mventry(0, mv2, sdp));
			*mv1 = *mv2;
			if ((ofl->ofl_flags & FLG_OF_RELOBJ) == 0) {
				if (ELF_ST_BIND(sdp->sd_sym->st_info) ==
				    STB_LOCAL) {
				    mv1->m_info =
					/* LINTED */
					ELF_M_INFO(symbssndx, mv2->m_info);
				    if (ELF_ST_TYPE(sdp->sd_sym->st_info) !=
				    STT_SECTION) {
					mv1->m_poffset = sdp->sd_sym->st_value -
					ofl->ofl_isbss->
						is_osdesc->os_shdr->sh_addr +
					mv2->m_poffset;
				    }
				} else {
				    mv1->m_info =
					/* LINTED */
					ELF_M_INFO(sdp->sd_symndx, mv2->m_info);
				}
			} else {
				if ((ELF_ST_BIND(sdp->sd_sym->st_info) ==
				    STB_LOCAL) &&
				    (ofl->ofl_flags1 & FLG_OF1_REDLSYM)) {
					Word symndx;
					symndx =
					sdp->sd_isc->is_osdesc->os_scnsymndx;
					mv1->m_info =
					/* LINTED */
					ELF_M_INFO(symndx, mv2->m_info);
					mv1->m_poffset += sdp->sd_sym->st_value;
				} else {
					mv1->m_info =
					/* LINTED */
					ELF_M_INFO(sdp->sd_symndx, mv2->m_info);
				}
			}
			DBG_CALL(Dbg_move_mventry(1, mv1, sdp));
			mv1++;
		}
	}

	return (1);
}

/*
 * Translate the shdr->sh_link from its input section value to that
 * of the corresponding shdr->sh_link output section value.
 */
Word
translate_sh_link(Os_desc * osp)
{
	Is_desc *	isp;
	Ifl_desc *	ifl;
	Word		link;

	if ((link = osp->os_shdr->sh_link) == 0)
		return (link);

	/*
	 * Does this output section translate back to an input file.  If not
	 * then there is no translation to do.  In this case we will assume that
	 * if sh_link has a value, it's the right value.
	 */
	isp = (Is_desc *)osp->os_isdescs.head->data;
	if ((ifl = isp->is_file) == NULL)
		return (link);

	/*
	 * Sanity check to make sure that the sh_link value is within range for
	 * the input file.
	 */
	if (link > ifl->ifl_ehdr->e_shnum) {
		eprintf(ERR_WARNING, MSG_INTL(MSG_UPD_LINKRANGE), ifl->ifl_name,
		    isp->is_name, EC_WORD(link));
		return (link);
	}

	/*
	 * Follow the link to the input section.
	 */
	if ((isp = ifl->ifl_isdesc[link]) == 0)
		return (0);
	if ((osp = isp->is_osdesc) == 0)
		return (0);

	/* LINTED */
	return ((Word)elf_ndxscn(osp->os_scn));
}

/*
 * Having created all of the necessary sections, segments, and associated
 * headers, fill in the program headers and update any other data in the
 * output image.  Some general rules:
 *
 *  o	If an interpretor is required always generate a PT_PHDR entry as
 *	well.  It is this entry that triggers the kernel into passing the
 *	interpretor an aux vector instead of just a file descriptor.
 *
 *  o	When generating an image that will be interpreted (ie. a dynamic
 *	executable, a shared object, or a static executable that has been
 *	provided with an interpretor - weird, but possible), make the initial
 *	loadable segment include both the ehdr and phdr[].  Both of these
 *	tables are used by the interpretor therefore it seems more intuitive
 *	to explicitly defined them as part of the mapped image rather than
 *	relying on page rounding by the interpretor to allow their access.
 *
 *  o	When generating a static image that does not require an interpretor
 *	have the first loadable segment indicate the address of the first
 *	.section as the start address (things like /kernel/unix and ufsboot
 *	expect this behavior).
 */
uintptr_t
update_outfile(Ofl_desc * ofl)
{
	Addr		vaddr = ofl->ofl_segorigin;
	Listnode *	lnp1, * lnp2;
	Sg_desc *	sgp;
	Os_desc *	osp;
	int		phdrndx = 0, segndx = -1, secndx;
	Ehdr *		ehdr = ofl->ofl_ehdr;
	Word		flags = ofl->ofl_flags;
	Addr		etext;
	List		osecs;
	Shdr *		hshdr;
	Addr		size;
	Phdr *		_phdr = 0;
	Word		phdrsz = ehdr->e_phnum * ehdr->e_phentsize;
	Word		ehdrsz = ehdr->e_ehsize;
	Boolean		nobits;
	Half 		symbssndx = 0;
	Half 		sunwbssndx = 0;

	Off		offset;

	/*
	 * Loop through the segment descriptors and pick out what we need.
	 */
	DBG_CALL(Dbg_seg_title());
	for (LIST_TRAVERSE(&ofl->ofl_segs, lnp1, sgp)) {
		Phdr *	phdr = &(sgp->sg_phdr);
		Xword 	p_align;

		segndx++;

		/*
		 * If an interpreter is required generate a PT_INTERP and
		 * PT_PHDR program header entry.  The PT_PHDR entry describes
		 * the program header table itself.  This information will be
		 * passed via the aux vector to the interpreter (ld.so.1).
		 * The program header array is actually part of the first
		 * loadable segment (and the PT_PHDR entry is the first entry),
		 * therefore its virtual address isn't known until the first
		 * loadable segment is processed.
		 */
		if (phdr->p_type == PT_PHDR) {
			if (ofl->ofl_osinterp) {
				phdr->p_offset = ehdr->e_phoff;
				phdr->p_filesz = phdr->p_memsz = phdrsz;
				DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine,
					segndx, sgp));
				ofl->ofl_phdr[phdrndx++] = *phdr;
			}
			continue;
		}
		if (phdr->p_type == PT_INTERP) {
			if (ofl->ofl_osinterp) {
				Shdr *	shdr = ofl->ofl_osinterp->os_shdr;

				phdr->p_vaddr = phdr->p_memsz = 0;
				phdr->p_offset = shdr->sh_offset;
				phdr->p_filesz = shdr->sh_size;
				DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine,
					segndx, sgp));
				ofl->ofl_phdr[phdrndx++] = *phdr;
			}
			continue;
		}

		/*
		 * As the dynamic program header occurs after the loadable
		 * headers in the segment descriptor table, all the address
		 * information for the .dynamic output section will have been
		 * figured out by now.
		 */
		if (phdr->p_type == PT_DYNAMIC) {
			if ((flags & (FLG_OF_DYNAMIC | FLG_OF_RELOBJ)) ==
			    FLG_OF_DYNAMIC) {
				Shdr *	shdr = ofl->ofl_osdynamic->os_shdr;

				phdr->p_vaddr = shdr->sh_addr;
				phdr->p_offset = shdr->sh_offset;
				phdr->p_filesz = shdr->sh_size;
				phdr->p_flags = M_DATASEG_PERM;
				DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine,
					segndx, sgp));
				ofl->ofl_phdr[phdrndx++] = *phdr;
			}
			continue;
		}

#if	defined(ELF_TARGET_IA64)
		/*
		 * Create the UNWIND program header.
		 */
		if (phdr->p_type == PT_IA_64_UNWIND) {
			if ((ofl->ofl_osunwind) && !(flags & FLG_OF_RELOBJ)) {
				Shdr *	shdr;
				Shdr *	endshdr;

				shdr = ofl->ofl_osunwind->os_shdr;
				endshdr = ofl->ofl_osendunwind->os_shdr;

				phdr->p_vaddr = shdr->sh_addr;
				phdr->p_offset = shdr->sh_offset;
				phdr->p_flags = PF_R | PF_X;
				phdr->p_memsz = phdr->p_filesz =
					(endshdr->sh_addr + endshdr->sh_size)
					- shdr->sh_addr;
				DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine,
					segndx, sgp));
				ofl->ofl_phdr[phdrndx++] = *phdr;
			}
			continue;
		}
#endif	/* ELF_TARGET_IA64 */

		/*
		 * If this is an empty segment declaration, it will occur after
		 * all other loadable segments, make sure the previous segment
		 * doesn't overlap.
		 */
		if (sgp->sg_flags & FLG_SG_EMPTY) {
			if (_phdr && (vaddr > phdr->p_vaddr) &&
			    (phdr->p_type == PT_LOAD))
				eprintf(ERR_WARNING,
				    MSG_INTL(MSG_UPD_SEGOVERLAP), ofl->ofl_name,
				    EC_ADDR(vaddr), sgp->sg_name,
				    EC_ADDR(phdr->p_vaddr));
			vaddr = phdr->p_vaddr;
			phdr->p_memsz = sgp->sg_length;
			DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine,
				segndx, sgp));
			ofl->ofl_phdr[phdrndx++] = *phdr;
			continue;
		}

		/*
		 * Having processed any of the special program headers any
		 * remaining headers will be built to express individual
		 * segments.  Segments are only built if they have output
		 * section descriptors associated with them (ie. some form of
		 * input section has been matched to this segment).
		 */
		osecs = sgp->sg_osdescs;
		if (osecs.head == NULL)
			continue;

		/*
		 * Determine the segments offset and size from the section
		 * information provided from elf_update().
		 * Allow for multiple NOBITS sections.
		 */
		hshdr = ((Os_desc *)osecs.head->data)->os_shdr;

		phdr->p_filesz = 0;
		phdr->p_memsz = 0;
		phdr->p_offset = offset = hshdr->sh_offset;
		nobits = (hshdr->sh_type == SHT_NOBITS);
		for (LIST_TRAVERSE(&osecs, lnp2, osp)) {
			Shdr *	shdr = osp->os_shdr;

			p_align = 0;
			if (shdr->sh_addralign > p_align)
				p_align = shdr->sh_addralign;
			offset = (Off)S_ROUND(offset, shdr->sh_addralign);
			offset += shdr->sh_size;
			if (shdr->sh_type != SHT_NOBITS) {
				if (nobits) {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_UPD_NOBITS));
					return (S_ERROR);
				}
				phdr->p_filesz = offset - phdr->p_offset;
			} else
				nobits = TRUE;
		}
		phdr->p_memsz = offset - hshdr->sh_offset;

		/*
		 * If this is PT_SUNWBSS, set alignment
		 */
		if (phdr->p_type == PT_SUNWBSS)
			phdr->p_align = p_align;

		/*
		 * If this is the first loadable segment of a dynamic object,
		 * or an interpretor has been specified (a static object built
		 * with an interpretor will still be given a PT_HDR entry), then
		 * compensate for the elf header and program header array.  Both
		 * of these are actually part of the loadable segment as they
		 * may be inspected by the interpretor.  Adjust the segments
		 * size and offset accordingly.
		 */
		if ((_phdr == 0) && (phdr->p_type == PT_LOAD) &&
		    ((ofl->ofl_osinterp) || (flags & FLG_OF_DYNAMIC)) &&
		    (!(sgp->sg_flags & FLG_SG_NOHDR))) {
			size = (Addr)S_ROUND((phdrsz + ehdrsz),
			    hshdr->sh_addralign);
			phdr->p_offset -= size;
			phdr->p_filesz += size;
			phdr->p_memsz += size;
		}

		/*
		 * If a segment size symbol is required (specified via a
		 * mapfile) update its value.
		 */
		if (sgp->sg_sizesym != NULL)
			sgp->sg_sizesym->sd_sym->st_value = phdr->p_memsz;

		/*
		 * If no file content has been assigned to this segment (it
		 * only contains no-bits sections), then reset the offset for
		 * consistancy.
		 */
		if (phdr->p_filesz == 0)
			phdr->p_offset = 0;

		/*
		 * If a virtual address has been specified for this segment
		 * (presumably from a map file) use it and make sure the
		 * previous segment does not run into this segment.
		 */
		if ((phdr->p_type == PT_LOAD) ||
		    (phdr->p_type == PT_SUNWBSS)) {
			if ((sgp->sg_flags & FLG_SG_VADDR)) {
				if (_phdr && (vaddr > phdr->p_vaddr) &&
				    (phdr->p_type == PT_LOAD))
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_UPD_SEGOVERLAP),
					    ofl->ofl_name, EC_ADDR(vaddr),
					    sgp->sg_name,
					    EC_ADDR(phdr->p_vaddr));
				vaddr = phdr->p_vaddr;
				phdr->p_align = 0;
			} else {
				vaddr = phdr->p_vaddr =
				    (Addr)S_ROUND(vaddr, phdr->p_align) +
					(phdr->p_offset % phdr->p_align);
			}
		}

		/*
		 * If an interpreter is required set the virtual address of the
		 * PT_PHDR program header now that we know the virtual address
		 * of the loadable segment that contains it.
		 */
		if ((_phdr == 0) && (phdr->p_type == PT_LOAD)) {
			_phdr = phdr;

			if (!(sgp->sg_flags & FLG_SG_NOHDR)) {
				if (ofl->ofl_osinterp)
					ofl->ofl_phdr[0].p_vaddr =
					    vaddr + ehdrsz;

				/*
				 * Finally, if we're creating a dynamic object
				 * (or a static object in which an interpretor
				 * is specified) update the vaddr to reflect
				 * the address of the first section within this
				 * segment.
				 */
				if ((ofl->ofl_osinterp) ||
				    (flags & FLG_OF_DYNAMIC))
					vaddr += size;
			} else {
				/*
				 * If the FLG_SG_NOHDR flag was set, PT_PHDR
				 * will not be part of any loadable segment.
				 */
				ofl->ofl_phdr[0].p_vaddr = 0;
				ofl->ofl_phdr[0].p_memsz = 0;
				ofl->ofl_phdr[0].p_flags = 0;
			}
		}

		/*
		 * Save the address of the first executable section for default
		 * use as the execution entry point.  This may get overridden in
		 * update_oehdr().
		 */
		if (!(flags & FLG_OF_RELOBJ) && !(ehdr->e_entry) &&
		    (phdr->p_flags & PF_X))
			ehdr->e_entry = vaddr;

		DBG_CALL(Dbg_seg_entry(ofl->ofl_e_machine, segndx, sgp));

		/*
		 * Traverse the output section descriptors for this segment so
		 * that we can update the section headers addresses.  We've
		 * calculated the virtual address of the initial section within
		 * this segment, so each successive section can be calculated
		 * based on their offsets from each other.
		 */
		secndx = 0;
		hshdr = 0;
		for (LIST_TRAVERSE(&(sgp->sg_osdescs), lnp2, osp)) {
			Shdr *	shdr = osp->os_shdr;

			shdr->sh_link = translate_sh_link(osp);

			if (!(flags & FLG_OF_RELOBJ) &&
			    (phdr->p_type == PT_LOAD) ||
			    (phdr->p_type == PT_SUNWBSS)) {
				if (hshdr)
					vaddr += (shdr->sh_offset -
					    hshdr->sh_offset);

				shdr->sh_addr = vaddr;
				hshdr = shdr;
			}

			DBG_CALL(Dbg_seg_os(ofl, osp, secndx));
			secndx++;
		}

		/*
		 * Establish the virtual address of the end of the last section
		 * in this segment so that the next segments offset can be
		 * calculated from this.
		 */
		if (hshdr)
			vaddr += hshdr->sh_size;

		/*
		 * Output sections for this segment complete.  Adjust the
		 * virtual offset for the last sections size, and make sure we
		 * haven't exceeded any maximum segment length specification.
		 */
		if ((sgp->sg_length != 0) && (sgp->sg_length < phdr->p_memsz)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_UPD_LARGSIZE),
			    ofl->ofl_name, sgp->sg_name,
			    EC_XWORD(phdr->p_memsz),
			    EC_XWORD(sgp->sg_length));
			return (S_ERROR);
		}

		if (phdr->p_type == PT_NOTE) {
			phdr->p_vaddr = 0;
			phdr->p_paddr = 0;
			phdr->p_align = 0;
			phdr->p_memsz = 0;
		}
		if ((phdr->p_type != PT_NULL) && !(flags & FLG_OF_RELOBJ))
			ofl->ofl_phdr[phdrndx++] = *phdr;
	}

	/*
	 * Update any new output sections.  When building the initial output
	 * image, a number of sections were created but left uninitialized (eg.
	 * .dynsym, .dynstr, .symtab, .symtab, etc.).  Here we update these
	 * sections with the appropriate data.  Other sections may still be
	 * modified via reloc_process().
	 *
	 * Copy the interpretor name into the .interp section.
	 */
	if (ofl->ofl_interp)
		(void) strcpy((char *)ofl->ofl_osinterp->os_outdata->d_buf,
		    ofl->ofl_interp);


	/*
	 * Build any output symbol tables, the symbols information is copied
	 * and updated into the new output image.
	 */
	if ((etext = update_osym(ofl,
	    &symbssndx, &sunwbssndx)) == (Addr)S_ERROR)
		return (S_ERROR);

	/*
	 * Update Move Table
	 */
	if (ofl->ofl_osmove || ofl->ofl_issunwdata1) {
		if (update_move(ofl, symbssndx) == S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Build any output headers, version information, dynamic structure and
	 * syminfo structure.
	 */
	if (update_oehdr(ofl) == S_ERROR)
		return (S_ERROR);
	if ((flags & (FLG_OF_VERDEF | FLG_OF_NOVERSEC)) == FLG_OF_VERDEF)
		if (update_overdef(ofl) == S_ERROR)
			return (S_ERROR);
	if ((flags & (FLG_OF_VERNEED | FLG_OF_NOVERSEC)) == FLG_OF_VERNEED)
		if (update_overneed(ofl) == S_ERROR)
			return (S_ERROR);
	if (flags & FLG_OF_DYNAMIC)
		if (update_odynamic(ofl) == S_ERROR)
			return (S_ERROR);
	if (ofl->ofl_ossyminfo)
		if (update_osyminfo(ofl) == S_ERROR)
			return (S_ERROR);

	/*
	 * Initialize the section headers string table index within the elf
	 * header.
	 */
	ofl->ofl_ehdr->e_shstrndx =
	    /* LINTED */
	    (Half)elf_ndxscn(ofl->ofl_osshstrtab->os_scn);

	return ((uintptr_t)etext);
}
