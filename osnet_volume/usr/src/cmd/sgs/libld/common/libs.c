/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)libs.c	1.44	98/10/23 SMI"

/*
 * Library processing
 */
#include	<stdio.h>
#include	<string.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"

/*
 * Because a tentative symbol may cause the extraction of an archive member,
 * make sure that the potential member is really required.  If the archive
 * member has a strong defined symbol it will be extracted.  If it simply
 * contains another tentative definition, or a defined function symbol, then it
 * will not be used.
 */
int
process_member(Ar_mem * amp, const char * name, unsigned char obind,
    Ofl_desc * ofl)
{
	Sym *		syms;
	Xword		symn, cnt;
	char *		strs;

	/*
	 * Find the first symbol table in the archive member, obtain its
	 * data buffer and determine the number of global symbols (Note,
	 * there must be a symbol table present otherwise the archive would
	 * never have been able to generate its own symbol entry for this
	 * member).
	 */
	if (amp->am_syms == 0) {
		Elf_Scn *	scn = NULL;
		Shdr *		shdr;
		Elf_Data *	data;

		while (scn = elf_nextscn(amp->am_elf, scn)) {
			if ((shdr = elf_getshdr(scn)) == NULL) {
				eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETSHDR),
				    amp->am_path);
				ofl->ofl_flags |= FLG_OF_FATAL;
				return (0);
			}
			if ((shdr->sh_type == SHT_SYMTAB) ||
			    (shdr->sh_type == SHT_DYNSYM))
				break;
		}
		if ((data = elf_getdata(scn, NULL)) == NULL) {
			eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETDATA),
			    amp->am_path);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (0);
		}
		syms = (Sym *)data->d_buf;
		syms += shdr->sh_info;
		symn = shdr->sh_size / shdr->sh_entsize;
		symn -= shdr->sh_info;

		/*
		 * Get the data for the associated string table.
		 */
		if ((scn = elf_getscn(amp->am_elf, (size_t)shdr->sh_link))
		    == NULL) {
			eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETSCN),
			    amp->am_path);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (0);
		}
		if ((data = elf_getdata(scn, NULL)) == NULL) {
			eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_GETDATA),
			    amp->am_path);
			ofl->ofl_flags |= FLG_OF_FATAL;
			return (0);
		}
		strs = data->d_buf;

		/*
		 * Initialize the archive member structure in case we have to
		 * come through here again.
		 */
		amp->am_syms = syms;
		amp->am_strs = strs;
		amp->am_symn = symn;
	} else {
		syms = amp->am_syms;
		strs = amp->am_strs;
		symn = amp->am_symn;
	}

	/*
	 * Loop through the symbol table entries looking for a match for the
	 * original symbol.  The archive member will be used if	the new symbol
	 * is a definition of an object (not a function).  Note however that a
	 * weak definition within the archive will not override a strong
	 * tentative symbol (see sym_realtent() resolution and ABI symbol
	 * binding description - page 4-27).
	 */
	for (cnt = 0; cnt < symn; syms++, cnt++) {
		Word		shndx = syms->st_shndx;
		unsigned char	info;

		if ((shndx == SHN_ABS) || (shndx == SHN_COMMON) ||
		    (shndx == SHN_UNDEF))
			continue;

		info = syms->st_info;
		if ((ELF_ST_TYPE(info) == STT_FUNC) ||
		    ((ELF_ST_BIND(info) == STB_WEAK) && (obind != STB_WEAK)))
			continue;

		if (strcmp(strs + syms->st_name, name) == NULL)
			return (1);
	}
	return (0);
}

/*
 * Create an archive descriptor.  By maintaining a list of archives any
 * duplicate occurrences of the same archive specified by the user enable us to
 * pick off where the last processing finished.
 */
Ar_desc *
ar_setup(const char * name, Elf * elf, int fd, Ofl_desc * ofl)
{
	Ar_desc *	adp;
	Elf *		arelf;
	size_t		number;
	Elf_Arsym *	start;

	/*
	 * Get the archive symbol table.  If this fails see if the file is
	 * really an elf archive so that we can provide a more meaningful error
	 * message.
	 */
	if ((start = elf_getarsym(elf, &number)) == 0) {
		(void) elf_rand(elf, 0);
		if (((arelf = elf_begin(fd, ELF_C_READ, elf)) != 0) &&
		    (elf_kind(arelf) != ELF_K_ELF)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_ELF_UNKNOWN), name);
			ofl->ofl_flags |= FLG_OF_FATAL;
		} else
			eprintf(ERR_WARNING, MSG_INTL(MSG_ELF_ARSYM), name);

		return (0);
	}

	/*
	 * As this is a new archive reference establish a new descriptor.
	 */
	if ((adp = (Ar_desc *)libld_malloc(sizeof (Ar_desc))) == 0)
		return ((Ar_desc *)S_ERROR);
	adp->ad_name = name;
	adp->ad_elf = elf;
	adp->ad_start = start;
	adp->ad_cnt = 10000;
	if ((adp->ad_aux = (Ar_aux *)libld_calloc(sizeof (Ar_aux),
	    number)) == 0)
		return ((Ar_desc *)S_ERROR);

	/*
	 * Add this new descriptor to the list of archives.
	 */
	if (list_appendc(&ofl->ofl_ars, adp) == 0)
		return ((Ar_desc *)S_ERROR);
	else
		return (adp);
}

/*
 * For each archive descriptor we maintain an `Ar_aux' table to parallel the
 * archive symbol table (returned from elf_getarsym(3e)).  We use this table to
 * hold the `Sym_desc' for each symbol (thus reducing the number of sym_find()'s
 * we have to do), and to hold the `Ar_mem' pointer.  The `Ar_mem' element can
 * have one of three values indicating the state of the archive member
 * associated with the offset for this symbol table entry:
 *
 *  0		indicates that the member has not been extracted.
 *
 *  AREXTRACTED	indicates that the member has been extracted.
 *
 *  addr	indicates that the member has been investigated to determine if
 *		it contained a symbol definition we need, but was found not to
 *		be a candidate for extraction.  In this case the members
 *		structure is maintained for possible later use.
 *
 * Each time we process an archive member we use its offset value to scan this
 * `Ar_aux' list.  If the member has been extracted, each entry with the same
 * offset has its `Ar_mem' pointer set to AREXTRACTED.  Thus if we cycle back
 * through the archive symbol table we will ignore these symbols as they will
 * have already been added to the output image.  If a member has been processed
 * but found not to contain a symbol we need, each entry with the same offset
 * has its `Ar_mem' pointer set to the member structures address.
 */
void
ar_member(Ar_desc * adp, Elf_Arsym * arsym, Ar_aux * aup, Ar_mem * amp)
{
	Elf_Arsym *	_arsym = arsym;
	Ar_aux *	_aup = aup;
	size_t		_off = arsym->as_off;

	if (_arsym != adp->ad_start) {
		do {
			_arsym--;
			_aup--;
			if (_arsym->as_off != _off)
				break;
			_aup->au_mem = amp;
		} while (_arsym != adp->ad_start);
	}

	_arsym = arsym;
	_aup = aup;

	do {
		if (_arsym->as_off != _off)
			break;
		_aup->au_mem = amp;
		_arsym++;
		_aup++;
	} while (_arsym->as_name);
}

/*
 * Read in the archive's symbol table; for each symbol in the table check
 * whether that symbol satisfies an unresolved, or tentative reference in
 * ld's internal symbol table; if so, the corresponding object from the
 * archive is processed.  The archive symbol table is searched until we go
 * through a complete pass without satisfying any unresolved symbols
 */
uintptr_t
process_archive(const char * name, int fd, Ar_desc * adp, Ofl_desc * ofl)
{
	Elf_Arsym *	arsym;
	Elf_Arhdr *	arhdr;
	Elf *		arelf;
	Ar_aux *	aup;
	Sym_desc *	sdp;
	char *		arname, * arpath;
	int		ndx, found = 0, again = 0;
	int		allexrt = ofl->ofl_flags1 & FLG_OF1_ALLEXRT;
	uintptr_t	err;
	int		ifl_errno;


	/*
	 * If this archive is already extracted by '-z allextract',
	 * no need to further process the file.
	 */
	if (adp->ad_elf == (Elf *) NULL)
		return (1);

	/*
	 * Loop through archive symbol table until we make a complete pass
	 * without satisfying an unresolved reference.  For each archive
	 * symbol, see if there is a symbol with the same name in ld's
	 * symbol table.  If so, and if that symbol is still unresolved or
	 * tentative, process the corresponding archive member.
	 */
	do {
		DBG_CALL(Dbg_file_archive(name, again));
		DBG_CALL(Dbg_syms_ar_title(name, again));

		ndx = again = 0;
		for (arsym = adp->ad_start, aup = adp->ad_aux; arsym->as_name;
		    ++arsym, ++aup, ndx++) {
			Ar_mem *	amp;
			Sym *		sym;
			Boolean		veravail;

			/*
			 * If the auxiliary members value indicates that this
			 * member has been extracted then this symbol will have
			 * been added to the output file image already.
			 */
			if (aup->au_mem == AREXTRACTED)
				continue;

			/*
			 * If the auxiliary symbol element is non-zero lookup
			 * the symbol from the internal symbol table.
			 * (But you skip this if allextract is specified.)
			 */
			if ((allexrt == 0) && ((sdp = aup->au_syms) == 0)) {
				if ((sdp = sym_find(arsym->as_name,
				    /* LINTED */
				    (Word)arsym->as_hash, ofl)) == 0) {
					DBG_CALL(Dbg_syms_ar_entry(ndx, arsym));
					continue;
				}
				aup->au_syms = sdp;
			}

			/*
			 * If '-z allextract' is on, this member will be
			 * extracted.
			 *
			 * This archive member is a candidate for extraction if
			 * the internal symbol is undefined or tentative.  By
			 * default weak references will not cause archive
			 * extraction, however the -zweakextract flag can be
			 * used to override this default.
			 * If this symbol has been bound to a versioned shared
			 * object make sure it is available for linking.
			 */
			if (allexrt == 0) {
				veravail = TRUE;
				if ((sdp->sd_ref == REF_DYN_NEED) &&
				    (sdp->sd_file->ifl_vercnt)) {
					Word		vndx;
					Ver_index *	vip;

					vndx = sdp->sd_aux->sa_verndx;
					vip = &sdp->sd_file->ifl_verndx[vndx];
					if (!(vip->vi_flags & FLG_VER_AVAIL))
						veravail = FALSE;
				}
				sym = sdp->sd_sym;
				if (((sym->st_shndx != SHN_UNDEF) &&
				    (sym->st_shndx != SHN_COMMON) &&
				    veravail) ||
				    ((ELF_ST_BIND(sym->st_info) == STB_WEAK) &&
				    (!(ofl->ofl_flags1 & FLG_OF1_WEAKEXT)))) {
					DBG_CALL(Dbg_syms_ar_entry(ndx, arsym));
					continue;
				}
			}

			/*
			 * Determine if we have already extracted this member,
			 * and if so reuse the Ar_mem information.
			 */
			if ((amp = aup->au_mem) != 0) {
				arelf = amp->am_elf;
				arname = amp->am_name;
				arpath = amp->am_path;
			} else {
				/*
				 * Set up a new elf descriptor for this member.
				 */
				if (elf_rand(adp->ad_elf, arsym->as_off) !=
				    arsym->as_off) {
					eprintf(ERR_ELF,
					    MSG_INTL(MSG_ELF_ARMEM), name,
					    EC_WORD(arsym->as_off), ndx,
					    arsym->as_name);
					ofl->ofl_flags |= FLG_OF_FATAL;
					return (0);
				}
				if ((arelf = elf_begin(fd, ELF_C_READ,
				    adp->ad_elf)) == NULL) {
					eprintf(ERR_ELF,
					    MSG_ORIG(MSG_ELF_BEGIN), name);
					ofl->ofl_flags |= FLG_OF_FATAL;
					return (0);
				}

				/*
				 * Construct the member filename.
				 */
				if ((arhdr = elf_getarhdr(arelf)) == NULL) {
					eprintf(ERR_ELF,
					    MSG_ORIG(MSG_ELF_GETARHDR), name);
					ofl->ofl_flags |= FLG_OF_FATAL;
					return (0);
				}
				arname = arhdr->ar_name;

				/*
				 * Construct the members full pathname
				 */
				if ((arpath = (char *)libld_malloc(
				    strlen(name) + strlen(arname) + 3)) == 0)
					return (S_ERROR);
				(void) sprintf(arpath, MSG_ORIG(MSG_FMT_ARMEM),
				    name, arname);
			}

			/*
			 * If the symbol for which this archive member is
			 * being processed is a tentative symbol, then this
			 * member must be verified to insure that it is
			 * going to provided a symbol definition that will
			 * override the tentative symbol.
			 */
			if ((allexrt == 0) && (sym->st_shndx == SHN_COMMON)) {
				/* LINTED */
				Byte bind = (Byte)ELF_ST_BIND(sym->st_info);

				/*
				 * If we don't already have a member structure
				 * allocate one.
				 */
				if (!amp) {
					if ((amp = (Ar_mem *)
					    libld_calloc(sizeof (Ar_mem),
					    1)) == 0)
						return (S_ERROR);
					amp->am_elf = arelf;
					amp->am_name = arname;
					amp->am_path = arpath;
				}
				DBG_CALL(Dbg_syms_ar_checking(ndx, arsym,
				    arname));
				if ((err = process_member(amp, arsym->as_name,
				    bind, ofl)) == S_ERROR)
					return (S_ERROR);

				/*
				 * If it turns out that we don't need this
				 * member simply initialize all other auxiliary
				 * entries that match this offset with this
				 * members address.  In this way we can resuse
				 * this information if we recurse back to this
				 * symbol.
				 */
				if (err == 0) {
					if (aup->au_mem == 0)
						ar_member(adp, arsym, aup, amp);
					continue;
				}
			}

			/*
			 * Process the archive member.  Retain any error for
			 * return to the caller.
			 */
			DBG_CALL(Dbg_syms_ar_resolve(ndx, arsym, arname,
			    allexrt));
			if ((err = (uintptr_t)process_ifl(arpath, NULL, fd,
			    arelf, FLG_IF_EXTRACT | FLG_IF_NEEDED, ofl,
			    &ifl_errno)) == S_ERROR)
				return (S_ERROR);

			/*
			 * Keep going if process_ifl() failed due to
			 * an ELF mismatch.
			 */
			if (ifl_errno > 0)
				continue;

			/*
			 * Indicate that we've extracted a member (enables
			 * debugging diag), and if we're not under -z allextract
			 * we need to rescan the archive.
			 */
			if (err) {
				found = 1;
				if (allexrt == 0)
					again = 1;
			}

			ar_member(adp, arsym, aup, AREXTRACTED);
			DBG_CALL(Dbg_syms_nl());
		}
	} while (again);

	if (found == 0)
		DBG_CALL(Dbg_file_unused(name));

	/*
	 * If this archive was extreacted by '-z allextract',
	 * ar_aux table and elf descriptor can be freed.
	 * Also set ad_elf to NULL to mark the archive is already
	 * processed.
	 */
	if (allexrt) {
		if (adp->ad_aux)
			(void) free(adp->ad_aux);
		(void) elf_end(adp->ad_elf);
		adp->ad_elf = (Elf *) NULL;
	}

	/*
	 * If this archives usage count has gone to zero free up any members
	 * that had been processed but were not really needed together with any
	 * other descriptors for this archive.
	 * (If '-z allextract' was specified, don't do it since some data are
	 * free'ed above already.)
	 */
	if ((--adp->ad_cnt == 0) && (allexrt == 0)) {
		Ar_mem *	amp = 0;

		for (arsym = adp->ad_start, aup = adp->ad_aux; arsym->as_name;
		    ++arsym, ++aup) {

			if (aup->au_mem && (aup->au_mem != AREXTRACTED)) {
				if (aup->au_mem != amp) {
					amp = aup->au_mem;
					(void) elf_end(amp->am_elf);
				}
			} else
				amp = 0;
		}

		(void) elf_end(adp->ad_elf);

		/*
		 * We should now get rid of the archive descriptor.
		 */
	}
	return (1);
}
