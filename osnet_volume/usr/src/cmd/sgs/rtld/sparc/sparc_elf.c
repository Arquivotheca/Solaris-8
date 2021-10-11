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
#pragma ident	"@(#)sparc_elf.c	1.70	99/09/14 SMI"

/*
 * SPARC machine dependent and ELF file class dependent functions.
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

/*
 * Local storage space created on the stack created for this glue
 * code includes space for:
 *		0x4	pointer to dyn_data
 *		0x4	size prev stack frame
 */
static const unsigned char dyn_plt_template[] = {
/* 0x00 */	0x80, 0x90, 0x00, 0x1e,	/* tst   %fp */
/* 0x04 */	0x02, 0x80, 0x00, 0x04, /* be    0x14 */
/* 0x08 */	0x82, 0x27, 0x80, 0x0e,	/* sub   %sp, %fp, %g1 */
/* 0x0c */	0x10, 0x80, 0x00, 0x03, /* ba	 0x20 */
/* 0x10 */	0x01, 0x00, 0x00, 0x00, /* nop */
/* 0x14 */	0x82, 0x10, 0x20, 0x60, /* mov	0x60, %g1 */
/* 0x18 */	0x9d, 0xe3, 0xbf, 0x98,	/* save	%sp, -0x68, %sp */
/* 0x1c */	0xc2, 0x27, 0xbf, 0xf8,	/* st	%g1, [%fp + -0x8] */
/* 0x20 */	0x03, 0x00, 0x00, 0x00,	/* sethi %hi(val), %g1 */
/* 0x24 */	0x82, 0x10, 0x60, 0x00, /* or	 %g1, %lo(val), %g1 */
/* 0x28 */	0x40, 0x00, 0x00, 0x00,	/* call  <rel_addr> */
/* 0x2c */	0xc2, 0x27, 0xbf, 0xfc	/* st    %g1, [%fp + -0x4] */
};

int	dyn_plt_ent_size = sizeof (dyn_plt_template) +
		sizeof (uintptr_t) +	/* reflmp */
		sizeof (uintptr_t) +	/* deflmp */
		sizeof (ulong_t) +	/* symndx */
		sizeof (ulong_t) +	/* sb_flags */
		sizeof (Sym);		/* symdef */

/*
 * the dynamic plt entry is:
 *
 *	tst	%fp
 *	be	1f
 *	nop
 *	sub	%sp, %fp, %g1
 *	ba	2f
 *	nop
 * 1:
 *	mov	SA(MINFRAME), %g1	! if %fp is null this is the
 *					!   'minimum stack'.  %fp is null
 *					!   on the initial stack frame
 * 2:
 *	save	%sp, -(SA(MINFRAME) + 2 * CLONGSIZE), %sp
 *	st	%g1, [%fp + -0x8] ! store prev_stack size in [%fp - 8]
 *	sethi	%hi(dyn_data), %g1
 *	or	%g1, %lo(dyn_data), %g1
 *	call	elf_plt_trace
 *	st	%g1, [%fp + -0x4] ! store dyn_data ptr in [%fp - 4]
 * dyn data:
 *	uintptr_t	reflmp
 *	uintptr_t	deflmp
 *	ulong_t		symndx
 *	ulong_t		sb_flags
 *	Sym		symdef
 */
static caddr_t
elf_plt_trace_write(caddr_t pc, Rt_map * rlmp, Rt_map * dlmp, Sym * sym,
	ulong_t symndx, ulong_t pltndx, caddr_t to, ulong_t sb_flags)
{
	extern ulong_t	elf_plt_trace();
	uintptr_t	dyn_plt;
	uintptr_t *	dyndata;

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
	dyn_plt = (uintptr_t)AUDINFO(rlmp)->ai_dynplts +
		(pltndx * dyn_plt_ent_size);

	/*
	 * Have we initialized this dynamic plt entry yet?  If we havn't do it
	 * now.  Otherwise this function has been called before, but from a
	 * different plt (ie. from another shared object).  In that case
	 * we just set the plt to point to the new dyn_plt.
	 */
	if (*(uint_t *)dyn_plt == 0) {
		Sym *	symp;
		Xword	symvalue;

		(void) memcpy((void *)dyn_plt, dyn_plt_template,
			sizeof (dyn_plt_template));
		/* LINTED */
		dyndata = (uintptr_t *)(dyn_plt + sizeof (dyn_plt_template));

		/*
		 * relocating:
		 *	sethi	%hi(dyndata), %g1
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_HI22, (unsigned char *)(dyn_plt + 0x20),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);
		/*
		 * relocating:
		 *	or	%g1, %lo(dyndata), %g1
		 */
		symvalue = (Xword)dyndata;
		if (do_reloc(R_SPARC_LO10, (unsigned char *)(dyn_plt + 0x24),
		    &symvalue, MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);
		/*
		 * relocating:
		 *	call	elf_plt_trace
		 */
		symvalue = (Xword)((uintptr_t)&elf_plt_trace -
			(dyn_plt + 0x28));
		if (do_reloc(R_SPARC_WDISP30, (unsigned char *)(dyn_plt + 0x28),
		    &symvalue, MSG_ORIG(MSG_SYM_ELFPLTTRACE),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		*dyndata++ = (uintptr_t)rlmp;
		*dyndata++ = (uintptr_t)dlmp;
		*(ulong_t *)dyndata++ = symndx;
		*(ulong_t *)dyndata++ = sb_flags;
		symp = (Sym *)dyndata;
		*symp = *sym;
		symp->st_name += (Word)STRTAB(dlmp);
		symp->st_value = (Addr)to;

		iflush_range((void *)dyn_plt, sizeof (dyn_plt_template));
	}

	/* LINTED */
	elf_plt_write((unsigned long *)pc, (unsigned long *)dyn_plt, 0);
	return (pc);
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
	unsigned long	addr, reloff, symval;
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
	DBG_CALL(Dbg_bind_global(NAME(lmp), from, from - ADDR(lmp), pltndx,
	    NAME(nlmp), (caddr_t)symval, (caddr_t)nsym->st_value, name));

	if ((LIST(lmp)->lm_flags | FLAGS1(lmp)) & FL1_AU_SYMBIND) {
		ulong_t	symndx = (((uintptr_t)nsym -
			(uintptr_t)SYMTAB(nlmp)) / SYMENT(nlmp));

		symval = audit_symbind(lmp, nlmp, nsym, symndx, symval,
			&sb_flags);
	}

	if (!(rtld_flags & RT_FL_NOBIND)) {
		if (((LIST(lmp)->lm_flags | FLAGS1(lmp)) &
		    (FL1_AU_PLTENTER | FL1_AU_PLTEXIT)) &&
		    AUDINFO(lmp)->ai_dynplts) {

			ulong_t	symndx = (((uintptr_t)nsym -
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


/*
 * Read and process the relocations for one link object, we assume all
 * relocation sections for loadable segments are stored contiguously in
 * the file.
 */
int
elf_reloc(Rt_map * lmp, int plt)
{
	ulong_t		relbgn, relend, relsiz, basebgn, pltbgn, pltend;
	ulong_t		roffset, rsymndx, psymndx = 0, etext, emap, dsymndx;
	unsigned char	rtype;
	long		reladd, value, pvalue;
	Sym *		symref, * psymref, * symdef, * psymdef;
	char *		name, * pname;
	Rt_map *	_lmp, * plmp;
	int		textrel = 0, bound = 0, error = 0, noplt = 0;
	long		relacount = RELACOUNT(lmp);
	Rela *		rel;

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
	if ((plt == 0) && PLTGOT(lmp))
		elf_plt_init((unsigned int *)PLTGOT(lmp), (caddr_t)lmp);

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

		rtype = ELF_R_TYPE(((Rela *)relbgn)->r_info);

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
			rtype = ELF_R_TYPE(((Rela *)relbgn)->r_info);
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
			unsigned long *	_roffset = (unsigned long *)roffset;

			_roffset++;
			if ((*_roffset & (~(S_MASK(22)))) != M_BA_A)
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
				ulong_t	pltndx = ((ulong_t)rel -
					(uintptr_t)JMPREL(lmp)) / relsiz;
				ulong_t	symndx = (((uintptr_t)symdef -
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
				DBG_CALL(Dbg_reloc_apply((Xword)roffset,
				    (Xword)value));
				elf_plt_write((unsigned long *)roffset,
				    (unsigned long *)value, 0);
			}
			break;
		default:
			value += reladd;
			/*
			 * Write the relocation out.
			 */
			if (do_reloc(rtype, (unsigned char *)roffset,
			    (Xword *)&value, name, NAME(lmp)) == 0)
				error = 1;

			/*
			 * value now contains the 'bit-shifted' value
			 * that was or'ed into memory (this was set
			 * by do_reloc()).
			 */
			DBG_CALL(Dbg_reloc_apply((Xword)roffset,
			    (Xword)value));

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
