/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident	"@(#)getdata.c	1.15	99/08/10 SMI" 	/* SVr4.0 1.16	*/


#pragma weak	elf_getdata = _elf_getdata


#include "syn.h"
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <libelf.h>
#include "decl.h"
#include "msg.h"


/*
 * Convert data from file format to memory format.
 */


static const size_t	align32[] =
{
	1,			/* ELF_T_BYTE */
	sizeof (Elf32),		/* ELF_T_ADDR */
	sizeof (Elf32),		/* ELF_T_DYN */
	sizeof (Elf32),		/* ELF_T_EHDR */
	sizeof (Elf32_Half),	/* ELF_T_HALF */
	sizeof (Elf32),		/* ELF_T_OFF */
	sizeof (Elf32),		/* ELF_T_PHDR */
	sizeof (Elf32),		/* ELF_T_RELA */
	sizeof (Elf32),		/* ELF_T_REL */
	sizeof (Elf32),		/* ELF_T_SHDR */
	sizeof (Elf32),		/* ELF_T_SWORD */
	sizeof (Elf32),		/* ELF_T_SYM */
	sizeof (Elf32),		/* ELF_T_WORD */
	sizeof (Elf32),		/* ELF_T_VERDEF */
	sizeof (Elf32),		/* ELF_T_VERNEED */
	sizeof (Elf64_Sxword),	/* ELF_T_SXWORD */
	sizeof (Elf64)		/* ELF_T_XWORD */

};

#define	Nalign32	(sizeof (align32)/sizeof (align32[0]))

static const size_t	align64[] =
{
	1,			/* ELF_T_BYTE */
	sizeof (Elf64),		/* ELF_T_ADDR */
	sizeof (Elf64),		/* ELF_T_DYN */
	sizeof (Elf64),		/* ELF_T_EHDR */
	sizeof (Elf64_Half),	/* ELF_T_HALF */
	sizeof (Elf64),		/* ELF_T_OFF */
	sizeof (Elf64),		/* ELF_T_PHDR */
	sizeof (Elf64),		/* ELF_T_RELA */
	sizeof (Elf64),		/* ELF_T_REL */
	sizeof (Elf64),		/* ELF_T_SHDR */
	sizeof (Elf64_Word),	/* ELF_T_SWORD */
	sizeof (Elf64),		/* ELF_T_SYM */
	sizeof (Elf64_Word),	/* ELF_T_WORD */
	sizeof (Elf64),		/* ELF_T_VDEF */
	sizeof (Elf64),		/* ELF_T_VNEED */
	sizeof (Elf64),		/* ELF_T_SXWORD */
	sizeof (Elf64)		/* ELF_T_XWORD */
};

#define	Nalign64	(sizeof (align64)/sizeof (align64[0]))


/*
 * Could use an array indexed by ELFCLASS*, but I'd rather
 * avoid .data over something this infrequently used.  The
 * next choice would be to add extra conditionals.
 */
#define	NALIGN(elf)	((elf->ed_class == ELFCLASS32) ? Nalign32 : Nalign64)
#define	ALIGN(elf)	((elf->ed_class == ELFCLASS32) ? align32 : align64)


Elf_Data *
_elf_locked_getdata(Elf_Scn * scn, Elf_Data * data)
{
	Dnode *		d = (Dnode *)data;
	Elf *		elf;
	Elf_Data	src;
	unsigned	work;

	assert(RW_LOCK_HELD(&(scn->s_elf->ed_rwlock)));
	assert(MUTEX_HELD(&(scn->s_mutex)));
	elf = scn->s_elf;

	if ((scn->s_myflags & SF_READY) == 0) {
		UPGRADELOCKS(elf, scn)
		/*
		 * make sure someone else didn't come along and cook
		 * this stuff.
		 */
		if ((scn->s_myflags & SF_READY) == 0)
			(void) _elf_cookscn(scn);
		DOWNGRADELOCKS(elf, scn)
	}

	if (d == 0)
		d = scn->s_hdnode;
	else
		d = d->db_next;

	if (scn->s_err != 0) {
		/*LINTED*/
		_elf_seterr((Msg)scn->s_err, 0);
		return (0);
	}

	if (d == 0) {
		return (0);
	}

	if (d->db_scn != scn) {
		_elf_seterr(EREQ_DATA, 0);
		return (0);
	}

	if (d->db_myflags & DBF_READY) {
		return (&d->db_data);
	}
	elf = scn->s_elf;

	/*
	 * Prepare return buffer.  The data comes from the memory
	 * image of the file.  "Empty" regions get an empty buffer.
	 *
	 * Only sections of an ELF_C_READ file can be not READY here.
	 * Furthermore, the input file must have been cooked or
	 * frozen by now.  Translate cooked files in place if possible.
	 */

	ELFACCESSDATA(work, _elf_work)
	d->db_data.d_version = work;
	if ((d->db_off == 0) || (d->db_fsz == 0)) {
		d->db_myflags |= DBF_READY;
		return (&d->db_data);
	}

	if (elf->ed_class == ELFCLASS32) {
		Elf32_Shdr	*sh = scn->s_shdr;
		size_t		sz = sh->sh_entsize;
		Elf_Type	t = d->db_data.d_type;

		if ((t != ELF_T_BYTE) &&
		    (sz > 1) && (sz != elf32_fsize(t, 1, elf->ed_version))) {
			_elf_seterr(EFMT_ENTSZ, 0);
			return (0);
		}
	} else if (elf->ed_class == ELFCLASS64) {
		Elf64_Shdr	*sh = scn->s_shdr;
		Elf64_Xword	sz = sh->sh_entsize;
		Elf_Type	t = d->db_data.d_type;

		if (t != ELF_T_BYTE && sz > 1 &&
		    sz != elf64_fsize(t, 1, elf->ed_version)) {
			_elf_seterr(EFMT_ENTSZ, 0);
			return (0);
		}
	} else {
		_elf_seterr(EREQ_CLASS, 0);
		return (0);
	}


	/*
	 * validate the region
	 */

	if ((d->db_off < 0) || (d->db_off >= elf->ed_fsz) ||
	    (elf->ed_fsz - d->db_off < d->db_fsz)) {
		_elf_seterr(EFMT_DATA, 0);
		return (0);
	}

	/*
	 * set up translation buffers and validate
	 */

	src.d_buf = (Elf_Void *)(elf->ed_ident + d->db_off);
	src.d_size = d->db_fsz;
	src.d_type = d->db_data.d_type;
	src.d_version = elf->ed_version;
	if (elf->ed_vm) {
		UPGRADELOCKS(elf, scn)
		if (_elf_vm(elf, (size_t)d->db_off, d->db_fsz) != OK_YES) {
			DOWNGRADELOCKS(elf, scn)
			return (0);
		}
		DOWNGRADELOCKS(elf, scn)
	}

	/*
	 * decide where to put destination
	 */

	switch (elf->ed_status) {
	case ES_COOKED:
		if ((size_t)d->db_data.d_type >= NALIGN(elf)) {
			_elf_seterr(EBUG_COOKTYPE, 0);
			return (0);
		}

		/*
		 * If the destination size (memory) is at least as
		 * big as the source size (file), reuse the space.
		 */

		if ((d->db_data.d_size <= src.d_size) &&
		    (d->db_off % ALIGN(elf)[d->db_data.d_type] == 0)) {
			d->db_data.d_buf = (Elf_Void *)(elf->ed_ident +
				d->db_off);
			break;
		}

		/*FALLTHRU*/
	case ES_FROZEN:
		if ((d->db_buf = malloc(d->db_data.d_size)) == 0) {
			_elf_seterr(EMEM_DATA, errno);
			return (0);
		}
		d->db_data.d_buf = d->db_buf;
		break;

	default:
		_elf_seterr(EBUG_COOKSTAT, 0);
		return (0);
	}

	if (elf->ed_class == ELFCLASS32) {
		if (elf32_xlatetom(&d->db_data, &src, elf->ed_encode) == 0)
			return (0);
	} else {	/* ELFCLASS64 */
		if (elf64_xlatetom(&d->db_data, &src, elf->ed_encode) == 0)
			return (0);
	}
	d->db_myflags |= DBF_READY;

	return (&d->db_data);
}

Elf_Data *
elf_getdata(Elf_Scn * scn, Elf_Data * data)
{
	Elf_Data *	rc;
	Elf *	elf;

	/*
	 * trap null args, end of list, previous buffer.
	 * SHT_NULL sections have no buffer list, so they
	 * fall out here too.
	 */
	if (scn == 0)
		return (0);

	elf = scn->s_elf;
	READLOCKS(elf, scn);
	rc = _elf_locked_getdata(scn, data);
	READUNLOCKS(elf, scn);
	return (rc);
}
