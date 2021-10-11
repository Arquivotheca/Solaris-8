/*
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)machrel.c	1.4	99/09/14 SMI"

/* LINTLIBRARY */

#include	<string.h>
#include	<assert.h>
#include	<sys/elf_ia64.h>
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
	Os_desc *osp = ofl->ofl_osdynamic;

	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * On IA64 we always create the DT_PLTGOT entry
		 * as it will be initialized to the value of the GP
		 * for this module.
		 */
		(*cnt)++;		/* DT_PLTGOT */
		if ((osp->os_sgdesc) &&
		    (osp->os_sgdesc->sg_phdr.p_flags & PF_W)) {
			(*cnt)++;	/* DT_IA_64_PLT_RESERVE */
		}
	}
}

void
mach_update_odynamic(Ofl_desc *ofl, Dyn **dyn)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		(*dyn)->d_tag = DT_PLTGOT;
		(*dyn)->d_un.d_ptr = ofl->ofl_osgot->os_shdr->sh_addr;
		(*dyn)++;

		if (ofl->ofl_osdynamic->os_sgdesc &&
		    (ofl->ofl_osdynamic->os_sgdesc->sg_phdr.p_flags & PF_W)) {
			(*dyn)->d_tag = DT_IA_64_PLT_RESERVE;
			(*dyn)->d_un.d_ptr = ofl->ofl_osgot->os_shdr->sh_addr;
			(*dyn)++;
		}
	}
}


/*
 * When processing sections of type SHT_IA_64_UNWIND we must
 * make sure that they are all grouped together (this is done
 * via the ident) and that we remember the first and the last
 * of these sections.  We will use these sections to create
 * the PT_IA_64_UNWIND program header.
 */
uintptr_t
process_ia64_unwind(const char *name, Ifl_desc *ifl, Shdr *shdr, Elf_Scn *scn,
	Word ndx, int ident, Ofl_desc *ofl)
{
	uintptr_t	error;
	Is_desc *	isp;

	error = process_section(name, ifl, shdr, scn, ndx, ident, ofl);
	if ((error == 0) || (error == S_ERROR))
		return (error);

	isp = ifl->ifl_isdesc[ndx];
	if (ofl->ofl_osunwind == 0)
		ofl->ofl_osunwind = isp->is_osdesc;
	ofl->ofl_osendunwind = isp->is_osdesc;

	return (error);
}


static unsigned int pltx_template[] = {
/* 0x00 */	0x0000000d, 0x00000001, 0xe0000200, 0x90000801,
		/*
		 * .PLTx:
		 * {
		 *	nop
		 *	nop
		 *	addl	r15 = @pltoff(name1), gp
		 * }
		 */
/* 0x10 */	0x1e20800d, 0x00001418, 0xc0000200, 0x84000801,
		/*
		 * {
		 *	ld8	r16, [r15], 0x8
		 *	nop
		 *	mov	r14 = gp
		 * }
		 */
/* 0x20 */	0x1e000810, 0x80601018, 0x00038004, 0x00800060,
		/*
		 * {
		 *	ld8	gp = [r15]
		 *	mov	b6 = r16
		 *	br	b6
		 * }
		 */
/* 0x30 */	0x00000010, 0x00f00001, 0x00480000, 0x40000000
		/*
		 * .PLTxa:
		 * {
		 *	nop
		 *	mov	r15, <reloc_index>
		 *	br	.PLT0
		 * }
		 */
};

/*
 * Build a single plt entry based off of the pltx_template[] above.
 */
void
plt_entry(Ofl_desc *ofl, Sym_desc *sdp)
{
	unsigned char	*pltent, *gotent;
	Addr		plt_off;
	Addr		plt0_addr;
	Xword		value;
	Xword		pltndx = sdp->sd_aux->sa_PLTndx - 1;
	Xword		got_off;

	plt_off = M_PLT_RESERVSZ + (pltndx * M_PLT_ENTSIZE);
	got_off = sdp->sd_FUNCndx * M_GOT_ENTSIZE;

	plt0_addr = (Addr)(ofl->ofl_osplt->os_outdata->d_buf);
	pltent = (unsigned char *)(plt0_addr) + plt_off;
	memcpy(pltent, pltx_template, sizeof (pltx_template));
	/*
	 * Perform PLTOFF22 relocation against:
	 * .PLTx:
	 *	addl r15 = *pltoff(name1), gp
	 */
	value = got_off;
	(void) do_reloc(R_IA_64_PLTOFF22, (unsigned char *)pltent +0x2,
		&value, "null_sym", "null_file");

	/*
	 * Perform IMM22 relocatoin against:
	 *	.PLTxa:
	 *		move	r15, <reloc_index>
	 */
	value = pltndx;
	(void) do_reloc(R_IA_64_IMM22, (unsigned char *)pltent + 0x31,
		&value, "null_sym", "null_file");
	/*
	 * Perform PCREL21B relocation against:
	 *	.PLTxa:
	 *		br	.PLT0
	 */
	value = (Xword)(plt0_addr - (Xword)(pltent + 0x30));
	(void) do_reloc(R_IA_64_PCREL21B, (unsigned char *)pltent + 0x32,
		&value, "null_sym", "null_file");

	gotent = (unsigned char *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;

	/*
	 * Fill in the Function Descriptor with the address of .PLTxa
	 */
	*(Addr *)gotent = (Addr)(plt_off +
		ofl->ofl_osplt->os_shdr->sh_addr + 0x30);
}


/*
 * Partially Initialized Symbol Handling routines
 */
static Sym_desc *
am_I_partial(Ofl_desc *ofl, Rel_desc *reld)
{
	Sym_desc	*symd;
	Ifl_desc	*ifile;
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
perform_outreloc(Rel_desc *orsp, Ofl_desc *ofl)
{
	Os_desc			*osp;		/* output section */
	Os_desc			*relosp;	/* reloc output section */
	Xword			ndx;		/* sym & scn index */
	Xword			roffset;	/* roffset for output rel */
	Xword			value;
	Sxword			raddend;	/* raddend for output rel */
	char			*relbits;
	Sym_desc		*sdp;		/* current relocation sym */
	const Rel_entry		*rep;
	Sym_desc		*psym;		/* Partially init. sym */
	int			sectmoved;
	Word			relshtype;	/* type of output rels */

	if (ofl->ofl_flags & FLG_OF_RELOBJ)
		relshtype = M_OBJREL_SHT_TYPE;
	else
		relshtype = M_DYNREL_SHT_TYPE;

	psym = (Sym_desc *) NULL;
	raddend = orsp->rel_raddend;
	sdp = orsp->rel_sym;
	sectmoved = 0;

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
		    (psym = am_I_partial(ofl, orsp))) {
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
	} else if (orsp->rel_flags & (FLG_REL_RFPTR1 | FLG_REL_RFPTR2)) {
		uint_t	fndx = sdp->sd_FUNCndx;

		if (orsp->rel_flags & FLG_REL_RFPTR2)
			fndx++;
		osp = ofl->ofl_osgot;
		roffset = (Xword) (osp->os_shdr->sh_addr) +
		    (-negative_got_offset * M_GOT_ENTSIZE) +
		    (fndx * M_GOT_ENTSIZE);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		osp = ofl->ofl_osplt;
		roffset = (sdp->sd_FUNCndx * M_GOT_ENTSIZE) +
			ofl->ofl_osgot->os_shdr->sh_addr;
		plt_entry(ofl, sdp);
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
			    conv_reloc_ia64_type_str(orsp->rel_rtype),
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
	if (orsp->rel_rtype == R_IA_64_REL64LSB)
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

	if (relshtype == SHT_RELA) {
		Rela	rela;
		rela.r_info = ELF_R_INFO(ndx, ELF_R_TYPE_INFO(
			orsp->rel_rextoffset, orsp->rel_rtype));

		rela.r_offset = roffset;
		rela.r_addend = raddend;
		DBG_CALL(Dbg_reloc_out(M_MACH, relshtype, &rela,
		    orsp->rel_sname, relosp->os_name));

		(void) memcpy(relbits + relosp->os_szoutrels,
			(char *)&rela, sizeof (Rela));
		relosp->os_szoutrels += (Xword)sizeof (Rela);
	} else {
		Rel	rel;
		rel.r_info = ELF_R_INFO(ndx, ELF_R_TYPE_INFO(
			orsp->rel_rextoffset, orsp->rel_rtype));

		rel.r_offset = roffset;
		DBG_CALL(Dbg_reloc_out(M_MACH, relshtype, &rel,
		    orsp->rel_sname, relosp->os_name));

		(void) memcpy((relbits + relosp->os_szoutrels),
			(char *)&rel, sizeof (Rel));
		relosp->os_szoutrels += (Xword)sizeof (Rel);
	}

	/*
	 * Determine whether this relocation is against a
	 * non-writeable, allocatable section.  If so we may
	 * need to provide a text relocation diagnostic.
	 */
	if (orsp->rel_rtype == R_IA_64_IPLTLSB)
		osp = ofl->ofl_osgot;
	reloc_remain_entry(orsp, osp, ofl);

	return (1);
}

uintptr_t
do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc	*arsp;
	Rel_cache	*rcp;
	Listnode	*lnp;
	uintptr_t	return_code = 1;


	DBG_CALL(Dbg_reloc_doactiverel());
	/*
	 * process active relocs
	 */
	for (LIST_TRAVERSE(&ofl->ofl_actrels, lnp, rcp)) {
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			unsigned char	*addr;
			Xword		value;
			Sym_desc	*sdp;
			const char	*ifl_name;
			Xword		refaddr;
			int		rtype = arsp->rel_rtype;

			/*
			 * Some relocations have the islot location
			 * encoded in the offset - strip that out
			 * when computing the refaddr.
			 */
			if (IS_FORMOFF(arsp->rel_rtype))
				refaddr = arsp->rel_roffset & (~0x3);
			else
				refaddr = arsp->rel_roffset;

			refaddr += (Off)_elf_getxoff(arsp->rel_isdesc->
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

			if (arsp->rel_flags & FLG_REL_CLVAL) {
				value = 0;
			} else if (IS_FPTR(arsp->rel_rtype) &&
			    !(arsp->rel_flags & FLG_REL_FPTR)) {
				value = (sdp->sd_FUNCndx * M_GOT_ENTSIZE) +
					ofl->ofl_osgot->os_shdr->sh_addr;
			} else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				Sym_desc *	sym;
				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 */
				if ((sdp->sd_isc->is_flags & FLG_IS_RELUPD) &&
				    (sym = am_I_partial(ofl, arsp))) {
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
			} else {
				/*
				 * else the value is the symbols value
				 */
				value = sdp->sd_sym->st_value;
			}

			/*
			 * relocation against the GLOBAL_OFFSET_TABLE
			 */
			if (arsp->rel_flags & (FLG_REL_GOT | FLG_REL_FPTR))
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


			if (arsp->rel_flags & FLG_REL_GOT) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Gotndx *	gnp;

				/*
				 * calculate offset into GOT at which to apply
				 * the relocation.
				 */
				gnp = find_gotndx(&sdp->sd_GOTndxs,
					arsp->rel_reloc);
				R1addr = (Xword)((char *)
				    (gnp->gn_gotndx * M_GOT_ENTSIZE));

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
			} else if (arsp->rel_flags & FLG_REL_FPTR) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Xword		gotaddr;
				/*
				 * Initialized the function descriptor
				 */

				/*
				 * The first entry is the Function Pointer.
				 */
				R1addr = (Xword)((char *)(sdp->sd_FUNCndx *
				    M_GOT_ENTSIZE));
				/*
				 * add the GOTs data's offset
				 */
				R2addr = R1addr + (uintptr_t)
				    arsp->rel_osdesc->os_outdata->d_buf;
				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, ofl->ofl_osgot));
				*(Xword *)R2addr = value;

				/*
				 * Next comes the GP
				 */
				R1addr += M_GOT_ENTSIZE;
				R2addr += M_GOT_ENTSIZE;
				gotaddr = ofl->ofl_osgot->os_shdr->sh_addr;
				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, gotaddr,
				    MSG_ORIG(MSG_SYM_GOFTBL_U),
				    ofl->ofl_osgot));

				*(Xword *)R2addr = gotaddr;
				continue;
			} else if (IS_GOT_BASED(arsp->rel_rtype)) {
				value -= ofl->ofl_osgot->os_shdr->sh_addr;
			} else if (IS_PC_RELATIVE(arsp->rel_rtype)) {
				value -= refaddr;
			} else if (IS_SEG_RELATIVE(arsp->rel_rtype)) {
				value -= arsp->rel_osdesc->os_sgdesc->
					sg_phdr.p_vaddr;
			} else if (IS_SEC_RELATIVE(arsp->rel_rtype)) {
				value -= (Xword)(arsp->rel_osdesc->
						os_outdata->d_buf);
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
					(uintptr_t)addr -
					    (uintptr_t)ofl->ofl_ehdr);

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
add_outrel(Half flags, Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	Rela		*rloc = vrel;
	Rel_desc	*orsp;
	Rel_cache	*rcp;
	Word		relsize;	/* size of output relocations being */
					/* produced */


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
	 * If no relocation cache structures are available allocate
	 * a new one and link it into the cache list.
	 */
	if ((ofl->ofl_outrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_outrels.tail->data) == 0) ||
	    ((orsp = rcp->rc_free) == rcp->rc_end)) {
		if ((rcp = (Rel_cache *)libld_malloc(sizeof (Rel_cache) +
		    (sizeof (Rel_desc) * REL_OIDESCNO))) == 0)
			return (S_ERROR);
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

	/*
	 * Determine if we are outputing Rel or Rela relocations.
	 */
	if (ofl->ofl_flags & FLG_OF_RELOBJ) {
		relsize = sizeof (Rela);
	} else {
		relsize = sizeof (Rel);
		/*
		 * If we are creating Rel relocations - then
		 * create a active relocation to write the addend
		 * out to the file.
		 */
		if ((orsp->rel_raddend) && (orsp->rel_rtype != M_R_RELATIVE)) {
			/*
			 * By setting the CLVAL relocation we
			 * ignore the value *but* still add the
			 * addend to the output file (perfect).
			 */
			if (add_actrel(FLG_REL_CLVAL | flags, rsp, rloc,
			    ofl) == S_ERROR)
				return (S_ERROR);
		}
	}

	if (flags & (FLG_REL_GOT | FLG_REL_RFPTR1 | FLG_REL_RFPTR2))
		ofl->ofl_relocgotsz += (Xword)relsize;
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += (Xword)relsize;
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += (Xword)relsize;
	else
		rsp->rel_osdesc->os_szoutrels += (Xword)relsize;

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

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
add_actrel(Half flags, Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	Rela		*rloc = vrel;
	Rel_desc	*arsp;
	Rel_cache	*rcp;

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
 * stub routine since register symbols are not allowed on IA64
 */
/* ARGSUSED0 */
uintptr_t
reloc_register(Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOREG));
	return (S_ERROR);
}

void
assign_function_index(Sym_desc *sdp, Ofl_desc *ofl)
{
	if (sdp->sd_FUNCndx != -1)
		return;

	sdp->sd_FUNCndx = ofl->ofl_gotcnt;
	/*
	 * On IA64 the FD takes two GOT entries, the first
	 * is for the Function Pointer, the second is for
	 * the %gp of the destination object.
	 */
	ofl->ofl_gotcnt += 2;
}

uintptr_t
reloc_local_fptr(Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	Rela		*reloc = vrel;
	Word		flags = ofl->ofl_flags;

	/*
	 * If this is a locally bound symbol - then we must create
	 * a function descriptor (FD) for it.  If it is not locally
	 * bound then we are just dependent upon the FD we find
	 * at run-time.
	 */
	if (rsp->rel_sym->sd_FUNCndx == -1) {
		Word	rtype = rsp->rel_rtype;

		assign_function_index(rsp->rel_sym, ofl);

		if ((flags & FLG_OF_SHAROBJ) &&
		    !IS_SEG_RELATIVE(rtype)) {
			rsp->rel_rtype = M_R_RELATIVE;
			if (add_outrel(FLG_REL_RFPTR1 | FLG_REL_ADVAL,
			    rsp, reloc, ofl) == S_ERROR)
				return (S_ERROR);
			if (add_outrel(FLG_REL_RFPTR2 | FLG_REL_ADVAL,
			    rsp, reloc, ofl) == S_ERROR)
				return (S_ERROR);
			rsp->rel_rtype = rtype;
		}
		if (add_actrel(FLG_REL_FPTR, rsp, reloc,
		    ofl) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}


/*
 * process relocation for a LOCAL symbol
 */
uintptr_t
reloc_local(Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	Word		flags = ofl->ofl_flags;
	Rela		*reloc = (Rela *)vrel;

	if (IS_FPTR(rsp->rel_rtype))
		if (reloc_local_fptr(rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);

	if ((flags & FLG_OF_SHAROBJ) &&
	    (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_GOT_BASED(rsp->rel_rtype)) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype)) &&
	    !(IS_SEG_RELATIVE(rsp->rel_rtype))) {
		Word ortype = rsp->rel_rtype;
		rsp->rel_rtype = M_R_RELATIVE;
		if (add_outrel(NULL, rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
	}

	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    (rsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF)) {
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_SPARC_type_str(rsp->rel_rtype), rsp->rel_fname,
		    rsp->rel_sname, rsp->rel_osdesc->os_name);
		return (1);
	}
	/*
	 * perform relocation
	 */
	return (add_actrel(NULL, rsp, reloc, ofl));
}

uintptr_t
reloc_relobj(Boolean local, Rel_desc *rsp, void *vrel, Ofl_desc *ofl)
{
	Rela		*reloc = vrel;
	Word		rtype = rsp->rel_rtype;
	Sym_desc	*sdp = rsp->rel_sym;
	Is_desc		*isp = rsp->rel_isdesc;
	Word		flags = ofl->ofl_flags;

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
		return (add_outrel(FLG_REL_SCNNDX | FLG_REL_ADVAL,
			rsp, reloc, ofl));
	}

	return (add_outrel(NULL, rsp, reloc, ofl));
}


/*
 * Search the GOT index list to see if there is a
 * GOT entry with the proper addend.
 */
Gotndx *
find_gotndx(List *lst, void *vrel)
{
	Rela		*reloc = vrel;
	Xword		raddend;
	Listnode	*lnp;
	Gotndx		*gnp;

	raddend = reloc->r_addend;
	for (LIST_TRAVERSE(lst, lnp, gnp)) {
		if (raddend == gnp->gn_addend)
			return (gnp);
	}
	return ((Gotndx *)0);
}


/* ARGSUSED */
uintptr_t
assign_got_ndx(List *lst, Rel_desc *rsp, void *vrel, Sym_desc *sdp,
	Gotndx *pgnp, Ofl_desc *ofl)
{
	Rela		*reloc = vrel;
	Xword		raddend;
	Gotndx		*gnp, *_gnp;
	Listnode	*lnp, *plnp;

	if (pgnp)
		return (1);

	raddend = reloc->r_addend;

	plnp = 0;
	for (LIST_TRAVERSE(lst, lnp, _gnp)) {
		if (gnp->gn_addend > raddend)
			break;
		plnp = lnp;
	}
	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	/*
	 * A unique got entry is created for each unique Addend value
	 */
	raddend = gnp->gn_addend = reloc->r_addend;
	gnp->gn_gotndx = ofl->ofl_gotcnt++;

	if (plnp == 0) {
		/*
		 * Insert at head of list
		 */
		if (list_prependc(lst, (void *)gnp) == 0)
			return (S_ERROR);
	} else if (_gnp->gn_addend > raddend) {
		/*
		 * Insert in middle of list
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
assign_plt_ndx(Sym_desc *sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	assign_function_index(sdp, ofl);
}



uintptr_t
allocate_got(Ofl_desc *ofl)
{
	return (1);
}


static unsigned int plt0_template[] = {
/* 0x00 */	0x0000000d, 0x00000001, 0x40000200, 0x84007000,
		/*
		 * .PLT0:
		 * {
		 * 	nop
		 *	nop
		 *	mov	r2 = r14
		 * }
		 */
/* 0x10 */	0x0000000d, 0x00000001, 0xc0000200, 0x90001001,
		/*
		 * {
		 *	nop
		 *	nop
		 *	addl	r14 = @gprel(plt_reserve), r2
		 * }
		 *
		 * NOTE: since PLT_RESERVE is placed at gp[0] on
		 *	Solaris no additional update is needed to this
		 *	entry.  If it ever moves - a @gprel(plt_reserve)
		 *	relocation will have to be done to the above.
		 */
/* 0x20 */	0x1c20800d, 0x00001418, 0x00000200, 0x00040000,
		/*
		 * {
		 *	ld8	r16 = [r14], 0x8
		 *	nop
		 *	nop
		 * }
		 */
/* 0x30 */	0x1c20880d, 0x00001418, 0x00000200, 0x00040000,
		/*
		 * {
		 *	ld8	r17, [r14], 0x8
		 *	nop
		 *	nop
		 * }
		 */
/* 0x40 */	0x1c000810, 0x88601018, 0x00038004, 0x00800060
		/*
		 * {
		 *	ld8	gp = [r14]
		 *	mov	b6 = r17
		 *	br	b6
		 * }
		 */
};


/*
 * Initialize GOT[0] & PLT[0]
 */
void
fillin_gotplt1(Ofl_desc *ofl)
{
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		unsigned char *pltent;
		pltent = (unsigned char *)ofl->ofl_osplt->os_outdata->d_buf;
		memcpy(pltent, plt0_template, sizeof (plt0_template));
	}
}


/*
 * Return plt[0].
 */
Addr
fillin_gotplt2(Ofl_desc *ofl)
{
	if (ofl->ofl_osplt)
		return (ofl->ofl_osplt->os_shdr->sh_addr);
	else
		return (0);
}
