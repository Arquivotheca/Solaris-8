/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)newdata.c	1.10	98/08/28 SMI" 	/* SVr4.0 1.2	*/

#pragma weak	elf_newdata = _elf_newdata


#include "syn.h"
#include "libelf.h"
#include "decl.h"
#include "msg.h"


Elf_Data *
elf_newdata(Elf_Scn * s)
{
	Dnode *		d;
	Elf_Data *	rc;
	Elf *		elf;
	unsigned	work;

	if (s == 0)
		return (0);
	elf = s->s_elf;
	READLOCKS(elf, s)
	if (s->s_index == SHN_UNDEF) {
		_elf_seterr(EREQ_SCNNULL, 0);
		READUNLOCKS(elf, s)
		return (0);
	}

	if ((s->s_myflags & SF_READY) == 0) {
		UPGRADELOCKS(elf, s)
		/*
		 * re-confirm that another 'thread' hasn't come along
		 * and cooked this section while the locks were
		 * obtained.
		 */
		if ((s->s_myflags & SF_READY) == 0)
			(void) _elf_cookscn(s);
		DOWNGRADELOCKS(elf, s)
	}

	/*
	 * If this is the first new node, use the one allocated
	 * in the scn itself.  Update data buffer in both cases.
	 */
	ELFACCESSDATA(work, _elf_work)
	if (s->s_hdnode == 0) {
		s->s_dnode.db_uflags |= ELF_F_DIRTY;
		s->s_dnode.db_myflags |= DBF_READY;
		s->s_hdnode = &s->s_dnode;
		s->s_tlnode = &s->s_dnode;
		s->s_dnode.db_scn = s;
		s->s_dnode.db_data.d_version = work;
		rc = &s->s_dnode.db_data;
		READUNLOCKS(elf, s)
		return (rc);
	}
	if ((d = _elf_dnode()) == 0) {
		READUNLOCKS(elf, s)
		return (0);
	}
	NOTE(NOW_INVISIBLE_TO_OTHER_THREADS(*d))
	d->db_data.d_version = work;
	d->db_scn = s;
	d->db_uflags |= ELF_F_DIRTY;
	d->db_myflags |= DBF_READY;
	s->s_tlnode->db_next = d;
	s->s_tlnode = d;
	rc = &d->db_data;
	NOTE(NOW_VISIBLE_TO_OTHER_THREADS(*d))
	READUNLOCKS(elf, s)
	return (rc);
}
