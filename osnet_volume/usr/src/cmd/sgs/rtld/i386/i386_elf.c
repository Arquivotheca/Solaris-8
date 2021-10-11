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
#pragma ident	"@(#)i386_elf.c	1.64	99/09/14 SMI"

/*
 * x86 machine dependent and ELF file class dependent functions.
 * Contains routines for performing function binding and symbol relocations.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/elf.h>
#include	<sys/elf_386.h>
#include	<sys/mman.h>
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


extern void	elf_rtbndr(Rt_map *, unsigned long, caddr_t);

static const unsigned char dyn_plt_template[] = {
/* 0x0 */  0xb8, 0x00, 0x00, 0x00, 0x00,	/* movl trace_fields, %eax */
/* 0x5 */  0xe9, 0xfc, 0xff, 0xff, 0xff, 0xff	/* jmp  elf_plt_trace */
};
int	dyn_plt_ent_size = sizeof (dyn_plt_template);

/*
 * the dynamic plt entry is:
 *
 *	movl	tfp, %eax
 *	jmp	elf_plt_trace
 * dyn_data:
 *	.align  4
 *	uintptr_t	reflmp
 *	uintptr_t	deflmp
 *	uint_t		symndx
 *	uint_t		sb_flags
 *	Sym		symdef
 */
static caddr_t
elf_plt_trace_write(uint_t roffset, Rt_map * rlmp, Rt_map * dlmp, Sym * sym,
    uint_t symndx, uint_t pltndx, caddr_t to, uint_t sb_flags)
{
	extern int	elf_plt_trace();
	unsigned long	got_entry;
	unsigned char *	dyn_plt;
	uintptr_t *	dyndata;

	/*
	 * We only need to add the glue code if there is an auditing
	 * library that is interesting in this bindings.
	 */
	dyn_plt = (unsigned char *)((uintptr_t)AUDINFO(rlmp)->ai_dynplts +
		(pltndx * dyn_plt_ent_size));

	/*
	 * Have we initialized this dynamic plt entry yet?  If we haven't do it
	 * now.  Otherwise this function has been called before, but from a
	 * different plt (ie. from another shared object).  In that case
	 * we just set the plt to point to the new dyn_plt.
	 */
	if (*dyn_plt == 0) {
		Sym *	symp;
		Word	symvalue;

		(void) memcpy(dyn_plt, dyn_plt_template,
		    sizeof (dyn_plt_template));

		/* LINTED */
		dyndata = (uintptr_t *)((uintptr_t)dyn_plt +
			ROUND(sizeof (dyn_plt_template), 4));


		/*
		 * relocate:
		 *	movl	dyn_data, %ebx
		 */
		symvalue = (Word)dyndata;
		if (do_reloc(R_386_32, &dyn_plt[1], &symvalue,
		    MSG_ORIG(MSG_SYM_LADYNDATA),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		/*
		 * jmps are relative, so I need to figure out the relative
		 * address to elf_plt_trace.
		 *
		 * relocating:
		 *	jmp	elf_plt_trace
		 */
		symvalue = (unsigned long)(elf_plt_trace) -
			(unsigned long)(dyn_plt + 6);
		if (do_reloc(R_386_PC32, &dyn_plt[6], &symvalue,
		    MSG_ORIG(MSG_SYM_ELFPLTTRACE),
		    MSG_ORIG(MSG_SPECFIL_DYNPLT)) == 0)
			exit(1);

		*dyndata++ = (uintptr_t)rlmp;
		*dyndata++ = (uintptr_t)dlmp;
		*dyndata++ = (uint_t)symndx;
		*dyndata++ = (uint_t)sb_flags;
		symp = (Sym *)dyndata;
		*symp = *sym;
		symp->st_name += (Word)STRTAB(dlmp);
		symp->st_value = (Addr)to;
	}

	got_entry = (unsigned long)roffset;
	if (!(FLAGS(rlmp) & FLG_RT_FIXED))
		got_entry += ADDR(rlmp);

	*(unsigned long *)got_entry = (unsigned long)dyn_plt;
	return ((caddr_t)dyn_plt);
}


/*
 * Function binding routine - invoked on the first call to a function through
 * the procedure linkage table;
 * passes first through an assembly language interface.
 *
 * Takes the offset into the relocation table of the associated
 * relocation entry and the address of the link map (rt_private_map struct)
 * for the entry.
 *
 * Returns the address of the function referenced after re-writing the PLT
 * entry to invoke the function directly.
 *
 * On error, causes process to terminate with a signal.
 */
/* ARGSUSED2 */
unsigned long
elf_bndr(Rt_map * lmp, unsigned long reloff, caddr_t from)
{
	Rt_map *	nlmp, ** tobj = 0;
	unsigned long	addr, symval;
	char *		name;
	Rel *		rptr;
	Sym *		sym, * nsym;
	int		bind;
	uint_t		sb_flags = 0;
	unsigned long	rsymndx;
	Slookup		sl;

	PRF_MCOUNT(1, elf_bndr);
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_wrlock(&bindlock);

	/*
	 * Perform some basic sanity checks.  If we didn't get a load map or
	 * the relocation offset is invalid then its possible someone has walked
	 * over the .got entries.  Note, if the relocation offset is invalid
	 * we can't depend on it to deduce the plt offset, so use -1 instead -
	 * at this point you have to wonder if we can depended on anything.
	 */
	if (!lmp || ((reloff % sizeof (Rel)) != 0)) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_PLTENTRY),
		    conv_reloc_386_type_str(R_386_JMP_SLOT),
		    EC_XWORD((!lmp ? (reloff / sizeof (Rel)) : -1)),
		    EC_ADDR(from),
		    (!lmp ? MSG_INTL(MSG_REL_PLTREF) :
		    MSG_INTL(MSG_REL_RELOFF)));
		exit(1);
	}

	/*
	 * Use relocation entry to get symbol table entry and symbol name.
	 */
	addr = (unsigned long)JMPREL(lmp);
	rptr = (Rel *)(addr + reloff);
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
	    EC_XWORD((reloff / sizeof (Rel))), NAME(nlmp), (caddr_t)symval,
	    (caddr_t)nsym->st_value, name));

	if ((LIST(lmp)->lm_flags | FLAGS1(lmp)) & FL1_AU_SYMBIND) {
		uint_t	symndx = (((uintptr_t)nsym -
			(uintptr_t)SYMTAB(nlmp)) / SYMENT(nlmp));
		symval = audit_symbind(lmp, nlmp, nsym, symndx, symval,
			&sb_flags);
	}

	if (!(rtld_flags & RT_FL_NOBIND)) {
		if (((LIST(lmp)->lm_flags | FLAGS1(lmp)) &
		    (FL1_AU_PLTENTER | FL1_AU_PLTEXIT)) &&
		    AUDINFO(lmp)->ai_dynplts) {
			uint_t	pltndx = reloff / sizeof (Rel);
			uint_t	symndx = (((int)nsym -
						(int)SYMTAB(nlmp)) /
						SYMENT(nlmp));

			symval = (unsigned long)elf_plt_trace_write(
				rptr->r_offset, lmp,
				nlmp, nsym, symndx, pltndx, (caddr_t)symval,
				sb_flags);
		} else {
			/*
			 * Write standard PLT entry to jump directly
			 * to newly bound function.
			 */
			addr = rptr->r_offset;
			if (!(FLAGS(lmp) & FLG_RT_FIXED))
				addr += ADDR(lmp);
			*(unsigned long *)addr = symval;
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
 * When the relocation loop realizes that it's dealing with relative
 * relocations in a shared object, it breaks into this tighter loop
 * as an optimization.
 */
ulong_t
elf_reloc_relative(ulong_t relbgn, ulong_t relend, ulong_t relsiz,
    ulong_t basebgn, ulong_t etext, ulong_t emap)
{
	ulong_t roffset = ((Rel *)relbgn)->r_offset;
	char rtype;

	do {
		roffset += basebgn;

		/*
		 * If this relocation is against an address not mapped in,
		 * then break out of the relative relocation loop, falling
		 * back on the main relocation loop.
		 */
		if (roffset < etext || roffset > emap)
			break;

		/*
		 * Perform the actual relocation.
		 */
		*((ulong_t *)roffset) += basebgn;

		relbgn += relsiz;

		if (relbgn >= relend)
			break;

		rtype = ELF_R_TYPE(((Rel *)relbgn)->r_info);
		roffset = ((Rel *)relbgn)->r_offset;

	} while (rtype == R_386_RELATIVE);

	return (relbgn);
}

/*
 * This is the tightest loop for RELATIVE relocations for those
 * objects built with the DT_RELACOUNT .dynamic entry.
 */
ulong_t
elf_reloc_relacount(ulong_t relbgn, ulong_t relacount, ulong_t relsiz,
    ulong_t basebgn)
{
	ulong_t roffset = ((Rel *) relbgn)->r_offset;

	for (; relacount; relacount--) {
		roffset += basebgn;

		/*
		 * Perform the actual relocation.
		 */
		*((ulong_t *)roffset) += basebgn;

		relbgn += relsiz;

		roffset = ((Rel *)relbgn)->r_offset;

	}

	return (relbgn);
}

/*
 * Read and process the relocations for one link object, we assume all
 * relocation sections for loadable segments are stored contiguously in
 * the file.
 */
int
elf_reloc(Rt_map * lmp, int plt)
{
	ulong_t		relbgn, relend, relsiz, basebgn;
	ulong_t		pltbgn, pltend, _pltbgn, _pltend;
	ulong_t		roffset, rsymndx, psymndx = 0, etext, emap;
	ulong_t		dsymndx;
	unsigned char	rtype;
	long		value, pvalue;
	Sym *		symref, * psymref, * symdef, * psymdef;
	char *		name, * pname;
	Rt_map *	_lmp, * plmp;
	int		textrel = 0, bound = 0, error = 0, noplt = 0;
	int		relacount = RELACOUNT(lmp), plthint = 0;
	Rel *		rel;

	PRF_MCOUNT(2, elf_reloc);

	/*
	 * Although only necessary for lazy binding, initialize the first
	 * global offset entry to go to elf_rtbndr().  dbx(1) seems
	 * to find this useful.
	 */
	if ((plt == 0) && PLTGOT(lmp))
		elf_plt_init((unsigned int *)PLTGOT(lmp), (caddr_t)lmp);

	/*
	 * Initialize the plt start and end addresses.
	 */
	if ((pltbgn = (unsigned long)JMPREL(lmp)) != 0)
		pltend = pltbgn + (unsigned long)(PLTRELSZ(lmp));


	relsiz = (unsigned long)(RELENT(lmp));
	basebgn = ADDR(lmp);
	etext = ETEXT(lmp);
	emap = ADDR(lmp) + MSIZE(lmp);

	if (PLTRELSZ(lmp))
		plthint = PLTRELSZ(lmp) / relsiz;

	/*
	 * If we've been called upon to promote an RTLD_LAZY object to an
	 * RTLD_NOW then we're only interested in scaning the .plt table.
	 * An uninitialized .plt is the case where the associated got entry
	 * points back to the plt itself.  Determine the range of the real .plt
	 * entries using the _PROCEDURE_LINKAGE_TABLE_ symbol.
	 */
	if (plt) {
		relbgn = pltbgn;
		relend = pltend;
		if (!relbgn || (relbgn == relend))
			return (1);

		if ((symdef = elf_find_sym(MSG_ORIG(MSG_SYM_PLT), lmp, &_lmp, 0,
		    elf_hash(MSG_ORIG(MSG_SYM_PLT)))) == 0)
			return (1);

		_pltbgn = symdef->st_value;
		if (!(FLAGS(lmp) & FLG_RT_FIXED) &&
		    (symdef->st_shndx != SHN_ABS))
			_pltbgn += basebgn;
		_pltend = _pltbgn + (((PLTRELSZ(lmp) / relsiz)) *
			M_PLT_ENTSIZE) + M_PLT_RESERVSZ;

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

	DBG_CALL(Dbg_reloc_run(NAME(lmp), M_DYNREL_SHT_TYPE));

	/*
	 * If we're processing a dynamic executable in lazy mode there is no
	 * need to scan the .rel.plt table, however if we're processing a shared
	 * object in lazy mode the .got addresses associated to each .plt must
	 * be relocated to reflect the location of the shared object.
	 */
	if (pltbgn && !(MODE(lmp) & RTLD_NOW) && (FLAGS(lmp) & FLG_RT_FIXED))
		noplt = 1;

	/*
	 * Loop through relocations.
	 */
	while (relbgn < relend) {
		uint_t	sb_flags = 0;

		rtype = ELF_R_TYPE(((Rel *)relbgn)->r_info);

		/*
		 * If this is a RELATIVE relocation in a shared object
		 * (the common case), and if we are not debuggin, then
		 * jump into a tighter relocation loop (elf_reloc_relative)
		 * Only make the jump if we've been given a hint on
		 * the number of relocations.
		 */
		if (relacount && (rtype == R_386_RELATIVE) &&
		    !(FLAGS(lmp) & FLG_RT_FIXED) && !dbg_mask) {
			if (relacount) {
				relbgn = elf_reloc_relacount(relbgn,
					relacount, relsiz, basebgn);

				if (relbgn >= relend)
					break;
				/*
				 * We only do this once.
				 */
				relacount = 0;
				continue;
			} else {
				relbgn = elf_reloc_relative(relbgn, relend,
					relsiz, basebgn, etext, emap);
				if (relbgn >= relend)
					break;
				rtype = ELF_R_TYPE(((Rel *)relbgn)->r_info);
			}
		}

		roffset = ((Rel *)relbgn)->r_offset;

		/*
		 * If this is a shared object, add the base address to offset.
		 */
		if (!(FLAGS(lmp) & FLG_RT_FIXED)) {


			/*
			 * If we're processing lazy bindings, we have to step
			 * through the plt entries and add the base address
			 * to the corresponding got entry.
			 */
			if (plthint && (plt == 0) &&
			    (rtype == R_386_JMP_SLOT) &&
			    !(MODE(lmp) & RTLD_NOW)) {
				relbgn = elf_reloc_relacount(relbgn,
				    plthint, relsiz, basebgn);
				plthint = 0;
				continue;
			}
			roffset += basebgn;
		}

		rsymndx = ELF_R_SYM(((Rel *)relbgn)->r_info);
		rel = (Rel *)relbgn;
		relbgn += relsiz;

		/*
		 * Optimizations.
		 */
		if (rtype == R_386_NONE)
			continue;
		if (noplt && ((unsigned long)rel >= pltbgn) &&
		    ((unsigned long)rel < pltend)) {
			relbgn = pltend;
			continue;
		}


		/*
		 * If we're promoting plts determine if this one has already
		 * been written.
		 */
		if (plt) {
			if ((*(unsigned long *)roffset < _pltbgn) ||
			    (*(unsigned long *)roffset > _pltend))
				continue;
		}

		/*
		 * If this relocation is not against part of the image
		 * mapped into memory we skip it.
		 */
		if ((roffset < ADDR(lmp)) || (roffset > (ADDR(lmp) +
		    MSIZE(lmp))))
			continue;

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
				    (rtype != R_386_COPY)) {
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

					if (rtype == R_386_COPY)
						flags = LKUP_COPY;
					else
						flags = 0;

					if (rtype == R_386_JMP_SLOT)
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
					if (rtype != R_386_COPY) {
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
				    (caddr_t)(roffset - basebgn), EC_XWORD(-1),
				    NAME(_lmp), (caddr_t)value,
				    (caddr_t)symdef->st_value, name));
			}
		} else {
			value = basebgn;
			name = (char *)0;
		}

		/*
		 * If this object has relocations in the text segment, turn
		 * off the write protect.
		 */
		if (roffset < etext) {
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
		case R_386_COPY:
			if (elf_copy_reloc(name, symref, lmp, (void *)roffset,
			    symdef, _lmp, (const void *)value) == 0)
				error = 1;
			break;
		case R_386_JMP_SLOT:
			if (((LIST(lmp)->lm_flags | FLAGS1(lmp)) &
			    (FL1_AU_PLTENTER | FL1_AU_PLTEXIT)) &&
			    AUDINFO(lmp)->ai_dynplts) {
				int	pltndx = (((ulong_t)rel -
					(uintptr_t)JMPREL(lmp)) / relsiz);
				int	symndx = (((int)symdef -
					    (int)SYMTAB(_lmp)) / SYMENT(_lmp));

				(void) elf_plt_trace_write(roffset, lmp,
					_lmp, symdef, symndx, pltndx,
					(caddr_t)value, sb_flags);

			} else {
				/*
				 * Write standard PLT entry to jump directly
				 * to newly bound function.
				 */
				DBG_CALL(Dbg_reloc_apply((Xword)roffset,
				    (Xword)value));
				*(unsigned long *)roffset = value;
			}
			break;
		default:
			/*
			 * Write the relocation out.
			 */
			if (do_reloc(rtype, (unsigned char *)roffset,
			    (Word *)&value, name, NAME(lmp)) == 0)
				error = 1;

			DBG_CALL(Dbg_reloc_apply((Xword)roffset,
			    (Xword)value));
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

/*
 * Initialize the first few got entries so that function calls go to
 * elf_rtbndr:
 *
 *	GOT[GOT_XLINKMAP] =	the address of the link map
 *	GOT[GOT_XRTLD] =	the address of rtbinder
 */
void
elf_plt_init(unsigned int *got, caddr_t l)
{
	unsigned int *	_got;
	/* LINTED */
	Rt_map *	lmp = (Rt_map *)l;

	_got = got + M_GOT_XLINKMAP;
	*_got = (unsigned int)lmp;
	_got = got + M_GOT_XRTLD;
	*_got = (unsigned int)elf_rtbndr;
}

/*
 * For SVR4 Intel compatability.  USL uses /usr/lib/libc.so.1 as the run-time
 * linker, so the interpreter's address will differ from /usr/lib/ld.so.1.
 * Further, USL has special _iob[] and _ctype[] processing that makes up for the
 * fact that these arrays do not have associated copy relocations.  So we try
 * and make up for that here.  Any relocations found will be added to the global
 * copy relocation list and will be processed in setup().
 */
static int
_elf_copy_reloc(const char * name, Rt_map * rlmp, Rt_map * dlmp)
{
	Sym *		symref, * symdef;
	caddr_t 	ref, def;
	Rt_map *	_lmp;
	Rel		rel;
	int		error;
	Slookup		sl;

	/*
	 * Determine if the special symbol exists as a reference in the dynamic
	 * executable, and that an associated definition exists in libc.so.1.
	 */
	sl.sl_name = name;
	sl.sl_permit = PERMIT(rlmp);
	sl.sl_cmap = rlmp;
	sl.sl_imap = rlmp;
	sl.sl_rsymndx = 0;
	if ((symref = lookup_sym(&sl, &_lmp,
	    (LKUP_DEFT | LKUP_FIRST))) == 0)
		return (1);

	sl.sl_imap = dlmp;
	if ((symdef = lookup_sym(&sl, &_lmp,
	    (LKUP_DEFT))) == 0)
		return (1);
	if (strcmp(NAME(_lmp), MSG_ORIG(MSG_PTH_LIBC)))
		return (1);

	/*
	 * Determine the reference and definition addresses.
	 */
	ref = (void *)(symref->st_value);
	if (!(FLAGS(rlmp) & FLG_RT_FIXED))
		ref += ADDR(rlmp);
	def = (void *)(symdef->st_value);
	if (!(FLAGS(_lmp) & FLG_RT_FIXED))
		def += ADDR(_lmp);

	/*
	 * Set up a relocation entry for debugging and call the generic copy
	 * relocation function to provide symbol size error checking and to
	 * actually perform the relocation.
	 */
	rel.r_offset = (Addr)ref;
	rel.r_info = (Word)R_386_COPY;
	DBG_CALL(Dbg_reloc_in(M_MACH, M_DYNREL_SHT_TYPE, &rel, name, 0));

	error = elf_copy_reloc((char *)name, symref, rlmp, (void *)ref, symdef,
	    _lmp, (void *)def);

	if (!(FLAGS(_lmp) & FLG_RT_COPYTOOK)) {
		if (list_append(&COPY(rlmp), &COPY(_lmp)) == 0)
			error = 1;
		FLAGS(_lmp) |= FLG_RT_COPYTOOK;
	}
	return (error);
}

int
_elf_copy_gen(Rt_map * lmp)
{
	if (interp && ((unsigned long)interp->i_faddr != r_debug.r_ldbase) &&
	    !(strcmp(interp->i_name, MSG_ORIG(MSG_PTH_LIBC)))) {
		DBG_CALL(Dbg_reloc_run(pr_name, M_DYNREL_SHT_TYPE));
		if (_elf_copy_reloc(MSG_ORIG(MSG_SYM_CTYPE), lmp,
		    (Rt_map *)NEXT(lmp)) == 0)
			return (0);
		if (_elf_copy_reloc(MSG_ORIG(MSG_SYM_IOB), lmp,
		    (Rt_map *)NEXT(lmp)) == 0)
			return (0);
	}
	return (1);
}

/*
 * Plt writing interface to allow debugging initialization to be generic.
 */
void
/* ARGSUSED2 */
elf_plt_write(unsigned long * pc, unsigned long * symval, unsigned long * gp)
{
	*(unsigned long *)pc = (unsigned long)symval;
}
