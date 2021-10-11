/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sparc_elf.c	1.33	99/09/14 SMI"

/*
 * SPARC V9 machine dependent and ELF file class dependent functions.
 * Contains routines for performing function binding and symbol relocations.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/elf.h>
#include	<sys/elf_SPARC.h>
#include	<sys/mman.h>
#include	<sys/procfs_isa.h>
#include	<signal.h>
#include	<dlfcn.h>
#include	<synch.h>
#include	<string.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"
#include	"reloc.h"
#include	"conv.h"


extern void	iflush_range(caddr_t, size_t);
extern void	elf_rtbndr(caddr_t pc, unsigned long reloc,
    Rt_map * lmp, caddr_t from);

/*
 * Local storage space created on the stack created for this glue
 * code includes space for:
 *		0x8	pointer to dyn_data
 *		0x8	size prev stack frame
 */
static const Byte dyn_plt_template[] = {
/* 0x0 */	0x2a, 0xcf, 0x80, 0x03,	/* brnz,a,pt %fp, 0xc	*/
/* 0x4 */	0x82, 0x27, 0x80, 0x0e,	/* sub %fp, %sp, %g1 */
/* 0x8 */	0x82, 0x10, 0x20, 0xb0,	/* mov 176, %g1	*/
/* 0xc */	0x9d, 0xe3, 0xbf, 0x40,	/* save %sp, -192, %sp	*/
/* 0x10 */	0xc2, 0x77, 0xa7, 0xef,	/* stx %g1, [%fp + 2031] */
/* 0x14 */	0x0b, 0x00, 0x00, 0x00,	/* sethi %hh(dyn_data), %g5 */
/* 0x18 */	0x8a, 0x11, 0x60, 0x00,	/* or %g5, %hm(dyn_data), %g5	*/
/* 0x1c */	0x8b, 0x29, 0x70, 0x20,	/* sllx %g5, 32, %g5	*/
/* 0x20 */	0x03, 0x00, 0x00, 0x00,	/* sethi %lm(dyn_data), %g1	*/
/* 0x24 */	0x82, 0x10, 0x60, 0x00,	/* or %g1, %lo(dyn_data), %g1	*/
/* 0x28 */	0x82, 0x10, 0x40, 0x05,	/* or %g1, %g5, %g1	*/
/* 0x2c */	0x40, 0x00, 0x00, 0x00,	/* call <rel_addr>	*/
/* 0x30 */	0xc2, 0x77, 0xa7, 0xf7,	/* stx %g1, [%fp + 2039] */
/* 0x34 */	0x01, 0x00, 0x00, 0x00	/* nop ! for 8-byte alignment */
};


int	dyn_plt_ent_size = sizeof (dyn_plt_template) +
		sizeof (Addr) +		/* reflmp */
		sizeof (Addr) +		/* deflmp */
		sizeof (Word) +		/* symndx */
		sizeof (Word) +		/* sb_flags */
		sizeof (Sym);		/* symdef */


/*
 * the dynamic plt entry is:
 *
 *	brnz,a,pt	%fp, 1f
 *	 sub     	%sp, %fp, %g1
 *	mov     	SA(MINFRAME), %g1
 * 1:
 *	save    	%sp, -(SA(MINFRAME) + (2 * CLONGSIZE)), %sp
 *
 *	! store prev stack size
 *	stx     	%g1, [%fp + STACK_BIAS - (2 * CLONGSIZE)]
 *
 *	sethi   	%hh(dyn_data), %g5
 *	or      	%g5, %hm(dyn_data), %g5
 *	sllx    	%g5, 32, %g5
 *	sethi   	%lm(dyn_data), %g1
 *	or      	%g1, %lo(dyn_data), %g1
 *	or      	%g1, %g5, %g1
 *
 *	! store dyn_data ptr and call
 *	call    	elf_plt_trace
 *	 stx     	%g1, [%fp + STACK_BIAS - CLONGSIZE]
 *	nop
 * * dyn data:
 *	Addr		reflmp
 *	Addr		deflmp
 *	Word		symndx
 *	Word		sb_flags
 *	Sym		symdef  (Elf64_Sym = 24-bytes)
 */
static caddr_t
elf_plt_trace_write(caddr_t pc, Rt_map * rlmp, Rt_map * dlmp, Sym * sym,
	uint_t symndx, ulong_t pltndx, caddr_t to, uint_t sb_flags)
{
	extern ulong_t	elf_plt_trace();
	Addr		dyn_plt;
	Addr *		dyndata;

	/*
	 * If both pltenter & pltexit have been disabled there
	 * there is no reason to even create the glue code.
	 */
	if ((sb_flags & (LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT)) ==
	    (LA_SYMB_NOPLTENTER | LA_SYMB_NOPLTEXIT)) {
		/* LINTED */
		elf_plt_write((unsigned long *)pc, (unsigned long *)to, 0);
		return (pc);
	}

	/*
	 * We only need to add the glue code if there is an auditing
	 * library that is interested in this binding.
	 */
	dyn_plt = (Xword)AUDINFO(rlmp)->ai_dynplts +
		(pltndx * dyn_plt_ent_size);

	/*
	 * Have we initialized this dynamic plt entry yet?  If we havn't do it
	 * now.  Otherwise this function has been called before, but from a
	 * different plt (ie. from another shared object).  In that case
	 * we just set the plt to point to the new dyn_plt.
	 */
	if (*(Word *)dyn_plt == 0) {
		Sym *	symp;
		Xword	symvalue;

		(void) memcpy((void *)dyn_plt, dyn_plt_template,
			sizeof (dyn_plt_template));
		/* LINTED */
		dyndata = (Addr *)(dyn_plt + sizeof (dyn_plt_template));

		/*
		 * relocating:
		 *	sethi	%hh(dyndata), %g5
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_HH22, (Byte *)(dyn_plt + 0x14),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		/*
		 * relocating:
		 *	or	%g5, %hm(dyndata), %g5
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_HM10, (Byte *)(dyn_plt + 0x18),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		/*
		 * relocating:
		 *	sethi	%lm(dyndata), %g1
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_LM22, (Byte *)(dyn_plt + 0x20),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		/*
		 * relocating:
		 *	or	%g1, %lo(dyndata), %g1
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_LO10, (Byte *)(dyn_plt + 0x24),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		/*
		 * relocating:
		 *	call	elf_plt_trace
		 */
		symvalue = (Xword)((Addr)&elf_plt_trace -
			(Addr)(dyn_plt + 0x2c));
		if (do_reloc(R_SPARC_WDISP30, (Byte *)(dyn_plt + 0x2c),
		    &symvalue, MSG_ORIG(MSG_SYM_ELFPLTTRACE),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		*dyndata++ = (Addr)rlmp;
		*dyndata++ = (Addr)dlmp;

		/*
		 * symndx in the high word, sb_flags in the low.
		 */
		*dyndata = (Addr)sb_flags;
		*(Word *)dyndata = symndx;
		dyndata++;

		symp = (Sym *)dyndata;
		*symp = *sym;
		symp->st_value = (Addr)to;
		iflush_range((void *)dyn_plt, sizeof (dyn_plt_template));
	}

	/* LINTED */
	elf_plt_write((unsigned long *)pc, (unsigned long *)dyn_plt, 0);
	return (pc);
}


/*
 *  We got here because something invoked .PLT0, which for v9
 *  should handle extended entries (plt entries who's index is
 *  > 32k -- see the v9 ABI).  This isn't implemented, so warn
 *  the user.
 */
void
/*ARGSUSED1*/
elf_rtbndr_err(unsigned long pltoff, unsigned long plt)
{
	Xword pltndx = (pltoff - M_PLT_RESERVSZ) / M_PLT_ENTSIZE;

	eprintf(ERR_FATAL, MSG_INTL(MSG_REL_EXTND_PLTREF),
		conv_reloc_SPARC_type_str(R_SPARC_JMP_SLOT),
		EC_XWORD(pltndx));
	exit(1);
}


/*
 * Function binding routine - invoked on the first call to a function through
 * the procedure linkage table;
 * passes first through an assembly language interface.
 *
 * Takes the address of the PLT entry where the call originated,
 * the offset into the relocation table of the associated
 * relocation entry and the address of the link map (rt_private_map struct)
 * for the entry.
 *
 * Returns the address of the function referenced after re-writing the PLT
 * entry to invoke the function directly.
 *
 * On error, causes process to terminate with a signal.
 */

unsigned long
elf_bndr(caddr_t pc, unsigned long pltoff, Rt_map * lmp, caddr_t from)
{
	Rt_map *	nlmp, ** tobj = 0;
	Addr		addr, reloff, symval;
	char *		name;
	Rela *		rptr;
	Sym *		sym, * nsym;
	Xword		pltndx;
	int		bind;
	uint_t		sb_flags = 0;
	unsigned long	rsymndx;
	Slookup		sl;

	PRF_MCOUNT(1, elf_bndr);
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	/*
	 * Must calculate true plt relocation address from reloc.
	 * Take offset, subtract number of reserved PLT entries, and divide
	 * by PLT entry size, which should give the index of the plt
	 * entry (and relocation entry since they have been defined to be
	 * in the same order).  Then we must multiply by the size of
	 * a relocation entry, which will give us the offset of the
	 * plt relocation entry from the start of them given by JMPREL(lm).
	 */
	addr = pltoff - M_PLT_RESERVSZ;
	pltndx = addr / M_PLT_ENTSIZE;

	/*
	 * Perform some basic sanity checks.  If we didn't get a load map
	 * or the plt offset is invalid then its possible someone has walked
	 * over the plt entries.
	 */
	if (!lmp || ((addr % M_PLT_ENTSIZE) != 0)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_PLTENTRY),
		    conv_reloc_SPARC_type_str(R_SPARC_JMP_SLOT),
		    EC_XWORD(pltndx), EC_ADDR(from), (!lmp ?
			MSG_INTL(MSG_REL_PLTREF) :
			MSG_INTL(MSG_REL_PLTOFF)));
		exit(1);
	}
	reloff = pltndx * sizeof (Rela);

	/*
	 * Use relocation entry to get symbol table entry and symbol name.
	 */
	addr = (unsigned long)JMPREL(lmp);
	rptr = (Rela *)(addr + reloff);
	rsymndx = ELF_R_SYM(rptr->r_info);
	sym = (Sym *)((unsigned long)SYMTAB(lmp) +
		(rsymndx * SYMENT(lmp)));
	name = (char *)(STRTAB(lmp) + sym->st_name);

	/*
	 * Find definition for symbol.
	 */
	sl.sl_name = name;
	sl.sl_permit = PERMIT(lmp);
	sl.sl_cmap = lmp;
	sl.sl_imap = LIST(lmp)->lm_head;
	sl.sl_rsymndx = rsymndx;
	if ((nsym = lookup_sym(&sl, &nlmp, LKUP_DEFT)) != 0) {
		symval = nsym->st_value;
		if (!(FLAGS(nlmp) & FLG_RT_FIXED) &&
		    (nsym->st_shndx != SHN_ABS))
			symval += ADDR(nlmp);
		if ((lmp != nlmp) && (!(FLAGS(nlmp) & FLG_RT_ISMAIN))) {
			/*
			 * Record that this new link map is now bound to the
			 * caller.
			 */
			if (bound_add(REF_SYMBOL, lmp, nlmp) == 0)
				exit(1);
		}
	} else {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NOSYM), NAME(lmp), name);
		exit(1);
	}

	/*
	 * Print binding information and rebuild PLT entry.
	 */
	DBG_CALL(Dbg_bind_global(NAME(lmp), from, from - ADDR(lmp),
	    (uint_t)pltndx, NAME(nlmp), (caddr_t)symval,
	    (caddr_t)nsym->st_value, name));

	if ((LIST(lmp)->lm_flags | FLAGS1(lmp)) & FL1_AU_SYMBIND) {
		/* LINTED */
		uint_t	symndx = (uint_t)(((uintptr_t)nsym -
			(uintptr_t)SYMTAB(nlmp)) / SYMENT(nlmp));

		symval = audit_symbind(lmp, nlmp, nsym, symndx, symval,
			&sb_flags);
	}

	if (!(rtld_flags & RT_FL_NOBIND)) {
		if (((LIST(lmp)->lm_flags | FLAGS1(lmp)) &
		    (FL1_AU_PLTENTER | FL1_AU_PLTEXIT)) &&
		    AUDINFO(lmp)->ai_dynplts) {
			/* LINTED */
			uint_t	symndx = (uint_t)(((uintptr_t)nsym -
				(uintptr_t)SYMTAB(nlmp)) / SYMENT(nlmp));

			symval = (unsigned long) elf_plt_trace_write(pc, lmp,
				nlmp, nsym, symndx, pltndx, (caddr_t)symval,
				sb_flags);
		} else {
			/*
			 * Write standard PLT entry to jump directly
			 * to newly bound function.
			 */
			/* LINTED */
			elf_plt_write((unsigned long *)pc,
			    (unsigned long *)symval, 0);
		}
	}

	if (bind) {
		/*
		 * If objects were loaded determine if any .init sections need
		 * firing.  Note that we don't know exactly where any new
		 * objects are loaded (we know the object that supplied the
		 * symbol, but others may have been loaded lazily as we
		 * searched for the symbol), so sorting starts from the caller.
		 */
		if ((tobj = tsort(nlmp, LIST(nlmp)->lm_init,
		    RT_SORT_REV)) == (Rt_map **)S_ERROR)
			tobj = 0;

		if (rtld_flags & RT_FL_CLEANUP)
			cleanup();

		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	if ((LIST(lmp)->lm_flags | FLAGS1(lmp)) & FL1_AU_ACTIVITY)
		audit_activity(lmp, LA_ACT_CONSISTENT);

	/*
	 * After releasing any locks call any .init sections if necessary.
	 */
	if (tobj)
		call_init(tobj);

	return (symval);
}


static int
bindpltpad(Rt_map * lmp, List * padlist, Addr value, void ** pltaddr,
    const char * fname, const char * sname)
{
	Listnode *	lnp;
	Pltpadinfo *	pip;
	void *		plt;
	int		i;

	for (LIST_TRAVERSE(padlist, lnp, pip)) {
		if (pip->pp_addr == value) {
			*pltaddr = pip->pp_plt;
			DBG_CALL(Dbg_pltpad_bindto64(NAME(lmp), sname,
				(Addr)*pltaddr));
			return (1);
		}
	}

	plt = PLTPAD(lmp);
	PLTPAD(lmp) = (void *)((uintptr_t)PLTPAD(lmp) + M_PLT_ENTSIZE);

	if (PLTPAD(lmp) > PLTPADEND(lmp)) {
		/*
		 * Just fail in usual relocation way
		 */
		*pltaddr = (void *)value;
		return (1);
	}

	/*
	 * Elf_plt_write assumes the plt was previously filled
	 * with NOP's, so fill it in now.
	 */
	for (i = 0; i < (M_PLT_ENTSIZE / sizeof (unsigned int)); i++) {
		((unsigned int *)plt)[i] = M_NOP;
	}
	iflush_range((caddr_t)plt, M_PLT_ENTSIZE);

	elf_plt_write((unsigned long *)plt, (unsigned long *)value, 0);

	if ((pip = calloc(sizeof (Pltpadinfo), 1)) == 0)
		return (0);
	pip->pp_addr = value;
	pip->pp_plt = plt;

	if (list_append(padlist, pip) == 0) {
		free(pip);
		return (0);
	}

	*pltaddr = plt;
	DBG_CALL(Dbg_pltpad_boundto64(NAME(lmp), (Addr)*pltaddr, fname, sname));
	return (1);
}

/*
 * Read and process the relocations for one link object, we assume all
 * relocation sections for loadable segments are stored contiguously in
 * the file.
 */
int
elf_reloc(Rt_map *lmp, int plt)
{
	ulong_t		relbgn, relend, relsiz, basebgn, pltbgn, pltend;
	ulong_t		roffset, rsymndx, psymndx = 0, etext, emap;
	uint_t		dsymndx;
	Byte		rtype;
	long		reladd;
	Addr		value, pvalue;
	Sym *		symref, * psymref, * symdef, * psymdef;
	char *		name, * pname;
	Rt_map *	_lmp, * plmp;
	int		textrel = 0, bound = 0, error = 0, noplt = 0;
	long		relacount = RELACOUNT(lmp);
	Rela *		rel;
	List		pltpadlist = {0, 0};

	PRF_MCOUNT(2, elf_reloc);

	/*
	 * If an object has any DT_REGISTER entries associated with
	 * it, they are processed now.
	 */
	if (FLAGS(lmp) & FLG_RT_REGSYMS) {
		if (elf_regsyms(lmp) == 0)
			return (0);
	}

	/*
	 * Although only necessary for lazy binding, initialize the first
	 * procedure linkage table entry to go to elf_rtbndr().  dbx(1) seems
	 * to find this useful.
	 */
	if ((plt == 0) && PLTGOT(lmp)) {
		Xword pltoff;

		/*
		 * Install the lm pointer in .PLT2 as per the ABI.
		 */
		pltoff = (2 * M_PLT_ENTSIZE) / M_PLT_INSSIZE;
		elf_plt2_init(PLTGOT(lmp) + pltoff, lmp);

		/*
		 * The V9 ABI states that the first 32k PLT entries
		 * use .PLT1, with .PLT0 used by the "latter" entries.
		 * We don't currently implement the extendend format,
		 * so install an error handler in .PLT0 to catch anyone
		 * trying to use it.
		 */
		elf_plt_init(PLTGOT(lmp), (caddr_t)elf_rtbndr_err);


		/*
		 * Initialize .PLT1
		 */
		pltoff = M_PLT_ENTSIZE / M_PLT_INSSIZE;
		elf_plt_init(PLTGOT(lmp) + pltoff, (caddr_t)elf_rtbndr);
	}


	/*
	 * Initialize the plt start and end addresses.
	 */
	if ((pltbgn = (unsigned long)JMPREL(lmp)) != 0)
		pltend = pltbgn + (unsigned long)(PLTRELSZ(lmp));

	/*
	 * If we've been called upon to promote an RTLD_LAZY object to an
	 * RTLD_NOW then we're only interested in scaning the .plt table.
	 */
	if (plt) {
		relbgn = pltbgn;
		relend = pltend;
	} else {
		/*
		 * The relocation sections appear to the run-time linker as a
		 * single table.  Determine the address of the beginning and end
		 * of this table.  There are two different interpretations of
		 * the ABI at this point:
		 *
		 *   o	The REL table and its associated RELSZ indicate the
		 *	concatenation of *all* relocation sections (this is the
		 *	model our link-editor constructs).
		 *
		 *   o	The REL table and its associated RELSZ indicate the
		 *	concatenation of all *but* the .plt relocations.  These
		 *	relocations are specified individually by the JMPREL and
		 *	PLTRELSZ entries.
		 *
		 * Determine from our knowledege of the relocation range and
		 * .plt range, the range of the total relocation table.  Note
		 * that one other ABI assumption seems to be that the .plt
		 * relocations always follow any other relocations, the
		 * following range checking drops that assumption.
		 */
		relbgn = (unsigned long)(REL(lmp));
		relend = relbgn + (unsigned long)(RELSZ(lmp));
		if (pltbgn) {
			if (!relbgn || (relbgn > pltbgn))
				relbgn = pltbgn;
			if (!relbgn || (relend < pltend))
				relend = pltend;
		}
	}
	if (!relbgn || (relbgn == relend))
		return (1);

	relsiz = (unsigned long)(RELENT(lmp));
	basebgn = ADDR(lmp);
	etext = ETEXT(lmp);
	emap = ADDR(lmp) + MSIZE(lmp);

	DBG_CALL(Dbg_reloc_run(NAME(lmp), M_DYNREL_SHT_TYPE));

	/*
	 * If we're processing in lazy mode there is no need to scan the
	 * .rela.plt table.
	 */
	if (pltbgn && !(MODE(lmp) & RTLD_NOW))
		noplt = 1;

	/*
	 * Loop through relocations.
	 */
	while (relbgn < relend) {
		uint_t		sb_flags = 0;

		/* LINTED */
		rtype = (Byte)ELF_R_TYPE(((Rela *)relbgn)->r_info);

		/*
		 * If this is a RELATIVE relocation in a shared object
		 * (the common case), and if we are not debugging, then
		 * jump into a tighter relocaiton loop (elf_reloc_relacount)
		 * Only make the jump if we've been given a hint on the
		 * number of relocations.
		 */
		if ((rtype == R_SPARC_RELATIVE) &&
		    !(FLAGS(lmp) & FLG_RT_FIXED) && !dbg_mask) {
			if (relacount) {
				relbgn = elf_reloc_relacount(relbgn, relacount,
					relsiz, basebgn);

				relacount = 0;
			} else
				relbgn = elf_reloc_relative(relbgn, relend,
					relsiz, basebgn, etext, emap);
			if (relbgn >= relend)
				break;

			/* LINTED */
			rtype = (Byte)ELF_R_TYPE(((Rela *)relbgn)->r_info);
		}

		roffset = ((Rela *)relbgn)->r_offset;

		reladd = (long)(((Rela *)relbgn)->r_addend);
		rsymndx = ELF_R_SYM(((Rela *)relbgn)->r_info);

		rel = (Rela *)relbgn;
		relbgn += relsiz;

		/*
		 * Optimizations.
		 */
		if (rtype == R_SPARC_NONE)
			continue;
		if (noplt && ((unsigned long)rel >= pltbgn) &&
		    ((unsigned long)rel < pltend)) {
			relbgn = pltend;
			continue;
		}

		if (rtype != R_SPARC_REGISTER) {
			/*
			 * If this is a shared object, add the base address
			 * to offset.
			 */
			if (!(FLAGS(lmp) & FLG_RT_FIXED))
				roffset += basebgn;

			/*
			 * If this relocation is not against part of the image
			 * mapped into memory we skip it.
			 */
			if ((roffset < ADDR(lmp)) || (roffset > (ADDR(lmp) +
			    MSIZE(lmp))))
				continue;
		}

		/*
		 * If we're promoting plts determine if this one has already
		 * been written. An uninitialized plts' second instruction is a
		 * branch.
		 */
		if (plt) {
			unsigned char *	_roffset = (unsigned char *)roffset;

			_roffset += M_PLT_INSSIZE;
			if ((*(unsigned int *)_roffset &
			    (~(S_MASK(19)))) != M_BA_A_XCC)
				continue;
		}

		/*
		 * If a symbol index is specified then get the symbol table
		 * entry, locate the symbol definition, and determine its
		 * address.
		 */
		if (rsymndx) {
			/*
			 * Get the local symbol table entry.
			 */
			symref = (Sym *)((unsigned long)SYMTAB(lmp) +
					(rsymndx * SYMENT(lmp)));

			/*
			 * If this is a local symbol, just use the base address.
			 * (we should have no local relocations in the
			 * executable).
			 */
			if (ELF_ST_BIND(symref->st_info) == STB_LOCAL) {
				value = basebgn;
				name = (char *)0;
			} else {
				/*
				 * If the symbol index is equal to the previous
				 * symbol index relocation we processed then
				 * reuse the previous values. (Note that there
				 * have been cases where a relocation exists
				 * against a copy relocation symbol, our ld(1)
				 * should optimize this away, but make sure we
				 * don't use the same symbol information should
				 * this case exist).
				 */
				if ((rsymndx == psymndx) &&
				    (rtype != R_SPARC_COPY)) {
					/* LINTED */
					value = pvalue;
					/* LINTED */
					name = pname;
					/* LINTED */
					symdef = psymdef;
					/* LINTED */
					symref = psymref;
					/* LINTED */
					_lmp = plmp;
					if ((LIST(_lmp)->lm_flags |
					    FLAGS1(_lmp)) & FL1_AU_SYMBIND) {
						value = audit_symbind(lmp, _lmp,
						    /* LINTED */
						    symdef, dsymndx, value,
						    &sb_flags);
					}
				} else {
					int		flags;
					Slookup		sl;
					unsigned char	bind;

					/*
					 * Lookup the symbol definition.
					 */
					name = (char *)(STRTAB(lmp) +
					    symref->st_name);

					sl.sl_name = name;
					sl.sl_permit = PERMIT(lmp);
					sl.sl_cmap = lmp;
					sl.sl_imap = LIST(lmp)->lm_head;
					sl.sl_rsymndx = rsymndx;

					if (rtype == R_SPARC_COPY)
						flags = LKUP_COPY;
					else
						flags = 0;

					if (rtype == R_SPARC_JMP_SLOT)
						flags |= LKUP_DEFT;
					else
						flags |= LKUP_SPEC;

					bind = ELF_ST_BIND(symref->st_info);
					if (bind == STB_WEAK)
						flags |= LKUP_WEAK;

					symdef = lookup_sym(&sl, &_lmp, flags);

					/*
					 * If the symbol is not found and the
					 * reference was not to a weak symbol,
					 * report an error.  Weak references may
					 * be unresolved.
					 * chkmsg: MSG_INTL(MSG_LDD_SYM_NFOUND)
					 */
					if (symdef == 0) {
					    if (bind != STB_WEAK) {
						if (LIST(lmp)->lm_flags &
						    LML_FLG_IGNERROR) {
						    continue;
						} else if (LIST(lmp)->lm_flags &
						    LML_TRC_WARN) {
						    (void) printf(MSG_INTL(
							MSG_LDD_SYM_NFOUND),
							name, NAME(lmp));
						    continue;
						} else {
						    eprintf(ERR_FATAL,
							MSG_INTL(MSG_REL_NOSYM),
							NAME(lmp), name);
						    return (0);
						}
					    } else {
						DBG_CALL(Dbg_bind_weak(
						    NAME(lmp), (caddr_t)roffset,
						    (caddr_t)
						    (roffset - basebgn), name));
						continue;
					    }
					}

					/*
					 * If symbol was found in an object
					 * other than the referencing object
					 * then set the boundto bit in the
					 * defining object.
					 */
					if ((lmp != _lmp) &&
					    (!(FLAGS(_lmp) & FLG_RT_ISMAIN))) {
						FLAGS(_lmp) |= FLG_RT_BOUND;
						bound = 1;
					}

					/*
					 * Calculate the location of definition;
					 * symbol value plus base address of
					 * containing shared object.
					 */
					value = symdef->st_value;
					if (!(FLAGS(_lmp) & FLG_RT_FIXED) &&
					    (symdef->st_shndx != SHN_ABS))
						value += ADDR(_lmp);

					/*
					 * Retain this symbol index and the
					 * value in case it can be used for the
					 * subsequent relocations.
					 */
					if (rtype != R_SPARC_COPY) {
						psymndx = rsymndx;
						pvalue = value;
						pname = name;
						psymdef = symdef;
						psymref = symref;
						plmp = _lmp;
					}
					if ((LIST(_lmp)->lm_flags |
					    FLAGS1(_lmp)) & FL1_AU_SYMBIND) {
						/* LINTED */
						dsymndx = (((uintptr_t)symdef -
							(uintptr_t)
							SYMTAB(_lmp)) /
							SYMENT(_lmp));
						value = audit_symbind(lmp, _lmp,
						    symdef, dsymndx, value,
						    &sb_flags);
					}

				}

				/*
				 * If relocation is PC-relative, subtract
				 * offset address.
				 */
				if (IS_PC_RELATIVE(rtype))
					value -= roffset;

				DBG_CALL(Dbg_bind_global(NAME(lmp),
				    (caddr_t)roffset,
				    (caddr_t)(roffset - basebgn), (Xword)(-1),
				    NAME(_lmp), (caddr_t)value,
				    (caddr_t)symdef->st_value, name));
			}
		} else {
			/*
			 * Special case, a regsiter symbol associated with
			 * symbol index 0 is initialized (i.e. relocated) to
			 * a constant in the r_addend field rather than to a
			 * symbol value.
			 */
			if (rtype == R_SPARC_REGISTER)
				value = 0;
			else
				value = basebgn;
			name = (char *)0;
		}

		/*
		 * If this object has relocations in the text segment, turn
		 * off the write protect.
		 */
		if ((rtype != R_SPARC_REGISTER) && (roffset < etext)) {
			if (!textrel) {
				if (elf_set_prot(lmp, PROT_WRITE) == 0)
					return (0);
				textrel = 1;
			}
		}

		/*
		 * Call relocation routine to perform required relocation.
		 */
		DBG_CALL(Dbg_reloc_in(M_MACH, M_DYNREL_SHT_TYPE, rel,
			name, NULL));

		switch (rtype) {
		case R_SPARC_REGISTER:
			/*
			 * The v9 ABI 4.2.4 says that system objects may,
			 * but are not required to, use register symbols
			 * to inidcate how they use global registers. Thus
			 * at least %g6, %g7 must be allowed in addition
			 * to %g2 and %g3.
			 */
			value += reladd;
			if (roffset == R_G1) {
				set_sparc_g1(value);
			} else if (roffset == STO_SPARC_REGISTER_G2) {
				set_sparc_g2(value);
			} else if (roffset == STO_SPARC_REGISTER_G3) {
				set_sparc_g3(value);
			} else if (roffset == R_G4) {
				set_sparc_g4(value);
			} else if (roffset == R_G5) {
				set_sparc_g5(value);
			} else if (roffset == R_G6) {
				set_sparc_g6(value);
			} else if (roffset == R_G7) {
				set_sparc_g7(value);
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_BADREG),
					NAME(lmp), EC_ADDR(roffset));
				error = 1;
				break;
			}

			DBG_CALL(Dbg_reloc_reg_apply((Xword)roffset,
				(Xword)value));
			break;
		case R_SPARC_COPY:
			if (elf_copy_reloc(name, symref, lmp, (void *)roffset,
			    symdef, _lmp, (const void *)value) == 0)
				error = 1;
			break;
		case R_SPARC_JMP_SLOT:
			value += reladd;
			if (((LIST(lmp)->lm_flags | FLAGS1(lmp)) &
			    (FL1_AU_PLTENTER | FL1_AU_PLTEXIT)) &&
			    AUDINFO(lmp)->ai_dynplts) {
				ulong_t	pltndx = (relbgn -
					(uintptr_t)JMPREL(lmp)) / relsiz;
				/* LINTED */
				uint_t	symndx = (uint_t)(((uintptr_t)symdef -
					(uintptr_t)SYMTAB(_lmp)) /
					SYMENT(_lmp));

				(void) elf_plt_trace_write((caddr_t)roffset,
					lmp, _lmp, symdef, symndx, pltndx,
					(caddr_t)value, sb_flags);
			} else {
				/*
				 * Write standard PLT entry to jump directly
				 * to newly bound function.
				 */
				DBG_CALL(Dbg_reloc_apply(roffset,
				    (unsigned long)value));
				elf_plt_write((unsigned long *)roffset,
				    (unsigned long *)value, 0);
			}
			break;
		case R_SPARC_WDISP30:
			if (PLTPAD(lmp) &&
			    (S_INRANGE((Sxword)value, 29) == 0)) {
				void *	plt = 0;

				if (bindpltpad(lmp, &pltpadlist,
				    value + roffset, &plt,
				    NAME(_lmp), name) == 0)
					return (0);
				value = (Addr)((Addr)plt - roffset);
			}
			/* FALLTHROUGH */
		default:
			value += reladd;
			if (IS_EXTOFFSET(rtype))
				value += (Word)ELF_R_TYPE_DATA(rel->r_info);
			/*
			 * Write the relocation out.
			 */
			if (do_reloc(rtype, (unsigned char *)roffset,
			    /* LINTED */
			    (Xword *)&value,
			    name, NAME(lmp)) == 0)
				error = 1;

			/*
			 * value now contains the 'bit-shifted' value
			 * that was or'ed into memory (this was set
			 * by do_reloc()).
			 */
			DBG_CALL(Dbg_reloc_apply((unsigned long)roffset,
			    value));

			/*
			 * If this relocation is against a text segment
			 * we must make sure that the instruction
			 * cache is flushed.
			 */
			if (textrel)
				iflush_range((caddr_t)roffset, 0x4);
		}
	}

	DBG_CALL(Dbg_reloc_end(NAME(lmp)));

	if (error)
		return (0);

	/*
	 * Free up any items on the pltpadlist if it was allocated
	 */
	if (pltpadlist.head) {
		Listnode *	lnp;
		Pltpadinfo *	pip;
		for (LIST_TRAVERSE(&pltpadlist, lnp, pip))
			free(pip);
	}
	/*
	 * All objects with BOUND flag set hold definitions for the object
	 * we just relocated.  Call bound_add() to save those references.
	 */
	if (bound) {
		if (bound_add(REF_SYMBOL, lmp, 0) == 0)
			return (0);
	}

	/*
	 * If we write enabled the text segment to perform these relocations
	 * re-protect by disabling writes.
	 */
	if (textrel) {
		(void) elf_set_prot(lmp, 0);
		textrel = 0;
	}

	return (1);
}
