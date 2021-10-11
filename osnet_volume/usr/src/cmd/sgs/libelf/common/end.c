/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)end.c	1.9	98/08/28 SMI" 	/* SVr4.0 1.11	*/

#pragma weak	elf_end = _elf_end


#include "syn.h"
#include <ar.h>
#include <stdlib.h>
#include "libelf.h"
#include "decl.h"
#include "member.h"


int
elf_end(Elf * elf)
{
	Elf_Scn *	s;
	Dnode *	d;
	Elf_Void *		trail = 0;
	int			rc;

	if (elf == 0)
		return (0);

	ELFWLOCK(elf)
	if (--elf->ed_activ != 0) {
		rc = elf->ed_activ;
		ELFUNLOCK(elf)
		return (rc);
	}

#ifndef __lock_lint
	while (elf->ed_activ == 0) {
		for (s = elf->ed_hdscn; s != 0; s = s->s_next) {
			if (s->s_myflags & SF_ALLOC) {
				if (trail != 0)
					free(trail);
				trail = (Elf_Void *)s;
			}

			if ((s->s_myflags & SF_READY) == 0)
				continue;
			for (d = s->s_hdnode; d != 0; ) {
				register Dnode	*t;

				if (d->db_buf != 0)
					free(d->db_buf);
				if ((t = d->db_raw) != 0) {
					if (t->db_buf != 0)
						free(t->db_buf);
					if (t->db_myflags & DBF_ALLOC)
						free(t);
				}
				t = d->db_next;
				if (d->db_myflags & DBF_ALLOC)
					free(d);
				d = t;
			}
		}
		if (trail != 0) {
			free(trail);
			trail = 0;
		}

		{
			register Memlist	*l;
			register Memident	*i;

			for (l = elf->ed_memlist; l; l = (Memlist *)trail) {
				trail = (Elf_Void *)l->m_next;
				for (i = (Memident *)(l + 1); i < l->m_free;
				    i++)
					free(i->m_member);
				free(l);
			}
		}
		if (elf->ed_myflags & EDF_EHALLOC)
			free(elf->ed_ehdr);
		if (elf->ed_myflags & EDF_PHALLOC)
			free(elf->ed_phdr);
		if (elf->ed_myflags & EDF_SHALLOC)
			free(elf->ed_shdr);
		if (elf->ed_myflags & EDF_RAWALLOC)
			free(elf->ed_raw);
		if (elf->ed_myflags & EDF_ASALLOC)
			free(elf->ed_arsym);
		if (elf->ed_myflags & EDF_ASTRALLOC)
			free(elf->ed_arstr);

		/*
		 * Don't release the image until the last reference dies.
		 * If the image was introduced via elf_memory() then
		 * we don't release it at all, it's not ours to release.
		 */

		if (elf->ed_parent == 0) {
			if (elf->ed_vm != 0)
				free(elf->ed_vm);
			else if ((elf->ed_myflags & EDF_MEMORY) == 0)
				_elf_unmap(elf->ed_image, elf->ed_imagesz);
		}
		trail = (Elf_Void *)elf;
		elf = elf->ed_parent;
		ELFUNLOCK(trail)
		free(trail);
		if (elf == 0)
			break;
		/*
		 * If parent is inactive we close
		 * it too, so we need to lock it too.
		 */
		ELFWLOCK(elf)
		--elf->ed_activ;
	}

	if (elf) {
		ELFUNLOCK(elf)
	}
#else
	/*
	 * This sill stuff is here to make warlock happy
	 * durring it's lock checking.  The problem is that it
	 * just can't track the multiple dynamic paths through
	 * the above loop so we just give it a simple one it can
	 * look at.
	 */
	_elf_unmap(elf->ed_image, elf->ed_imagesz);
	ELFUNLOCK(elf)
#endif

	return (0);
}
