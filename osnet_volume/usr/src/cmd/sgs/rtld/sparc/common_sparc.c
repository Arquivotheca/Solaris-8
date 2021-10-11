/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)common_sparc.c	1.9	99/07/25 SMI"


#include	<stdio.h>
#include	<strings.h>
#include	<sys/elf.h>
#include	<sys/elf_SPARC.h>
#include	"_rtld.h"
#include	"_elf.h"
#include	"msg.h"
#include	"conv.h"

/*
 *
 *  Matrix of legal combinations of usage of a given register:
 *
 *	Obj1\Obj2       Scratch Named
 *	Scratch          OK      NO
 *	Named            NO      *
 *
 *  * OK if the symbols are identical, NO if they are not.  Two symbols
 *  are identical if and only if one of the following is true:
 *        A. They are both global and have the same name.
 *        B. They are both local, have the same name, and are defined in
 *        the same object.  (Note that a local symbol in one object is
 *        never identical to a local symbol in another object, even if the
 *        name is the same.)
 *
 *  Matrix of legal combinations of st_shndx for the same register symbol:
 *
 *	Obj1\Obj2       UNDEF   ABS
 *	UNDEF            OK      OK
 *	ABS              OK      NO
 */

/*
 * Test the compatiblity of two register symbols, 0 pass, >0 fail
 */
static uintptr_t
check_regsyms(Sym * sym1, const char * name1, Sym * sym2, const char * name2)
{
	if ((sym1->st_name == 0) && (sym2->st_name == 0))
		return (0);	/* scratches are always compatible */

	if ((ELF_ST_BIND(sym1->st_info) == STB_LOCAL) ||
	    (ELF_ST_BIND(sym2->st_info) == STB_LOCAL)) {
		if (sym1->st_value == sym2->st_value)
			return (1);	/* local symbol incompat */
		return (0);		/* no other prob from locals */
	}

	if (sym1->st_value == sym2->st_value) {
		/* NOTE this just avoids strcmp */
		if ((sym1->st_name == 0) || (sym2->st_name == 0))
			return (2);	/* can't match scratch to named */

		if (strcmp(name1, name2) != 0)
			return (4);	/* diff name, same register value */

		if ((sym1->st_shndx == SHN_ABS) && (sym2->st_shndx == SHN_ABS))
			return (3);	/* multiply defined */
	} else if (strcmp(name1, name2) == 0)
		return (5);	/* same name, diff register value */

	return (0);
}

int
elf_regsyms(Rt_map * lmp)
{
	Dyn *	dyn;
	Sym *	symdef;
	ulong_t	rsymndx;

	/*
	 * Scan through the .dynamic section of this object looking for all
	 * DT_REGISTER entries.  For each DT_REGISTER entry found identify the
	 * register symbol it identifies and confirm that it doesn't conflict
	 * with any other register symbols.
	 */
	for (dyn = DYN(lmp); dyn->d_tag != DT_NULL; dyn++) {
		Reglist *	rp;

		if ((dyn->d_tag != DT_SPARC_REGISTER) &&
		    (dyn->d_tag != DT_DEPRECATED_SPARC_REGISTER))
			continue;

		/*
		 * Get the local symbol table entry.
		 */
		rsymndx = dyn->d_un.d_val;
		symdef = (Sym *)((unsigned long)SYMTAB(lmp) +
				(rsymndx * SYMENT(lmp)));

		for (rp = reglist; rp; rp = rp->rl_next) {
			char *	sym1, * sym2;

			if (rp->rl_sym == symdef) {
				/*
				 * Same symbol definition - everything is a-ok.
				 */
				return (1);
			}

			sym1 = (char *)(STRTAB(rp->rl_lmp) +
			    rp->rl_sym->st_name);
			sym2 = (char *)(STRTAB(lmp) + symdef->st_name);

			if (check_regsyms(rp->rl_sym, sym1, symdef, sym2) == 0)
				continue;

			if (LIST(lmp)->lm_flags & LML_TRC_WARN) {
				(void) printf(MSG_INTL(MSG_LDD_REG_SYMCONF),
				    conv_sym_SPARC_value_str(symdef->st_value),
				    NAME(rp->rl_lmp), sym1, NAME(lmp), sym2);
			} else {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REG_SYMCONF),
				    conv_sym_SPARC_value_str(symdef->st_value),
				    NAME(rp->rl_lmp), sym1, NAME(lmp), sym2);
				return (0);
			}
		}
		if ((rp = calloc(sizeof (Reglist), 1)) == (Reglist *)0)
			return (0);
		rp->rl_lmp = lmp;
		rp->rl_sym = symdef;
		rp->rl_next = reglist;
		reglist = rp;
	}
	return (1);
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
	ulong_t roffset = ((Rela *) relbgn)->r_offset;
	Byte rtype;

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
		*((ulong_t *)roffset) +=
		    basebgn + (long)(((Rela *)relbgn)->r_addend);

		relbgn += relsiz;

		if (relbgn >= relend)
			break;

		/* LINTED */
		rtype = (Byte)ELF_R_TYPE(((Rela *)relbgn)->r_info);
		roffset = ((Rela *)relbgn)->r_offset;

	} while (rtype == R_SPARC_RELATIVE);

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
	ulong_t roffset = ((Rela *) relbgn)->r_offset;

	for (; relacount; relacount--) {
		roffset += basebgn;

		/*
		 * Perform the actual relocation.
		 */
		*((ulong_t *)roffset) =
		    basebgn + (long)(((Rela *)relbgn)->r_addend);

		relbgn += relsiz;

		roffset = ((Rela *)relbgn)->r_offset;
	}

	return (relbgn);
}
