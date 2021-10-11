/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sparc_a.out.c	1.34	99/05/27 SMI"

/*
 * SPARC machine dependent and a.out format file class dependent functions.
 * Contains routines for performing function binding and symbol relocations.
 */
#include	"_synonyms.h"

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/mman.h>
#include	<synch.h>
#include	<dlfcn.h>
#include	"_a.out.h"
#include	"_rtld.h"
#include	"_audit.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"

extern void	iflush_range(caddr_t, size_t);

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
aout_bndr(caddr_t pc)
{
	Rt_map *	lmp, * nlmp;
	struct relocation_info *rp;
	struct nlist *	sp;
	Sym *		sym;
	char *		name;
	int 		rndx;
	unsigned long	symval;
	int		bind;
	Slookup		sl;

	PRF_MCOUNT(3, aout_bndr);
	if ((bind = bind_guard(THR_FLG_BIND)) == 1)
		(void) rw_rdlock(&bindlock);

	for (lmp = lml_main.lm_head; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if (FCT(lmp) == &aout_fct) {
			if (pc > (caddr_t)(LM2LP(lmp)->lp_plt) &&
			    pc < (caddr_t)((int)LM2LP(lmp)->lp_plt +
				    AOUTDYN(lmp)->v2->ld_plt_sz))  {
				break;
			}
		}
	}

#define	LAST22BITS	0x3fffff

	/* LINTED */
	rndx = *(int *)(pc + (sizeof (unsigned long *) * 2)) & LAST22BITS;
	rp = &LM2LP(lmp)->lp_rp[rndx];
	sp = &LM2LP(lmp)->lp_symtab[rp->r_symbolnum];
	name = &LM2LP(lmp)->lp_symstr[sp->n_un.n_strx];

	/*
	 * Find definition for symbol.
	 */
	sl.sl_name = name;
	sl.sl_permit = PERMIT(lmp);
	sl.sl_cmap = lmp;
	sl.sl_imap = LIST(lmp)->lm_head;
	sl.sl_rsymndx = 0;
	if ((sym = aout_lookup_sym(&sl, &nlmp, LKUP_DEFT)) != 0) {
		symval = sym->st_value;
		if (!(FLAGS(nlmp) & FLG_RT_FIXED) &&
		    (sym->st_shndx != SHN_ABS))
			symval += (int)(ADDR(nlmp));
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
	DBG_CALL(Dbg_bind_global(NAME(lmp),
	    (caddr_t)(ADDR(lmp) + rp->r_address), (caddr_t)rp->r_address, -1,
	    NAME(nlmp), (caddr_t)symval, (caddr_t)sym->st_value, name));

	if (!(rtld_flags & RT_FL_NOBIND))
		aout_plt_write((caddr_t)(ADDR(lmp) + rp->r_address), symval);

	if (bind) {
		(void) rw_unlock(&bindlock);
		(void) bind_clear(THR_FLG_BIND);
	}

	if ((LIST(lmp)->lm_flags | FLAGS1(lmp)) & FL1_AU_ACTIVITY)
		audit_activity(lmp, LA_ACT_CONSISTENT);

	/*
	 * If we're returning control to the user clean up any open file
	 * descriptors that may have resulted from the implementation of this
	 * binding (bringing in a filtee for example).
	 */
	if ((rtld_flags & RT_FL_CLEANUP) && (LIST(lmp) != &lml_rtld))
		cleanup();

	return (symval);
}


#define	IS_PC_RELATIVE(X) (pc_rel_type[(X)] == 1)

static const unsigned char pc_rel_type[] = {
	0,				/* RELOC_8 */
	0,				/* RELOC_16 */
	0,				/* RELOC_32 */
	1,				/* RELOC_DISP8 */
	1,				/* RELOC_DISP16 */
	1,				/* RELOC_DISP32 */
	1,				/* RELOC_WDISP30 */
	1,				/* RELOC_WDISP22 */
	0,				/* RELOC_HI22 */
	0,				/* RELOC_22 */
	0,				/* RELOC_13 */
	0,				/* RELOC_LO10 */
	0,				/* RELOC_SFA_BASE */
	0,				/* RELOC_SFA_OFF13 */
	0,				/* RELOC_BASE10 */
	0,				/* RELOC_BASE13 */
	0,				/* RELOC_BASE22 */
	0,				/* RELOC_PC10 */
	0,				/* RELOC_PC22 */
	0,				/* RELOC_JMP_TBL */
	0,				/* RELOC_SEGOFF16 */
	0,				/* RELOC_GLOB_DAT */
	0,				/* RELOC_JMP_SLOT */
	0				/* RELOC_RELATIVE */
};

int
aout_reloc(Rt_map * lmp, int plt)
{
	int		k;		/* loop temporary */
	int		nr;		/* number of relocations */
	char *		name;		/* symbol being searched for */
	long *		et;		/* cached _etext of object */
	long		value;		/* relocation temporary */
	long *		ra;		/* cached relocation address */
	struct relocation_info *rp;	/* current relocation */
	struct nlist *	sp;		/* symbol table of "symbol" */
	Rt_map *	_lmp;		/* lm which holds symbol definition */
	Sym *		sym;		/* symbol definition */
	int		textrel = 0, bound = 0;


	/*
	 * If we've been called upon to promote an RTLD_LAZY object to an
	 * RTLD_NOW don't bother to do anything - a.out's are bound as if
	 * RTLD_NOW regardless.
	 */
	if (plt)
		return (1);

	PRF_MCOUNT(4, aout_reloc);
	DBG_CALL(Dbg_reloc_run(NAME(lmp), SHT_RELA));

	rp = LM2LP(lmp)->lp_rp;
	/* LINTED */
	et = (long *)ETEXT(lmp);
	nr = GETRELSZ(AOUTDYN(lmp)) / sizeof (struct relocation_info);

	/*
	 * Initialize _PLT_, if any.
	 */
	if (AOUTDYN(lmp)->v2->ld_plt_sz)
		aout_plt_write((caddr_t)LM2LP(lmp)->lp_plt->jb_inst,
		    (unsigned long)aout_rtbndr);

	/*
	 * Loop through relocations.
	 */
	for (k = 0; k < nr; k++, rp++) {
		/* LINTED */
		ra = (long *)&((char *)ADDR(lmp))[rp->r_address];

		/*
		 * Check to see if we're relocating in the text segment
		 * and turn off the write protect if necessary.
		 */
		if (ra < et) {
			if (!textrel) {
				if (aout_set_prot(lmp, PROT_WRITE) == 0)
					return (0);
				textrel = 1;
			}
		}

		/*
		 * Perform the relocation.
		 */
		if (rp->r_extern == 0) {
			name = (char *)0;
			value = ADDR(lmp);
		} else {
			Slookup		sl;

			if (rp->r_type == RELOC_JMP_SLOT)
				continue;
			sp = &LM2LP(lmp)->lp_symtab[rp->r_symbolnum];
			name = &LM2LP(lmp)->lp_symstr[sp->n_un.n_strx];

			/*
			 * Locate symbol.
			 */
			sl.sl_name = name;
			sl.sl_permit = PERMIT(lmp);
			sl.sl_cmap = lmp;
			sl.sl_imap = LIST(lmp)->lm_head;
			sl.sl_rsymndx = 0;
			sym = 0;
			if ((sym = aout_lookup_sym(&sl, &_lmp,
			    LKUP_DEFT)) == 0) {
				if (LIST(lmp)->lm_flags & LML_TRC_WARN) {
					(void)
					    printf(MSG_INTL(MSG_LDD_SYM_NFOUND),
					    name, NAME(lmp));
					continue;
				} else {
					eprintf(ERR_FATAL,
					    MSG_INTL(MSG_REL_NOSYM), NAME(lmp),
					    name);
					return (0);
				}
			}

			/*
			 * If symbol was found in an object other than the
			 * referencing object then set the boundto bit in
			 * the defining object.
			 */
			if ((lmp != _lmp) &&
			    (!(FLAGS(_lmp) & FLG_RT_ISMAIN))) {
				FLAGS(_lmp) |= FLG_RT_BOUND;
				bound = 1;
			}

			value = sym->st_value + rp->r_addend;
			if (!(FLAGS(_lmp) & FLG_RT_FIXED) &&
			    (sym->st_shndx != SHN_COMMON) &&
			    (sym->st_shndx != SHN_ABS))
				value += ADDR(_lmp);

			if (IS_PC_RELATIVE(rp->r_type))
				value -= (long)ADDR(lmp);

			DBG_CALL(Dbg_bind_global(NAME(lmp), (caddr_t)ra,
			    (caddr_t)(ra - ADDR(lmp)), -1, NAME(_lmp),
			    (caddr_t)value, (caddr_t)sym->st_value, name));
		}

		/*
		 * Perform a specific relocation operation.
		 */
		switch (rp->r_type) {
		case RELOC_RELATIVE:
			value += *ra << (32-22);
			*(long *)ra = (*(long *)ra & ~S_MASK(22)) |
				((value >> (32 - 22)) & S_MASK(22));
			ra++;
			value += (*ra & S_MASK(10));
			*(long *)ra = (*(long *)ra & ~S_MASK(10)) |
				(value & S_MASK(10));
			break;
		case RELOC_8:
		case RELOC_DISP8:
			value += *ra & S_MASK(8);
			if (!S_INRANGE(value, 8))
			    eprintf(ERR_FATAL, MSG_INTL(MSG_REL_OVERFLOW),
				NAME(lmp), (name ? name :
				MSG_INTL(MSG_STR_UNKNOWN)), (int)value, 8,
				(uint_t)ra);
			*ra = value;
			break;
		case RELOC_LO10:
		case RELOC_BASE10:
			value += *ra & S_MASK(10);
			*(long *)ra = (*(long *)ra & ~S_MASK(10)) |
				(value & S_MASK(10));
			break;
		case RELOC_BASE13:
		case RELOC_13:
			value += *ra & S_MASK(13);
			*(long *)ra = (*(long *)ra & ~S_MASK(13)) |
				(value & S_MASK(13));
			break;
		case RELOC_16:
		case RELOC_DISP16:
			value += *ra & S_MASK(16);
			if (!S_INRANGE(value, 16))
			    eprintf(ERR_FATAL, MSG_INTL(MSG_REL_OVERFLOW),
				NAME(lmp), (name ? name :
				MSG_INTL(MSG_STR_UNKNOWN)), (int)value, 16,
				(uint_t)ra);
			*(short *)ra = value;
			break;
		case RELOC_22:
		case RELOC_BASE22:
			value += *ra & S_MASK(22);
			if (!S_INRANGE(value, 22))
			    eprintf(ERR_FATAL, MSG_INTL(MSG_REL_OVERFLOW),
				NAME(lmp), (name ? name :
				MSG_INTL(MSG_STR_UNKNOWN)), (int)value, 22,
				(uint_t)ra);
			*(long *)ra = (*(long *)ra & ~S_MASK(22)) |
				(value & S_MASK(22));
			break;
		case RELOC_HI22:
			value += (*ra & S_MASK(22)) << (32 - 22);
			*(long *)ra = (*(long *)ra & ~S_MASK(22)) |
				((value >> (32 - 22)) & S_MASK(22));
			break;
		case RELOC_WDISP22:
			value += *ra & S_MASK(22);
			value >>= 2;
			if (!S_INRANGE(value, 22))
			    eprintf(ERR_FATAL, MSG_INTL(MSG_REL_OVERFLOW),
				NAME(lmp), (name ? name :
				MSG_INTL(MSG_STR_UNKNOWN)), (int)value, 22,
				(uint_t)ra);
			*(long *)ra = (*(long *)ra & ~S_MASK(22)) |
				(value & S_MASK(22));
			break;
		case RELOC_WDISP30:
			value += *ra & S_MASK(30);
			value >>= 2;
			*(long *)ra = (*(long *)ra & ~S_MASK(30)) |
				(value & S_MASK(30));
			break;
		case RELOC_32:
		case RELOC_GLOB_DAT:
		case RELOC_DISP32:
			value += *ra;
			*(long *)ra = value;
			break;
		default:
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNIMPL), NAME(lmp),
			    (name ? name : MSG_INTL(MSG_STR_UNKNOWN)),
			    rp->r_type);
			return (0);
			/* NOTREACHED */
		}

		/*
		 * If this relocation is against a text segment we must make
		 * sure that the instruction cache is flushed.
		 */
		if (textrel) {
			if (rp->r_type == RELOC_RELATIVE)
				iflush_range((caddr_t)(ra - 1), 0x8);
			else
				iflush_range((caddr_t)ra, 0x4);
		}
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
	 * If we write enabled the text segment to perform these relocations,
	 * re-protect by disabling writes.
	 */
	if (textrel) {
		(void) aout_set_prot(lmp, 0);
		textrel = 0;
	}

	return (1);
}
