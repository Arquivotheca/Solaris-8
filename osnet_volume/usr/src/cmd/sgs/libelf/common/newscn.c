/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)newscn.c	1.10	98/08/28 SMI" 	/* SVr4.0 1.4	*/

#pragma weak	elf_newscn = _elf_newscn


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf_Scn *
elf_newscn(Elf * elf)
{
	Elf_Scn	*	tl;

	if (elf == 0)
		return (0);

	ELFWLOCK(elf)
	/*
	 * if no sections yet, the file either isn't cooked
	 * or it truly is empty.  Then allocate shdr[0]
	 */
	if ((elf->ed_hdscn == 0) && (_elf_cook(elf) != OK_YES)) {
		ELFUNLOCK(elf)
		return (0);
	}
	if (elf->ed_ehdr == 0) {
		_elf_seterr(ESEQ_EHDR, 0);
		ELFUNLOCK(elf)
		return (0);
	}

	if (elf->ed_class == ELFCLASS32) {
		Snode32	*s;

		if (elf->ed_hdscn == 0)	{
			if ((s = _elf32_snode()) == 0) {
				ELFUNLOCK(elf)
				return (0);
			}
			NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*s))
			s->sb_scn.s_elf = elf;
			elf->ed_hdscn = elf->ed_tlscn = &s->sb_scn;
			s->sb_scn.s_uflags |= ELF_F_DIRTY;
		}
		if ((s = _elf32_snode()) == 0) {
			ELFUNLOCK(elf)
			return (0);
		}
		NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*s))
		tl = elf->ed_tlscn;
		s->sb_scn.s_elf = elf;
		s->sb_scn.s_index = tl->s_index + 1;
		elf->ed_tlscn = tl->s_next = &s->sb_scn;
		((Elf32_Ehdr *)elf->ed_ehdr)->e_shnum
		    /* LINTED */
		    = (Elf32_Half)(tl->s_index + 2);
		s->sb_scn.s_uflags |= ELF_F_DIRTY;
		tl = &s->sb_scn;
		NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*s))
		ELFUNLOCK(elf)
		return (tl);
	} else if (elf->ed_class == ELFCLASS64) {
		Snode64	*s;

		if (elf->ed_hdscn == 0)	{
			if ((s = _elf64_snode()) == 0) {
				ELFUNLOCK(elf)
				return (0);
			}
			NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*s))
			s->sb_scn.s_elf = elf;
			elf->ed_hdscn = elf->ed_tlscn = &s->sb_scn;
			s->sb_scn.s_uflags |= ELF_F_DIRTY;
		}
		if ((s = _elf64_snode()) == 0) {
			ELFUNLOCK(elf)
			return (0);
		}
		NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*s))
		tl = elf->ed_tlscn;
		s->sb_scn.s_elf = elf;
		s->sb_scn.s_index = tl->s_index + 1;
		elf->ed_tlscn = tl->s_next = &s->sb_scn;
		((Elf64_Ehdr *)elf->ed_ehdr)->e_shnum
		    /* LINTED */
		    = (Elf64_Half)(tl->s_index + 2);
		s->sb_scn.s_uflags |= ELF_F_DIRTY;
		tl = &s->sb_scn;
		NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*s))
		ELFUNLOCK(elf)
		return (tl);
	} else {
		_elf_seterr(EREQ_CLASS, 0);
		ELFUNLOCK(elf)
		return (0);
	}
}
