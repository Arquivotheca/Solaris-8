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
#pragma ident	"@(#)machrel.c	1.76	99/09/14 SMI"

#include	<string.h>
#include	<assert.h>
#include	<sys/elf_SPARC.h>
#include	"debug.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_libld.h"

/*
 *	Local Variable Definitions
 */
static Xword negative_got_offset = 0;
				/* offset of GOT table from GOT symbol */
static Word countSmallGOT = M_GOT_XNumber;
				/* number of small GOT symbols */



void
mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * DT_PLTGOT
		 *
		 * This entry is created on sparc if we are going to
		 * create a PLT table
		 */
		if (ofl->ofl_pltcnt)
			(*cnt)++;				/* DT_PLTGOT */
	}
}

void
mach_update_odynamic(Ofl_desc * ofl, Dyn ** dyn)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		if (ofl->ofl_pltcnt) {
			(*dyn)->d_tag = DT_PLTGOT;
			(*dyn)->d_un.d_ptr = fillin_gotplt2(ofl);
			(*dyn)++;
		}
	}
}

#if	defined(_ELF64)
/*
 *	Build a single V9 P.L.T. entry - code is:
 *
 *	For Target Addresses +/- 4GB of the entry
 *	-----------------------------------------
 *	sethi	(. - .PLT0), %g1
 *	ba,a	%xcc, .PLT1
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *
 *	For Target Addresses +/- 2GB of the entry
 *	-----------------------------------------
 *
 *	.PLT0 is the address of the first entry in the P.L.T.
 *	This one is filled in by the run-time link editor. We just
 *	have to leave space for it.
 */
void
plt_entry(Ofl_desc *ofl, Xword pltndx)
{
	unsigned char	*pltent;	/* PLT entry being created. */
	Sxword		pltoff;		/* Offset of this entry from PLT top */


	/*
	 *  XX64:  The second part of the V9 ABI (sec. 5.2.4)
	 *  applies to plt entries greater than 0x8000 (32,768).
	 *  This isn't implemented yet.
	 */
	assert(pltndx < 0x8000);


	pltoff = M_PLT_RESERVSZ + (pltndx - 1) * M_PLT_ENTSIZE;
	pltent = (unsigned char *)ofl->ofl_osplt->os_outdata->d_buf + pltoff;

	/*
	 * PLT[0]: sethi %hi(. - .L0), %g1
	 */
	/* LINTED */
	*(Word *)pltent = M_SETHIG1 | pltoff;

	/*
	 * PLT[1]: ba,a %xcc, .PLT1 (.PLT1 accessed as a PC-relative index
	 * of longwords).
	 */
	pltent += M_PLT_INSSIZE;
	pltoff += M_PLT_INSSIZE;
	pltoff = -pltoff;
	/* LINTED */
	*(Word *)pltent = M_BA_A_XCC | (((pltoff + M_PLT_ENTSIZE) >> 2) &
					S_MASK(19));

	/*
	 * PLT[2]: sethi 0, %g0 (NOP for delay slot of eventual CTI).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[3]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[4]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[5]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[6]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[7]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;
}

#else  /* Elf 32 */

/*
 *	Build a single P.L.T. entry - code is:
 *
 *	sethi	(. - .L0), %g1
 *	ba,a	.L0
 *	sethi	0, %g0		(nop)
 *
 *	.L0 is the address of the first entry in the P.L.T.
 *	This one is filled in by the run-time link editor. We just
 *	have to leave space for it.
 */
void
plt_entry(Ofl_desc * ofl, Xword pltndx)
{
	Byte *	pltent;	/* PLT entry being created. */
	Sxword	pltoff;	/* Offset of this entry from PLT top */

	pltoff = M_PLT_RESERVSZ + (pltndx - 1) * M_PLT_ENTSIZE;
	pltent = (Byte *)ofl->ofl_osplt->os_outdata->d_buf + pltoff;

	/*
	 * PLT[0]: sethi %hi(. - .L0), %g1
	 */
	/* LINTED */
	*(Word *)pltent = M_SETHIG1 | pltoff;

	/*
	 * PLT[1]: ba,a .L0 (.L0 accessed as a PC-relative index of longwords)
	 */
	pltent += M_PLT_INSSIZE;
	pltoff += M_PLT_INSSIZE;
	pltoff = -pltoff;
	/* LINTED */
	*(Word *)pltent = M_BA_A | ((pltoff >> 2) & S_MASK(22));

	/*
	 * PLT[2]: sethi 0, %g0 (NOP for delay slot of eventual CTI).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_SETHIG0;

	/*
	 * PLT[3]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_SETHIG0;
}

#endif /* _ELF64 */

/*
 * Partially Initialized Symbol Handling routines
 */
static Sym_desc *
am_I_partial(Rel_desc *reld)
{
	Sym_desc *	symd;
	Ifl_desc *	ifile;
	int 		nlocs;	/* number of local symbols */
	int 		i;

	ifile = reld->rel_sym->sd_isc->is_file;
	nlocs = ifile->ifl_locscnt;
	for (i = 1; i < nlocs; i++) {
		symd = ifile->ifl_oldndx[i];
		if (symd->sd_osym == 0)
			continue;
		if ((symd->sd_flags & FLG_SY_PAREXPN) == 0)
			continue;
		if (symd->sd_osym->st_value == reld->rel_raddend)
			return (symd);
	}
	return ((Sym_desc *) 0);
}


uintptr_t
perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *		osp = 0;	/* output section */
	Os_desc *		relosp;		/* reloc output section */
	Xword			ndx;		/* sym & scn index */
	Xword			roffset;	/* roffset for output rel */
	Xword			value;
	Sxword			raddend;	/* raddend for output rel */
	Rela			rea;		/* SHT_RELA entry. */
	char			*relbits;
	Sym_desc *		sdp;		/* current relocation sym */
	const Rel_entry *	rep;
	Sym_desc *		psym;		/* Partially init. sym */
	int			sectmoved;

	psym = (Sym_desc *) NULL;
	raddend = orsp->rel_raddend;
	sdp = orsp->rel_sym;
	sectmoved = 0;

	/*
	 * Special case, a regsiter symbol associated with symbol
	 * index 0 is initialized (i.e. relocated) to a constant
	 * in the r_addend field rather than to a symbol value.
	 */
	if ((orsp->rel_rtype == M_R_REGISTER) && !sdp) {
		relosp = ofl->ofl_osrel;
		relbits = (char *)relosp->os_outdata->d_buf;
		ndx = 0;

		rea.r_info = ELF_R_INFO(ndx, ELF_R_TYPE_INFO(
				orsp->rel_rextoffset,
				orsp->rel_rtype));
		rea.r_offset = orsp->rel_roffset;
		rea.r_addend = raddend;
		DBG_CALL(Dbg_reloc_out(M_MACH, SHT_RELA, &rea,
		    orsp->rel_sname, relosp->os_name));

		(void) memcpy((relbits + relosp->os_szoutrels),
			(char *)&rea, sizeof (Rela));
		relosp->os_szoutrels += (Xword)sizeof (Rela);

		return (1);
	}

	/*
	 * If this is a relocation against the
	 * Move table or expanded move table,
	 * adjust the relocation entries.
	 */
	if (orsp->rel_mventry) {
		if (orsp->rel_flags & FLG_REL_MOVETAB)
			adj_movereloc(ofl, orsp);
		else
			adj_expandreloc(ofl, orsp);
	}

	/*
	 * If this is a relocation against a section then we
	 * need to adjust the raddend field to compensate
	 * for the new position of the input section within
	 * the new output section.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
		    (psym = am_I_partial(orsp))) {
			/*
			 * If the symbol is moved, adjust the value
			 */
			DBG_CALL(Dbg_move_outsctadj(psym));
			sectmoved = 1;
			if (ofl->ofl_flags & FLG_OF_RELOBJ)
				raddend = psym->sd_sym->st_value;
			else
				raddend = psym->sd_sym->st_value -
				psym->sd_isc->is_osdesc->os_shdr->sh_addr;
			raddend += (Off)_elf_getxoff(psym->sd_isc->is_indata);
			if (psym->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				psym->sd_isc->is_osdesc->os_shdr->sh_addr;
		} else {
			raddend += (Off)_elf_getxoff(sdp->sd_isc->is_indata);
			if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
				raddend +=
				sdp->sd_isc->is_osdesc->os_shdr->sh_addr;
		}
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		Gotndx * gnp;
		osp = ofl->ofl_osgot;
		gnp = find_gotndx(&sdp->sd_GOTndxs, orsp->rel_reloc);
		roffset = (Xword) (osp->os_shdr->sh_addr) +
		    (-negative_got_offset * M_GOT_ENTSIZE) +
		    (gnp->gn_gotndx * M_GOT_ENTSIZE);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		osp = ofl->ofl_osplt;
		roffset = (Xword) (osp->os_shdr->sh_addr) +
		    M_PLT_RESERVSZ +
		    ((sdp->sd_aux->sa_PLTndx - 1) * M_PLT_ENTSIZE);
		plt_entry(ofl, sdp->sd_aux->sa_PLTndx);
	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * this must be a R_SPARC_COPY - for those
		 * we also set the roffset to point to the
		 * new symbols location.
		 *
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Xword)value;
		/*
		 * the raddend doesn't mean anything in an
		 * R_SPARC_COPY relocation.  We will null
		 * it out because it can be confusing to
		 * people.
		 */
		raddend = 0;
	} else if (orsp->rel_flags & FLG_REL_REG) {
		/*
		 * The offsets of relocations against register symbols
		 * identifiy the register directly - so the offset
		 * does not need to be adjusted.
		 */
		roffset = orsp->rel_roffset;
	} else {
		osp = orsp->rel_osdesc;
		/*
		 * Calculate virtual offset of reference point;
		 * equals offset into section + vaddr of section
		 * for loadable sections, or offset plus
		 * section displacement for nonloadable
		 * sections.
		 */
		roffset = orsp->rel_roffset +
		    (Off)_elf_getxoff(orsp->rel_isdesc->is_indata);
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((osp == 0) || ((relosp = osp->os_relosdesc) == 0))
		relosp = ofl->ofl_osrel;


	/*
	 * Verify that the output relocations offset meets the
	 * alignment requirements of the relocation being processed.
	 */
	rep = &reloc_table[orsp->rel_rtype];
	if ((rep->re_flags & FLG_RE_UNALIGN) == 0) {
		if (((rep->re_fsize == 2) && (roffset & 0x1)) ||
		    ((rep->re_fsize == 4) && (roffset & 0x3)) ||
		    ((rep->re_fsize == 8) && (roffset & 0x7))) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NONALIGN),
			    conv_reloc_SPARC_type_str(orsp->rel_rtype),
			    orsp->rel_fname, orsp->rel_sname,
			    EC_XWORD(roffset));
			return (S_ERROR);
		}
	}


	/*
	 * assign the symbols index for the output
	 * relocation.  If the relocation refers to a
	 * SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise
	 * the index can be derived from the symbols index
	 * itself.
	 */
	if (orsp->rel_rtype == R_SPARC_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
		if (sectmoved == 0) {
			/*
			 * Check for a null input section. This can
			 * occur if this relocation references a symbol
			 * generated by sym_add_sym().
			 */
			if ((sdp->sd_isc != 0) &&
			    (sdp->sd_isc->is_osdesc != 0))
				ndx = sdp->sd_isc->is_osdesc->os_scnsymndx;
			else
				ndx = sdp->sd_sym->st_shndx;
		} else
			ndx = ofl->ofl_sunwdata1ndx;
	} else
		ndx = sdp->sd_symndx;

	/*
	 * Add the symbols 'value' to the addend field.
	 */
	if (orsp->rel_flags & FLG_REL_ADVAL)
		raddend += value;

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, ELF_R_TYPE_INFO(orsp->rel_rextoffset,
			orsp->rel_rtype));
	rea.r_offset = roffset;
	rea.r_addend = raddend;
	DBG_CALL(Dbg_reloc_out(M_MACH, SHT_RELA, &rea,
	    orsp->rel_sname, relosp->os_name));

	(void) memcpy((relbits + relosp->os_szoutrels),
		(char *)&rea, sizeof (Rela));
	relosp->os_szoutrels += (Xword)sizeof (Rela);

	/*
	 * Determine whether this relocation is against a
	 * non-writeable, allocatable section.  If so we may
	 * need to provide a text relocation diagnostic.
	 */
	reloc_remain_entry(orsp, osp, ofl);

	return (1);
}

uintptr_t
do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc *	arsp;
	Rel_cache *	rcp;
	Listnode *	lnp;
	uintptr_t	return_code = 1;


	DBG_CALL(Dbg_reloc_doactiverel());
	/*
	 * process active relocs
	 */
	for (LIST_TRAVERSE(&ofl->ofl_actrels, lnp, rcp)) {
		/*LINTED*/
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			unsigned char	*addr;
			Xword		value;
			Sym_desc *	sdp;
			const char	*ifl_name;
			Xword		refaddr = arsp->rel_roffset +
					    (Off)_elf_getxoff(arsp->rel_isdesc->
						is_indata);
			sdp = arsp->rel_sym;

			if (arsp->rel_isdesc->is_file)
				ifl_name = arsp->rel_isdesc->is_file->ifl_name;
			else
				ifl_name = MSG_INTL(MSG_STR_NULL);

			/*
			 * If this is a relocation against the
			 * Move table or expanded move table,
			 * adjust the relocation entries.
			 */
			if (arsp->rel_mventry) {
				if (arsp->rel_flags & FLG_REL_MOVETAB)
					adj_movereloc(ofl, arsp);
				else
					adj_expandreloc(ofl, arsp);
			}

			if ((arsp->rel_flags & FLG_REL_CLVAL) ||
			    (arsp->rel_flags & FLG_REL_GOTCL))
				value = 0;
			else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				Sym_desc *	sym;
				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 */
				if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
				    (sym = am_I_partial(arsp))) {
				    DBG_CALL(Dbg_move_actsctadj(sym));
					/*
					 * If the symbol is moved,
					 * adjust the value
					 */
				    value = (Off)_elf_getxoff(sym->sd_isc->
					is_indata);
				    if (sym->sd_isc->is_shdr->sh_flags &
					SHF_ALLOC)
					value += sym->sd_isc->is_osdesc->
					os_shdr->sh_addr;
				} else {
				    value = (Off)_elf_getxoff(sdp->sd_isc->
					    is_indata);
				    if (sdp->sd_isc->is_shdr->sh_flags &
					SHF_ALLOC)
					value += sdp->sd_isc->is_osdesc->
					    os_shdr->sh_addr;
				}
			} else
				/*
				 * else the value is the symbols value
				 */
				value = sdp->sd_sym->st_value;

			/*
			 * relocation against the GLOBAL_OFFSET_TABLE
			 */
			if (arsp->rel_flags & FLG_REL_GOT)
				arsp->rel_osdesc = ofl->ofl_osgot;

			/*
			 * If loadable and not producing a relocatable object
			 * add the sections virtual address to the reference
			 * address.
			 */
			if ((arsp->rel_flags & FLG_REL_LOAD) &&
			    !(ofl->ofl_flags & FLG_OF_RELOBJ))
				refaddr += arsp->rel_isdesc->is_osdesc->
				    os_shdr->sh_addr;

			/*
			 * If this entry has a PLT assigned to it, it's
			 * value is actually the address of the PLT (and
			 * not the address of the function).
			 */
			if (IS_PLT(arsp->rel_rtype)) {
				if (sdp->sd_aux && sdp->sd_aux->sa_PLTndx)
					value = (Xword)(ofl->ofl_osplt->
					    os_shdr->sh_addr) +
					    M_PLT_RESERVSZ +
					    ((sdp->sd_aux->sa_PLTndx - 1) *
					    M_PLT_ENTSIZE);
			}

			/*
			 * add relocations addend to value.
			 */
			value += arsp->rel_raddend;

			/*
			 * add extra relocation addend if needed.
			 */
			if (IS_EXTOFFSET(arsp->rel_rtype)) {
				value += arsp->rel_rextoffset;
			}

			if (arsp->rel_flags & FLG_REL_GOT) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Gotndx *	gnp;
				/*
				 * Clear the GOT table entry, on SPARC
				 * we clear the entry and the 'value' if
				 * needed is stored in an output relocations
				 * addend.
				 */

				/*
				 * calculate offset into GOT at which to apply
				 * the relocation.
				 */
				gnp = find_gotndx(&sdp->sd_GOTndxs,
					arsp->rel_reloc);
				/* LINTED */
				R1addr = (Xword)((char *)(-negative_got_offset *
				    M_GOT_ENTSIZE) + (gnp->gn_gotndx
				    * M_GOT_ENTSIZE));

				/*
				 * add the GOTs data's offset
				 */
				R2addr = R1addr + (uintptr_t)
				    arsp->rel_osdesc->os_outdata->d_buf;

				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, arsp->rel_osdesc));

				/*
				 * and do it.
				 */
				*(Xword *)R2addr = value;
				continue;
			} else if (IS_PC_RELATIVE(arsp->rel_rtype)) {
				value -= refaddr;
			} else if (IS_GOT_RELATIVE(arsp->rel_rtype)) {
				Gotndx *	gnp;
				gnp = find_gotndx(&sdp->sd_GOTndxs,
					arsp->rel_reloc);
				value = gnp->gn_gotndx * M_GOT_ENTSIZE;
			}


			/*
			 * Make sure we have data to relocate.  Our compiler
			 * and assembler developers have been known
			 * to generate relocations against invalid sections
			 * (normally .bss), so for their benefit give
			 * them sufficient information to help analyze
			 * the problem.  End users should probably
			 * never see this.
			 */
			if (arsp->rel_isdesc->is_indata->d_buf == 0) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_SPARC_type_str(arsp->rel_rtype),
				    ifl_name,
				    arsp->rel_sname, arsp->rel_isdesc->is_name);
				return (S_ERROR);
			}

			/*
			 * Get the address of the data item we need to modify.
			 */
			addr = (unsigned char *)_elf_getxoff(arsp->rel_isdesc->
				is_indata) + arsp->rel_roffset;

			/*LINTED*/
			DBG_CALL(Dbg_reloc_doact(M_MACH, arsp->rel_rtype,
			    (Xword)addr, value, arsp->rel_sname,
			    arsp->rel_osdesc));
			addr += (uintptr_t)arsp->rel_osdesc->os_outdata->d_buf;

			if ((((uintptr_t)addr -  (uintptr_t)ofl->ofl_ehdr) >
			    ofl->ofl_size) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				int	warn_level;
				if (((uintptr_t)addr -
				    (uintptr_t)ofl->ofl_ehdr) > ofl->ofl_size)
					warn_level = ERR_FATAL;
				else
					warn_level = ERR_WARNING;

				eprintf(warn_level,
					MSG_INTL(MSG_REL_INVALOFFSET),
					conv_reloc_SPARC_type_str(
					arsp->rel_rtype),
					ifl_name,
					arsp->rel_isdesc->is_name,
					arsp->rel_sname,
					EC_ADDR((uintptr_t)addr -
					    (uintptr_t)ofl->ofl_ehdr));

				if (warn_level == ERR_FATAL) {
					return_code = S_ERROR;
					continue;
				}
			}

			if (do_reloc((unsigned char)arsp->rel_rtype, addr,
			    &value, arsp->rel_sname,
			    ifl_name) == 0)
				return_code = S_ERROR;
		}
	}
	return (return_code);
}


uintptr_t
add_outrel(Half flags, Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	Rel_desc *	orsp;
	Rel_cache *	rcp;
	Rela *		rloc = (Rela *)vrel;

	/*
	 * Static exectuables *do not* want any relocations against
	 * them.  Since our engine still creates relocations against
	 * a 'WEAK UNDEFINED' symbol in a static executable it's best
	 * to just disable them here instead of through out the relocation
	 * code.
	 */
	if ((ofl->ofl_flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC))
		return (1);

	/*
	 * Because the R_SPARC_HIPLT22 & R_SPARC_LOPLT10 relocations
	 * are not relative they would not make any sense if they
	 * were created in a shared object - so emit the proper error
	 * message if that occurs.
	 */
	if ((ofl->ofl_flags & FLG_OF_SHAROBJ) && ((rsp->rel_rtype ==
	    R_SPARC_HIPLT22) || (rsp->rel_rtype == R_SPARC_LOPLT10))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNRELREL),
			conv_reloc_SPARC_type_str(rsp->rel_rtype),
			rsp->rel_fname, rsp->rel_sname);
		return (S_ERROR);
	}

	/*
	 * If no relocation cache structures are available allocate
	 * a new one and link it into the cache list.
	 */
	if ((ofl->ofl_outrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_outrels.tail->data) == 0) ||
	    ((orsp = rcp->rc_free) == rcp->rc_end)) {
		if ((rcp = (Rel_cache *)libld_malloc(sizeof (Rel_cache) +
		    (sizeof (Rel_desc) * REL_OIDESCNO))) == 0)
			return (S_ERROR);
		/*LINTED*/
		rcp->rc_free = orsp = (Rel_desc *)(rcp + 1);
		rcp->rc_end = (Rel_desc *)((long)rcp->rc_free +
				(sizeof (Rel_desc) * REL_OIDESCNO));
		if (list_appendc(&ofl->ofl_outrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*orsp = *rsp;
	orsp->rel_flags |= flags;
	orsp->rel_raddend = rloc->r_addend;
	orsp->rel_reloc = (Rel *)rloc;
	/*
	 * Look at ../relocate.c:process_movereloc()
	 */
	if ((orsp->rel_mventry == (Move *)NULL) ||
	    (orsp->rel_flags & FLG_REL_MOVETAB))
		orsp->rel_roffset = rloc->r_offset;
	/* LINTED */
	orsp->rel_rextoffset = (Word)ELF_R_TYPE_DATA(rloc->r_info);
	rcp->rc_free++;

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)sizeof (Rela);
	else if (flags & FLG_REL_NOINFO)
		ofl->ofl_relocrelsz += (Xword)sizeof (Rela);
	else
		rsp->rel_osdesc->os_szoutrels += (Xword)sizeof (Rela);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * When building a 64-bit object any R_SPARC_WDISP30 relocation is given
	 * a plt padding entry, unless we're building a relocatable object
	 * (ld -r) or -b is in effect.
	 */
#ifdef	_ELF64
	if ((orsp->rel_rtype == R_SPARC_WDISP30) &&
	    ((ofl->ofl_flags & (FLG_OF_BFLAG | FLG_OF_RELOBJ)) == 0) &&
	    ((orsp->rel_sym->sd_flags & FLG_SY_PLTPAD) == 0)) {
		ofl->ofl_pltpad++;
		orsp->rel_sym->sd_flags |= FLG_SY_PLTPAD;
	}
#endif

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if we
	 * were to sort them we would have to re-assign the plt indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	DBG_CALL(Dbg_reloc_ors_entry(M_MACH, orsp));
	return (1);
}


uintptr_t
add_actrel(Half flags, Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	Rel_desc * 	arsp;
	Rel_cache *	rcp;
	Rela *		rloc = (Rela *)vrel;

	/*
	 * If no relocation cache structures are available allocate a
	 * new one and link it into the bucket list.
	 */
	if ((ofl->ofl_actrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_actrels.tail->data) == 0) ||
	    ((arsp = rcp->rc_free) == rcp->rc_end)) {
		if ((rcp = (Rel_cache *)libld_malloc(sizeof (Rel_cache) +
			(sizeof (Rel_desc) * REL_AIDESCNO))) == 0)
				return (S_ERROR);
		/*LINTED*/
		rcp->rc_free = arsp = (Rel_desc *)(rcp + 1);
		rcp->rc_end = (Rel_desc *)((long)rcp->rc_free +
				(sizeof (Rel_desc) * REL_AIDESCNO));
		if (list_appendc(&ofl->ofl_actrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*arsp = *rsp;
	arsp->rel_flags |= flags;
	arsp->rel_raddend = rloc->r_addend;
	arsp->rel_reloc = (Rel *)rloc;
	/*
	 * Look at ../relocate.c:process_movereloc()
	 */
	if ((arsp->rel_mventry == (Move *)NULL) ||
	    (arsp->rel_flags & FLG_REL_MOVETAB))
		arsp->rel_roffset = rloc->r_offset;
	/* LINTED */
	arsp->rel_rextoffset = (Word)ELF_R_TYPE_DATA(rloc->r_info);
	rcp->rc_free++;

	DBG_CALL(Dbg_reloc_ars_entry(M_MACH, arsp));
	return (1);
}


/*
 * process relocation against a REGISTER symbol
 */
uintptr_t
reloc_register(Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	return (add_outrel(rsp->rel_flags | FLG_REL_REG, rsp, vrel, ofl));
}


/*
 * stabe routine since FPTR's are not valied on SPARC
 */
/* ARGSUSED */
uintptr_t
reloc_local_fptr(Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	return (1);
}

/*
 * process relocation for a LOCAL symbol
 */
uintptr_t
reloc_local(Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	Word		flags = ofl->ofl_flags;
	Rela *		reloc = (Rela *)vrel;

	/*
	 * If ((shared object) and (not pc relative relocation))
	 * then
	 *	if (rtype != R_SPARC_32)
	 *	then
	 *		build relocation against section
	 *	else
	 *		build R_SPARC_RELATIVE
	 *	fi
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype))) {
		Word	ortype = rsp->rel_rtype;
		if ((rsp->rel_rtype != R_SPARC_32) &&
		    (rsp->rel_rtype != R_SPARC_PLT32) &&
		    (rsp->rel_rtype != R_SPARC_64))
			return (add_outrel(FLG_REL_SCNNDX |
			    FLG_REL_ADVAL, rsp, reloc, ofl));

		rsp->rel_rtype = R_SPARC_RELATIVE;
		if (add_outrel(FLG_REL_ADVAL, rsp, reloc, ofl) ==
		    S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
		return (1);
	}

	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    (rsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF)) {
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_SPARC_type_str(rsp->rel_rtype), rsp->rel_fname,
		    rsp->rel_sname, rsp->rel_osdesc->os_name);
		return (1);
	}
	/*
	 * just do it.
	 */
	return (add_actrel(NULL, rsp, reloc, ofl));
}

uintptr_t
reloc_relobj(Boolean local, Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc *	sdp = rsp->rel_sym;
	Is_desc *	isp = rsp->rel_isdesc;
	Word		flags = ofl->ofl_flags;
	Rela *		reloc = (Rela *)vrel;

	/*
	 * Try to determine if we can do any relocations at
	 * this point.  We can if:
	 *
	 * (local_symbol) and (non_GOT_relocation) and
	 * (IS_PC_RELATIVE()) and
	 * (relocation to symbol in same section)
	 */
	if (local && !IS_GOT_RELATIVE(rtype) && IS_PC_RELATIVE(rtype) &&
	    ((sdp->sd_isc) && (sdp->sd_isc->is_osdesc == isp->is_osdesc)))
		return (add_actrel(NULL, rsp, reloc, ofl));

	/*
	 * If '-zredlocsym' is in effect make all local sym relocations
	 * against the 'section symbols', since they are the only symbols
	 * which will be added to the .symtab.
	 */
	if (local &&
	    (((ofl->ofl_flags1 & FLG_OF1_REDLSYM) &&
	    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL)) ||
	    ((sdp->sd_flags & FLG_SY_ELIM) && (flags & FLG_OF_PROCRED)))) {
		/*
		 * But if this is a PIC code, don't allow it for now.
		 */
		if (IS_GOT_RELATIVE(rsp->rel_rtype)) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_PICREDLOC),
				rsp->rel_sname,
				rsp->rel_isdesc->is_file->ifl_name,
				conv_reloc_type_str(ofl->ofl_e_machine,
					rsp->rel_rtype));
			return (S_ERROR);
		}
		return (add_outrel(FLG_REL_SCNNDX | FLG_REL_ADVAL,
			rsp, reloc, ofl));
	}

	return (add_outrel(NULL, rsp, reloc, ofl));
}


/*
 * allocate_got: if a GOT is to be made, after the section is built this
 * function is called to allocate all the GOT slots.  The allocation is
 * deferred until after all GOTs have been counted and sorted according
 * to their size, for only then will we know how to allocate them on
 * a processor like SPARC which has different models for addressing the
 * GOT.  SPARC has two: small and large, small uses a signed 13-bit offset
 * into the GOT, whereas large uses an unsigned 32-bit offset.
 */
static	Xword small_index;	/* starting index for small GOT entries */
static	Xword large_index;	/* starting index for large GOT entries */

uintptr_t
assign_got(Sym_desc * sdp)
{
	Listnode *	lnp;
	Gotndx *	gnp;

	for (LIST_TRAVERSE(&sdp->sd_GOTndxs, lnp, gnp)) {
		switch (gnp->gn_gotndx) {
		case M_GOT_SMALL:
			gnp->gn_gotndx = small_index++;
			if (small_index == 0)
				small_index = M_GOT_XNumber;
			break;
		case M_GOT_LARGE:
			gnp->gn_gotndx = large_index++;
			break;
		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_ASSIGNGOT),
			    EC_XWORD(gnp->gn_gotndx), sdp->sd_name);
			return (S_ERROR);
		}
	}
	return (1);
}


/*
 * Search the GOT index list to see if there is a GOT entry with the
 * proper addend
 */
Gotndx *
find_gotndx(List * lst, void * vrel)
{
	Xword		raddend;
	Listnode *	lnp;
	Gotndx *	gnp;
	Rela *		reloc = (Rela *)vrel;

	raddend = reloc->r_addend;
	for (LIST_TRAVERSE(lst, lnp, gnp)) {
		if (raddend == gnp->gn_addend)
			return (gnp);
	}
	return ((Gotndx *)0);
}

uintptr_t
assign_got_ndx(List * lst, Rel_desc * rsp, void * vrel, Sym_desc * sdp,
    Gotndx * pgnp, Ofl_desc * ofl)
{
	Xword		raddend;
	Gotndx *	gnp, * _gnp;
	Listnode *	lnp, * plnp;
	Rela *		reloc = (Rela *)vrel;

	raddend = reloc->r_addend;
	if (pgnp && (pgnp->gn_addend == raddend)) {
		/*
		 * If we already have an entry at this addend - then we
		 * need to see if it should be changed to a SMALL got.
		 */
		if ((pgnp->gn_gotndx != M_GOT_SMALL) &&
		    (rsp->rel_rtype == R_SPARC_GOT13)) {
			countSmallGOT++;
			pgnp->gn_gotndx = M_GOT_SMALL;
			sdp->sd_flags |= FLG_SY_SMGOT;
		}
		return (1);
	}

	plnp = 0;
	for (LIST_TRAVERSE(lst, lnp, _gnp)) {
		if (_gnp->gn_addend > raddend)
			break;
		plnp = lnp;
	}
	/*
	 * We need to alocate a new entry.
	 */
	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	gnp->gn_addend = reloc->r_addend;
	ofl->ofl_gotcnt++;
	if (rsp->rel_rtype == R_SPARC_GOT13) {
		gnp->gn_gotndx = M_GOT_SMALL;
		countSmallGOT++;
		sdp->sd_flags |= FLG_SY_SMGOT;
	} else
		gnp->gn_gotndx = M_GOT_LARGE;

	if (plnp == 0) {
		/*
		 * Insert at head of list
		 */
		if (list_prependc(lst, (void *)gnp) == 0)
			return (S_ERROR);
	} else if (_gnp->gn_addend > raddend) {
		/*
		 * Insert in middle of lest
		 */
		if (list_insertc(lst, (void *)gnp, plnp) == 0)
			return (S_ERROR);
	} else {
		/*
		 * Append to tail of list
		 */
		if (list_appendc(lst, (void *)gnp) == 0)
			return (S_ERROR);
	}
	return (1);
}

void
assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
}


uintptr_t
allocate_got(Ofl_desc * ofl)
{
	Sym_desc *	sdp;
	Addr		addr;

	/*
	 * Sanity check -- is this going to fit at all?
	 */
	if (countSmallGOT >= M_GOT_MAXSMALL) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_SMALLGOT),
		    EC_WORD(countSmallGOT), M_GOT_MAXSMALL);
		return (S_ERROR);
	}

	/*
	 * Set starting offset to be either 0, or a negative index into
	 * the GOT based on the number of small symbols we've got.
	 */
	negative_got_offset = countSmallGOT > (M_GOT_MAXSMALL / 2) ?
	    -((Sxword)((countSmallGOT - (M_GOT_MAXSMALL / 2)))) : 0;

	/*
	 * Initialize the large and small got offsets (used in assign_got()).
	 */
	small_index = negative_got_offset == 0 ?
	    M_GOT_XNumber : negative_got_offset;
	large_index = negative_got_offset + countSmallGOT;

	/*
	 * Assign bias to GOT symbols.
	 */
	addr = -negative_got_offset * M_GOT_ENTSIZE;
	if (sdp = sym_find(MSG_ORIG(MSG_SYM_GOFTBL), SYM_NOHASH, ofl))
		sdp->sd_sym->st_value = addr;
	if (sdp = sym_find(MSG_ORIG(MSG_SYM_GOFTBL_U), SYM_NOHASH, ofl))
		sdp->sd_sym->st_value = addr;
	return (1);
}


/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
void
fillin_gotplt1(Ofl_desc * ofl)
{
	Sym_desc *	sdp;

	if (ofl->ofl_osgot) {
		unsigned char	*genptr;

		if ((sdp = sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, ofl)) != NULL) {
			genptr = ((unsigned char *)
			    ofl->ofl_osgot->os_outdata->d_buf +
			    (-negative_got_offset * M_GOT_ENTSIZE) +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*((Xword *)genptr) = sdp->sd_sym->st_value;
		}
	}
}


/*
 * Return plt[0].
 */
Addr
fillin_gotplt2(Ofl_desc * ofl)
{
	if (ofl->ofl_osplt)
		return (ofl->ofl_osplt->os_shdr->sh_addr);
	else
		return (0);
}
