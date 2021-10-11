/*
 *	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF
 *	UNIX System Laboratories, Inc.
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)machrel.c	1.45	99/09/14 SMI"

#include	<string.h>
#include	<sys/elf_386.h>
#include	"debug.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_libld.h"


void
mach_make_dynamic(Ofl_desc *ofl, size_t *cnt)
{
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * DT_PLTGOT
		 *
		 * This entry is created on ia32 if we are going to
		 * create a PLT table
		 */
		if (ofl->ofl_pltcnt)
			(*cnt)++;		/* DT_PLTGOT */
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

/*
 *  Build a single plt entry - code is:
 *	if (building a.out)
 *		JMP	*got_off
 *	else
 *		JMP	*got_off@GOT(%ebx)
 *	PUSHL	&rel_off
 *	JMP	-n(%pc)		# -n is pcrel offset to first plt entry
 *
 *	The got_off@GOT entry gets filled with the address of the PUSHL,
 *	so the first pass through the plt jumps back here, jumping
 *	in turn to the first plt entry, which jumps to the dynamic
 *	linker.	 The dynamic linker then patches the GOT, rerouting
 *	future plt calls to the proper destination.
 */
static void
plt_entry(Ofl_desc * ofl, Word rel_off, Sym_desc * sdp)
{
	unsigned char	*pltent, *gotent;
	Sword		plt_off;
	Word		got_off;

	got_off = sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;
	plt_off = M_PLT_RESERVSZ + ((sdp->sd_aux->sa_PLTndx - 1) *
			M_PLT_ENTSIZE);
	pltent = (unsigned char *)(ofl->ofl_osplt->os_outdata->d_buf) + plt_off;
	gotent = (unsigned char *)(ofl->ofl_osgot->os_outdata->d_buf) + got_off;

	/*
	 * Fill in the got entry with the address of the next instruction.
	 */
	/* LINTED */
	*(Word *)gotent = ofl->ofl_osplt->os_shdr->sh_addr + plt_off +
		M_PLT_INSSIZE;

	if (!(ofl->ofl_flags & FLG_OF_SHAROBJ)) {
		pltent[0] = M_SPECIAL_INST;
		pltent[1] = M_JMP_DISP_IND;
		pltent += 2;
		/* LINTED */
		*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->sh_addr +
			got_off);
	} else {
		pltent[0] = M_SPECIAL_INST;
		pltent[1] = M_JMP_REG_DISP_IND;
		pltent += 2;
		/* LINTED */
		*(Word *)pltent = (Word)got_off;
	}
	pltent += 4;

	pltent[0] = M_INST_PUSHL;
	pltent++;
	/* LINTED */
	*(Word *)pltent = (Word)rel_off;
	pltent += 4;

	plt_off = -(plt_off + 16);	/* JMP, PUSHL, JMP take 16 bytes */
	pltent[0] = M_INST_JMP;
	pltent++;
	/* LINTED */
	*(Word *)pltent = (Word)plt_off;
}

/*
 * Partially Initialized Symbol Handling routines
 */
uintptr_t
perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *	osp;	/* output section */
	Os_desc *	relosp;	/* reloc output section */
	Word		ndx;	/* sym or scn index */
	Word		roffset; /* roffset for output rel */
	Word		value;
	Rel		rea;	/* SHT_RELA entry. */
	char		*relbits;
	Sym_desc *	sdp;	/* current relocation sym */

	sdp = orsp->rel_sym;
	value = sdp->sd_sym->st_value;

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

	if (orsp->rel_flags & FLG_REL_GOT) {
		Gotndx * gnp;
		osp = ofl->ofl_osgot;
		gnp = find_gotndx(&sdp->sd_GOTndxs, 0);
		roffset = (Word)(osp->os_shdr->sh_addr +
		    (gnp->gn_gotndx * M_GOT_ENTSIZE));
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		/*
		 * Note that relocations for PLT's actually
		 * cause a relocation againt the GOT.
		 */
		osp = ofl->ofl_osplt;
		roffset = (Word) (ofl->ofl_osgot->os_shdr->sh_addr) +
		    sdp->sd_aux->sa_PLTGOTndx * M_GOT_ENTSIZE;

		plt_entry(ofl, osp->os_relosdesc->os_szoutrels, sdp);

	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * this must be a R_386_COPY - for those
		 * we also set the roffset to point to the
		 * new symbols location.
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Word)value;
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

	if ((osp == 0) || (relosp = osp->os_relosdesc) == 0)
		relosp = ofl->ofl_osrel;



	/*
	 * assign the symbols index for the output
	 * relocation.  If the relocation refers to a
	 * SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise
	 * the index can be derived from the symbols index
	 * itself.
	 */
	if (orsp->rel_rtype == R_386_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF32_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)) {
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
		ndx = sdp->sd_symndx;

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF32_R_INFO(ndx, orsp->rel_rtype);
	rea.r_offset = roffset;
	DBG_CALL(Dbg_reloc_out(M_MACH, relosp->os_shdr->sh_type, &rea,
	    orsp->rel_sname, relosp->os_name));

	(void) memcpy((relbits + relosp->os_szoutrels),
		(char *)&rea, sizeof (Rel));
	relosp->os_szoutrels += sizeof (Rel);

	/*
	 * Determine whether this relocation is against a
	 * non-writeable, allocatable section.  If so we may
	 * need to provide a text relocation diagnostic.  Note
	 * that relocations against the .plt (R_386_JMP_SLOT)
	 * actually result in modifications to the .got.
	 */
	if (orsp->rel_rtype == R_386_JMP_SLOT)
		osp = ofl->ofl_osgot;
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
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			unsigned char	*addr;
			Word 		value;
			Sym_desc *	sdp;
			const char	*ifl_name;
			Word		refaddr = arsp->rel_roffset +
					    (Off)_elf_getxoff(arsp->
						rel_isdesc->is_indata);
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
				if (arsp->rel_flags & FLG_REL_MOVETAB) {
					adj_movereloc(ofl, arsp);
				} else
					adj_expandreloc(ofl, arsp);
			}

			if (arsp->rel_flags & FLG_REL_CLVAL)
				value = 0;
			else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 */
				value = (Off)_elf_getxoff(sdp->sd_isc->
				    is_indata);
				if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
					value += sdp->sd_isc->is_osdesc->
					    os_shdr->sh_addr;
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
					value = (Word)(ofl->ofl_osplt->os_shdr->
					    sh_addr) + M_PLT_RESERVSZ +
					    ((sdp->sd_aux->sa_PLTndx - 1) *
					    M_PLT_ENTSIZE);
			}

			if (arsp->rel_flags & FLG_REL_GOT) {
				Xword		R1addr;
				uintptr_t	R2addr;
				Gotndx *	gnp;

				/*
				 * perform relocation against GOT table.  Since
				 * this doesn't fit exactly into a relocation
				 * we place the appropriate byte in the GOT
				 * directly
				 */

				/*
				 * caclulate offset into GOT at which to apply
				 * the relocation.
				 */
				gnp = find_gotndx(&sdp->sd_GOTndxs,
					0);
				R1addr = (Xword)(gnp->gn_gotndx *
					M_GOT_ENTSIZE);

				/*
				 * add the GOTs data's offset
				 */
				R2addr = R1addr + (Xword) arsp->rel_osdesc->
				    os_outdata->d_buf;

				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, arsp->rel_osdesc));
				/*
				 * and do it.
				 */
				*(Word *)R2addr = (Word)value;
				continue;
			} else if (IS_GOT_BASED(arsp->rel_rtype)) {
				value -= ofl->ofl_osgot->os_shdr->sh_addr;
			} else if (IS_GOT_PC(arsp->rel_rtype)) {
				value = (Word) (ofl->ofl_osgot->os_shdr->
				    sh_addr) - refaddr;
			} else if ((IS_PC_RELATIVE(arsp->rel_rtype)) &&
			    (!(ofl->ofl_flags & FLG_OF_RELOBJ) ||
			    (arsp->rel_osdesc == sdp->sd_isc->is_osdesc))) {
				value -= refaddr;
			} else if (IS_GOT_RELATIVE(arsp->rel_rtype)) {
				Gotndx *	gnp;
				gnp = find_gotndx(&sdp->sd_GOTndxs, 0);
				value = gnp->gn_gotndx * M_GOT_ENTSIZE;
			}

			/*
			 * Make sure we have date to relocate.
			 */
			if (arsp->rel_isdesc->is_indata->d_buf == 0) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_386_type_str(arsp->rel_rtype),
				    ifl_name,
				    arsp->rel_sname, arsp->rel_isdesc->is_name);
				return (S_ERROR);
			}

			/*
			 * Get the address of the data item we need to modify.
			 */
			addr = (unsigned char *) _elf_getxoff(arsp->rel_isdesc->
				is_indata) + arsp->rel_roffset;
			DBG_CALL(Dbg_reloc_doact(M_MACH, arsp->rel_rtype,
			    (Word)addr, value, arsp->rel_sname,
			    arsp->rel_osdesc));
			addr += (int)arsp->rel_osdesc->os_outdata->d_buf;

			if (((Word)addr > ((Word)ofl->ofl_ehdr +
			    ofl->ofl_size)) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				int	warn_level;
				if (((Word)addr - (Word)ofl->ofl_ehdr) >
				    ofl->ofl_size)
					warn_level = ERR_FATAL;
				else
					warn_level = ERR_WARNING;

				eprintf(warn_level,
					MSG_INTL(MSG_REL_INVALOFFSET),
					conv_reloc_386_type_str(
					arsp->rel_rtype),
					ifl_name,
					arsp->rel_isdesc->is_name,
					arsp->rel_sname,
					(Word)addr - (Word)ofl->ofl_ehdr);

				if (warn_level == ERR_FATAL) {
					return_code = S_ERROR;
					continue;
				}
			}

			if (do_reloc(arsp->rel_rtype, addr, &value,
			    arsp->rel_sname,
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
	Rel *		rloc = (Rel *)vrel;


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
		rcp->rc_end = (Rel_desc *)((int)rcp->rc_free +
				(sizeof (Rel_desc) * REL_OIDESCNO));
		if (list_appendc(&ofl->ofl_outrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*orsp = *rsp;
	orsp->rel_flags |= flags;
	orsp->rel_reloc = rloc;
	/*
	 * Look at ../relocate.c:process_movereloc()
	 */
	if ((orsp->rel_mventry == (Move *)NULL) ||
	    (orsp->rel_flags & FLG_REL_MOVETAB))
		orsp->rel_roffset = rloc->r_offset;
	rcp->rc_free++;

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += sizeof (Rel);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += sizeof (Rel);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += sizeof (Rel);
	else
		orsp->rel_osdesc->os_szoutrels += sizeof (Rel);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if
	 * we were to sort them we would have to re-assign the plt
	 * indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	/*
	 * If an R_386_GOTPC relocation is seen this triggers
	 * the building of the GLOBAL_OFFSET_TABLE (as defined
	 * by the abi).
	 */
	if ((orsp->rel_rtype == R_386_GOTPC) ||
	    (orsp->rel_rtype == R_386_GOTOFF))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	DBG_CALL(Dbg_reloc_ors_entry(M_MACH, orsp));
	return (1);
}


uintptr_t
add_actrel(Half flags, Rel_desc * rsp, void * vrel, Ofl_desc * ofl)
{
	Rel_desc * 	arsp;
	Rel_cache *	rcp;
	Rel *		rloc = (Rel *)vrel;

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
		rcp->rc_end = (Rel_desc *)((int)rcp->rc_free +
				(sizeof (Rel_desc) * REL_AIDESCNO));
		if (list_appendc(&ofl->ofl_actrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*arsp = *rsp;
	arsp->rel_flags |= flags;
	arsp->rel_reloc = rloc;
	/*
	 * Look at ../relocate.c:process_movereloc()
	 */
	if ((arsp->rel_mventry == (Move *)NULL) ||
	    (arsp->rel_flags & FLG_REL_MOVETAB))
		arsp->rel_roffset = rloc->r_offset;
	rcp->rc_free++;

	/*
	 * If an R_386_GOTPC relocation is seen this triggers
	 * the building of the GLOBAL_OFFSET_TABLE (as defined
	 * by the abi).
	 */
	if ((arsp->rel_rtype == R_386_GOTPC) ||
	    (arsp->rel_rtype == R_386_GOTOFF))
		ofl->ofl_flags |= FLG_OF_BLDGOT;

	DBG_CALL(Dbg_reloc_ars_entry(M_MACH, arsp));
	return (1);
}


/*
 * stub routine since register symbols are not allowed on i386
 */
/* ARGSUSED */
uintptr_t
reloc_register(Rel_desc * rsp, void * reloc, Ofl_desc * ofl)
{
	eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOREG));
	return (S_ERROR);
}

/*
 * stub routine since FPTR's are not valid on IA32
 */
/* ARGSUSED */
uintptr_t
reloc_local_fptr(Rel_desc * rsp, void * reloc, Ofl_desc * ofl)
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
	Rel *		reloc = (Rel *)vrel;

	/*
	 * if ((shared_object) and
	 *    (not pc relative relocation))
	 * then
	 *	build R_386_RELATIVE
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype))) {
		Word	ortype = rsp->rel_rtype;
		rsp->rel_rtype = R_386_RELATIVE;
		if (add_outrel(NULL, rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
	}

	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    (rsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF)) {
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_386_type_str(rsp->rel_rtype), rsp->rel_fname,
		    rsp->rel_sname, rsp->rel_osdesc->os_name);
		return (1);
	}
	/*
	 * perform relocation
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
	Rel *		reloc = (Rel *)vrel;

	/*
	 * Try to determine if we can do any relocations at
	 * this point.  We can if:
	 *
	 * (local_symbol) and (non_GOT_relocation) and
	 * (IS_PC_RELATIVE()) and
	 * (relocation to symbol in same section)
	 */
	if (local && !IS_GOT_RELATIVE(rtype) && IS_PC_RELATIVE(rtype) &&
	    (rtype != R_386_GOTOFF) &&
	    ((sdp->sd_isc) && (sdp->sd_isc->is_osdesc == isp->is_osdesc))) {
		return (add_actrel(NULL, rsp, reloc, ofl));
	}

	/*
	 * if '-zredlocsym' is in effect make all local sym relocations
	 * against the 'section symbols', since they are the only symbols
	 * which will be added to the .symtab
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
		if (add_actrel(NULL, rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);
		return (add_outrel(FLG_REL_SCNNDX, rsp, reloc, ofl));
	}

	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION)
		if (add_actrel(NULL, rsp, reloc, ofl) == S_ERROR)
			return (S_ERROR);

	return (add_outrel(NULL, rsp, reloc, ofl));
}


/* ARGSUSED */
Gotndx *
find_gotndx(List * lst, void * vrel)
{
	Gotndx *	gnp;
	/*
	 * Since ia32 doesn't have addend there will be
	 * at max one entry on this list.  If there's an
	 * entry - return it.
	 */
	if (lst->head) {
		gnp = (Gotndx *)lst->head->data;
		return (gnp);
	}
	return ((Gotndx *)0);
}

/* ARGSUSED */
uintptr_t
assign_got_ndx(List * lst, Rel_desc * rsp, void * vrel,  Sym_desc * sdp,
	Gotndx * pgnp, Ofl_desc * ofl)
{
	Gotndx *	gnp;

	if (pgnp)
		return (1);
	/*
	 * Since ia32 does not use Rela relocations this is
	 * a list of exactly one entry.
	 */
	if ((gnp = libld_calloc(sizeof (Gotndx), 1)) == 0)
		return (S_ERROR);
	gnp->gn_gotndx = ofl->ofl_gotcnt++;
	if (list_appendc(lst, (void *)gnp) == 0)
		return (S_ERROR);
	return (1);
}


void
assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = 1 + ofl->ofl_pltcnt++;
	sdp->sd_aux->sa_PLTGOTndx = ofl->ofl_gotcnt++;
}

/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
void
fillin_gotplt1(Ofl_desc * ofl)
{
	Sym_desc *	sdp;

	if (ofl->ofl_osgot) {
		if ((sdp = sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, ofl)) != NULL) {
			unsigned char *genptr = ((unsigned char *)
			    ofl->ofl_osgot->os_outdata->d_buf +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*(Word *)genptr = (Word)sdp->sd_sym->st_value;
		}
	}

	/*
	 * Fill in the reserved slot in the procedure linkage table the first
	 * entry is:
	 *  if (building a.out) {
	 *	PUSHL	got[1]		    # the address of the link map entry
	 *	JMP *	got[2]		    # the address of rtbinder
	 *  } else {
	 *	PUSHL	got[1]@GOT(%ebx)    # the address of the link map entry
	 *	JMP *	got[2]@GOT(%ebx)    # the address of rtbinder
	 *  }
	 */
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) && ofl->ofl_osplt) {
		unsigned char *pltent;

		pltent = (unsigned char *)ofl->ofl_osplt->os_outdata->d_buf;
		if (!(ofl->ofl_flags & FLG_OF_SHAROBJ)) {
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_PUSHL_DISP;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->
				sh_addr + M_GOT_XLINKMAP * M_GOT_ENTSIZE);
			pltent += 4;
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_JMP_DISP_IND;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(ofl->ofl_osgot->os_shdr->
				sh_addr + M_GOT_XRTLD * M_GOT_ENTSIZE);
		} else {
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_PUSHL_REG_DISP;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(M_GOT_XLINKMAP *
				M_GOT_ENTSIZE);
			pltent += 4;
			pltent[0] = M_SPECIAL_INST;
			pltent[1] = M_JMP_REG_DISP_IND;
			pltent += 2;
			/* LINTED */
			*(Word *)pltent = (Word)(M_GOT_XRTLD *
				M_GOT_ENTSIZE);
		}
	}

}

/*
 * Return got[0].
 */
Addr
fillin_gotplt2(Ofl_desc * ofl)
{
	if (ofl->ofl_osgot)
		return (ofl->ofl_osgot->os_shdr->sh_addr);
	else
		return (0);
}
