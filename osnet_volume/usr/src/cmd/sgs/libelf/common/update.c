/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)update.c	1.21	99/08/10 SMI" 	/* SVr4.0 1.17	*/

#include "syn.h"
#include <memory.h>
#include <malloc.h>
#include <limits.h>
#include "decl.h"
#include "msg.h"

/*
 * This module is compiled twice, the second time having
 * -D_ELF64 defined.  The following set of macros, along
 * with machelf.h, represent the differences between the
 * two compilations.  Be careful *not* to add any class-
 * dependent code (anything that has elf32 or elf64 in the
 * name) to this code without hiding it behind a switch-
 * able macro like these.
 */
#if	defined(_ELF64)
#define	FSZ_LONG	ELF64_FSZ_XWORD
#define	ELFCLASS	ELFCLASS64
#define	_elf_snode_init	_elf64_snode_init

#define	_elfxx_cookscn	_elf64_cookscn
#define	_elf_upd_lib	_elf64_upd_lib
#define	elf_fsize	elf64_fsize
#define	_elf_entsz	_elf64_entsz
#define	_elf_msize	_elf64_msize
#define	_elf_upd_usr	_elf64_upd_usr
#define	wrt		wrt64
#define	elf_xlatetof	elf64_xlatetof
#define	_elfxx_update	_elf64_update
#else	/* ELF32 */
#pragma weak		elf_update	= _elf_update

#define	FSZ_LONG	ELF32_FSZ_WORD
#define	ELFCLASS	ELFCLASS32
#define	_elf_snode_init	_elf32_snode_init
#define	_elfxx_cookscn	_elf32_cookscn
#define	_elf_upd_lib	_elf32_upd_lib
#define	elf_fsize	elf32_fsize
#define	_elf_entsz	_elf32_entsz
#define	_elf_msize	_elf32_msize
#define	_elf_upd_usr	_elf32_upd_usr
#define	wrt		wrt32
#define	elf_xlatetof	elf32_xlatetof
#define	_elfxx_update	_elf32_update

#endif /* ELF64 */


/*
 * Output file update
 *	These functions walk an Elf structure, update its information,
 *	and optionally write the output file.  Because the application
 *	may control of the output file layout, two upd_... routines
 *	exist.  They're similar but too different to merge cleanly.
 *
 *	The library defines a "dirty" bit to force parts of the file
 *	to be written on update.  These routines ignore the dirty bit
 *	and do everything.  A minimal update routine might be useful
 *	someday.
 */


static size_t
_elf_upd_lib(Elf * elf)
{
	NOTE(ASSUMING_PROTECTED(*elf))
	Lword		hi;
	Lword		hibit;
	Elf_Scn *	s;
	register Xword	sz;
	Ehdr *		eh = elf->ed_ehdr;
	unsigned	ver = eh->e_version;
	register char	*p = (char *)eh->e_ident;


	/*
	 * Ehdr and Phdr table go first
	 */
	p[EI_MAG0] = ELFMAG0;
	p[EI_MAG1] = ELFMAG1;
	p[EI_MAG2] = ELFMAG2;
	p[EI_MAG3] = ELFMAG3;
	p[EI_CLASS] = ELFCLASS;
	/* LINTED */
	p[EI_VERSION] = (Byte)ver;
	hi = elf_fsize(ELF_T_EHDR, 1, ver);
	/* LINTED */
	eh->e_ehsize = (Half)hi;
	if (eh->e_phnum != 0) {
		/* LINTED */
		eh->e_phentsize = (Half)elf_fsize(ELF_T_PHDR, 1, ver);
		/* LINTED */
		eh->e_phoff = (Off)hi;
		hi += eh->e_phentsize * eh->e_phnum;
	} else {
		eh->e_phoff = 0;
		eh->e_phentsize = 0;
	}


	/*
	 * Loop through sections, skipping index zero.
	 * Compute section size before changing hi.
	 * Allow null buffers for NOBITS.
	 */

	if ((s = elf->ed_hdscn) == 0)
		eh->e_shnum = 0;
	else {
		eh->e_shnum = 1;
		*(Shdr *)s->s_shdr = _elf_snode_init.sb_shdr;
		s = s->s_next;
	}

	hibit = 0;
	for (; s != 0; s = s->s_next) {
		register Dnode	*d;
		register Lword	fsz, j;
		Shdr *sh = s->s_shdr;

		++eh->e_shnum;
		if (sh->sh_type == SHT_NULL) {
			*sh = _elf_snode_init.sb_shdr;
			continue;
		}

		if ((s->s_myflags & SF_READY) == 0)
			(void) _elfxx_cookscn(s);

		sh->sh_addralign = 1;
		if ((sz = (Xword)_elf_entsz(sh->sh_type, ver)) != 0)
			/* LINTED */
			sh->sh_entsize = (Half)sz;
		sz = 0;
		for (d = s->s_hdnode; d != 0; d = d->db_next) {
			if ((fsz = elf_fsize(d->db_data.d_type,
			    1, ver)) == 0)
				return (0);

			j = _elf_msize(d->db_data.d_type, ver);
			fsz *= (d->db_data.d_size / j);
			d->db_osz = (size_t)fsz;
			if ((j = d->db_data.d_align) > 1) {
				if (j > sh->sh_addralign)
					sh->sh_addralign = (Xword)j;

				if (sz % j != 0)
					sz += j - sz % j;
			}
			d->db_data.d_off = (off_t)sz;
			d->db_xoff = sz;
			sz += (Xword)fsz;
		}

		sh->sh_size = sz;
		/*
		 * We want to take into account the offsets for NOBITS
		 * sections and let the "sh_offsets" point to where
		 * the section would 'conceptually' fit within
		 * the file (as required by the ABI).
		 *
		 * But - we must also make sure that the NOBITS does
		 * not take up any actual space in the file.  We preserve
		 * the actual offset into the file in the 'hibit' variable.
		 * When we come to the first non-NOBITS section after a
		 * encountering a NOBITS section the hi counter is restored
		 * to its proper place in the file.
		 */
		if (sh->sh_type == SHT_NOBITS) {
			if (hibit == 0)
				hibit = hi;
		} else {
			if (hibit) {
				hi = hibit;
				hibit = 0;
			}
		}
		j = sh->sh_addralign;
		if ((fsz = hi % j) != 0)
			hi += j - fsz;

		/* LINTED */
		sh->sh_offset = (Off)hi;
		hi += sz;
	}

	/*
	 * if last section was a 'NOBITS' section then we need to
	 * restore the 'hi' counter to point to the end of the last
	 * non 'NOBITS' section.
	 */
	if (hibit) {
		hi = hibit;
		hibit = 0;
	}

	/*
	 * Shdr table last
	 */
	if (eh->e_shnum != 0) {
		if (hi % FSZ_LONG != 0)
			hi += FSZ_LONG - hi % FSZ_LONG;
		/* LINTED */
		eh->e_shoff = (Off)hi;
		/* LINTED */
		eh->e_shentsize = (Half)elf_fsize(ELF_T_SHDR, 1, ver);
		hi += eh->e_shentsize * eh->e_shnum;
	} else {
		eh->e_shoff = 0;
		eh->e_shentsize = 0;
	}

#if	!(defined(_LP64) && defined(_ELF64))
	if (hi > INT_MAX) {
		_elf_seterr(EFMT_FBIG, 0);
		return (0);
	}
#endif

	return ((size_t)hi);
}



static size_t
_elf_upd_usr(Elf * elf)
{
	NOTE(ASSUMING_PROTECTED(*elf))
	Lword		hi;
	Elf_Scn *	s;
	register Xword	sz;
	Ehdr *		eh = elf->ed_ehdr;
	unsigned	ver = eh->e_version;
	register char	*p = (char *)eh->e_ident;


	/*
	 * Ehdr and Phdr table go first
	 */
	p[EI_MAG0] = ELFMAG0;
	p[EI_MAG1] = ELFMAG1;
	p[EI_MAG2] = ELFMAG2;
	p[EI_MAG3] = ELFMAG3;
	p[EI_CLASS] = ELFCLASS;
	/* LINTED */
	p[EI_VERSION] = (Byte)ver;
	hi = elf_fsize(ELF_T_EHDR, 1, ver);
	/* LINTED */
	eh->e_ehsize = (Half)hi;

	/*
	 * If phnum is zero, phoff "should" be zero too,
	 * but the application is responsible for it.
	 * Allow a non-zero value here and update the
	 * hi water mark accordingly.
	 */

	if (eh->e_phnum != 0)
		/* LINTED */
		eh->e_phentsize = (Half)elf_fsize(ELF_T_PHDR, 1, ver);
	else
		eh->e_phentsize = 0;
	if ((sz = eh->e_phoff + eh->e_phentsize * eh->e_phnum) > hi)
		hi = sz;

	/*
	 * Loop through sections, skipping index zero.
	 * Compute section size before changing hi.
	 * Allow null buffers for NOBITS.
	 */

	if ((s = elf->ed_hdscn) == 0)
		eh->e_shnum = 0;
	else {
		eh->e_shnum = 1;
		*(Shdr*)s->s_shdr = _elf_snode_init.sb_shdr;
		s = s->s_next;
	}
	for (; s != 0; s = s->s_next) {
		register Dnode	*d;
		register Xword	fsz, j;
		Shdr *sh = s->s_shdr;

		if ((s->s_myflags & SF_READY) == 0)
			(void) _elfxx_cookscn(s);

		++eh->e_shnum;
		sz = 0;
		for (d = s->s_hdnode; d != 0; d = d->db_next) {
			if ((fsz = (Xword)elf_fsize(d->db_data.d_type, 1,
			    ver)) == 0)
				return (0);
			j = (Xword)_elf_msize(d->db_data.d_type, ver);
			fsz *= (Xword)(d->db_data.d_size / j);
			d->db_osz = (size_t)fsz;

			if ((sh->sh_type != SHT_NOBITS) &&
			((j = (Xword)(d->db_data.d_off + d->db_osz)) > sz))
				sz = j;
		}
		if (sh->sh_size < sz) {
			_elf_seterr(EFMT_SCNSZ, 0);
			return (0);
		}
		if ((sh->sh_type != SHT_NOBITS) &&
		    (hi < sh->sh_offset + sh->sh_size))
			hi = sh->sh_offset + sh->sh_size;
	}

	/*
	 * Shdr table last.  Comment above for phnum/phoff applies here.
	 */
	if (eh->e_shnum != 0)
		/* LINTED */
		eh->e_shentsize = (Half)elf_fsize(ELF_T_SHDR, 1, ver);
	else
		eh->e_shentsize = 0;

	if ((sz = eh->e_shoff + eh->e_shentsize * eh->e_shnum) > hi)
		hi = sz;

#if	!(defined(_LP64) && defined(_ELF64))
	if (hi > INT_MAX) {
		_elf_seterr(EFMT_FBIG, 0);
		return (0);
	}
#endif

	return ((size_t)hi);
}


static size_t
wrt(Elf * elf, Xword outsz, unsigned fill, int update_cmd)
{
	NOTE(ASSUMING_PROTECTED(*elf))
	Elf_Data	dst, src;
	unsigned	flag;
	Xword		hi, sz;
	char		*image;
	Elf_Scn		*s;
	Ehdr		*eh = elf->ed_ehdr;
	unsigned	ver = eh->e_version;
	unsigned	encode = eh->e_ident[EI_DATA];
	int		byte;

	/*
	 * Two issues can cause trouble for the output file.
	 * First, begin() with ELF_C_RDWR opens a file for both
	 * read and write.  On the write update(), the library
	 * has to read everything it needs before truncating
	 * the file.  Second, using mmap for both read and write
	 * is too tricky.  Consequently, the library disables mmap
	 * on the read side.  Using mmap for the output saves swap
	 * space, because that mapping is SHARED, not PRIVATE.
	 *
	 * If the file is write-only, there can be nothing of
	 * interest to bother with.
	 *
	 * The following reads the entire file, which might be
	 * more than necessary.  Better safe than sorry.
	 */

	if ((elf->ed_myflags & EDF_READ) &&
	    (_elf_vm(elf, (size_t)0, elf->ed_fsz) != OK_YES))
		return (0);

	flag = elf->ed_myflags & EDF_WRALLOC;
	if ((image = _elf_outmap(elf->ed_fd, outsz, &flag)) == 0)
		return (0);

	if (flag == 0)
		elf->ed_myflags |= EDF_IMALLOC;

	/*
	 * If an error occurs below, a "dirty" bit may be cleared
	 * improperly.  To save a second pass through the file,
	 * this code sets the dirty bit on the elf descriptor
	 * when an error happens, assuming that will "cover" any
	 * accidents.
	 */

	/*
	 * Hi is needed only when 'fill' is non-zero.
	 * Fill is non-zero only when the library
	 * calculates file/section/data buffer offsets.
	 * The lib guarantees they increase monotonically.
	 * That guarantees proper filling below.
	 */


	/*
	 * Ehdr first
	 */

	src.d_buf = (Elf_Void *)eh;
	src.d_type = ELF_T_EHDR;
	src.d_size = sizeof (Ehdr);
	src.d_version = EV_CURRENT;
	dst.d_buf = (Elf_Void *)image;
	dst.d_size = eh->e_ehsize;
	dst.d_version = ver;
	if (elf_xlatetof(&dst, &src, encode) == 0)
		return (0);
	elf->ed_ehflags &= ~ELF_F_DIRTY;
	hi = eh->e_ehsize;

	/*
	 * Phdr table if one exists
	 */

	if (eh->e_phnum != 0) {
		unsigned	work;
		/*
		 * Unlike other library data, phdr table is
		 * in the user version.  Change src buffer
		 * version here, fix it after translation.
		 */

		src.d_buf = (Elf_Void *)elf->ed_phdr;
		src.d_type = ELF_T_PHDR;
		src.d_size = elf->ed_phdrsz;
		ELFACCESSDATA(work, _elf_work)
		src.d_version = work;
		dst.d_buf = (Elf_Void *)(image + eh->e_phoff);
		dst.d_size = eh->e_phnum * eh->e_phentsize;
		hi = (Xword)(eh->e_phoff + dst.d_size);
		if (elf_xlatetof(&dst, &src, encode) == 0) {
			elf->ed_uflags |= ELF_F_DIRTY;
			return (0);
		}
		elf->ed_phflags &= ~ELF_F_DIRTY;
		src.d_version = EV_CURRENT;
	}

	/*
	 * Loop through sections
	 */

	ELFACCESSDATA(byte, _elf_byte);
	for (s = elf->ed_hdscn; s != 0; s = s->s_next) {
		register Dnode	*d, *prevd;
		Xword		off = 0;
		Shdr		*sh = s->s_shdr;
		char		*start = image + sh->sh_offset;
		char		*here;

		/*
		 * Just "clean" DIRTY flag for "empty" sections.  Even if
		 * NOBITS needs padding, the next thing in the
		 * file will provide it.  (And if this NOBITS is
		 * the last thing in the file, no padding needed.)
		 */
		if ((sh->sh_type == SHT_NOBITS) ||
		    (sh->sh_type == SHT_NULL)) {
			d = s->s_hdnode, prevd = 0;
			for (; d != 0; prevd = d, d = d->db_next)
				d->db_uflags &= ~ELF_F_DIRTY;
			continue;
		}
		/*
		 * Clear out the memory between the end of the last
		 * section and the begining of this section.
		 */
		if (fill && (sh->sh_offset > hi)) {
			sz = sh->sh_offset - hi;
			(void) memset(start - sz, byte, sz);
		}


		for (d = s->s_hdnode, prevd = 0;
		    d != 0; prevd = d, d = d->db_next) {
			d->db_uflags &= ~ELF_F_DIRTY;
			here = start + d->db_data.d_off;

			/*
			 * Clear out the memory between the end of the
			 * last update and the start of this data buffer.
			 */
			if (fill && (d->db_data.d_off > off)) {
				sz = (Xword)(d->db_data.d_off - off);
				(void) memset(here - sz, byte, sz);
			}

			if ((d->db_myflags & DBF_READY) == 0) {
				SCNLOCK(s);
				if (_elf_locked_getdata(s, &prevd->db_data) !=
				    &d->db_data) {
					elf->ed_uflags |= ELF_F_DIRTY;
					SCNUNLOCK(s);
					return (0);
				}
				SCNUNLOCK(s);
			}
			dst.d_buf = (Elf_Void *)here;
			dst.d_size = d->db_osz;

			/*
			 * Copy the translated bits out to the destination
			 * image.
			 */
			if (elf_xlatetof(&dst, &d->db_data, encode) == 0) {
				elf->ed_uflags |= ELF_F_DIRTY;
				return (0);
			}

			off = (Xword)(d->db_data.d_off + dst.d_size);
		}
		hi = sh->sh_offset + sh->sh_size;
	}

	/*
	 * Shdr table last
	 */

	if (fill && (eh->e_shoff > hi)) {
		sz = eh->e_shoff - hi;
		(void) memset(image + hi, byte, sz);
	}

	src.d_type = ELF_T_SHDR;
	src.d_size = sizeof (Shdr);
	dst.d_buf = (Elf_Void *)(image + eh->e_shoff);
	dst.d_size = eh->e_shentsize;
	for (s = elf->ed_hdscn; s != 0; s = s->s_next) {
		s->s_shflags &= ~ELF_F_DIRTY;
		s->s_uflags &= ~ELF_F_DIRTY;
		src.d_buf = s->s_shdr;

		if (elf_xlatetof(&dst, &src, encode) == 0) {
			elf->ed_uflags |= ELF_F_DIRTY;
			return (0);
		}

		dst.d_buf = (char *)dst.d_buf + eh->e_shentsize;
	}
	/*
	 * ELF_C_WRIMAGE signifyes that we build the memory image, but
	 * that we do not actually write it to disk.  This is used
	 * by ld(1) to build up a full image of an elf file and then
	 * to process the file before it's actually written out to
	 * disk.  This saves ld(1) the overhead of having to write
	 * the image out to disk twice.
	 */
	if (update_cmd == ELF_C_WRIMAGE) {
		elf->ed_uflags &= ~ELF_F_DIRTY;
		elf->ed_wrimage = image;
		elf->ed_wrimagesz = outsz;
		return (outsz);
	}

	if (_elf_outsync(elf->ed_fd, image, outsz,
	    ((elf->ed_myflags & EDF_IMALLOC) ? 0 : 1)) != 0) {
		elf->ed_uflags &= ~ELF_F_DIRTY;
		elf->ed_myflags &= ~EDF_IMALLOC;
		return (outsz);
	}

	elf->ed_uflags |= ELF_F_DIRTY;
	return (0);
}




/*
 * The following is a private interface between the linkers (ld & ld.so.1)
 * and libelf:
 *
 * elf_update(elf, ELF_C_WRIMAGE)
 *	This will cause full image representing the elf file
 *	described by the elf pointer to be built in memory.  If the
 *	elf pointer has a valid file descriptor associated with it
 *	we will attempt to build the memory image from mmap()'ed
 *	storage.  If the elf descriptor does not have a valid
 *	file descriptor (opened with elf_begin(0, ELF_C_IMAGE, 0))
 *	then the image will be allocated from dynamic memory (malloc()).
 *
 *	elf_update() will return the size of the memory image built
 *	when sucessful.
 *
 *	When a subsequent call to elf_update() with ELF_C_WRITE as
 *	the command is performed it will sync the image created
 *	by ELF_C_WRIMAGE to disk (if fd available) and
 *	free the memory allocated.
 */

off_t
_elfxx_update(Elf * elf, Elf_Cmd cmd)
{
	size_t		sz;
	unsigned	u;
	Ehdr		*eh = elf->ed_ehdr;

	if (elf == 0)
		return (-1);

	ELFWLOCK(elf)
	switch (cmd) {
	default:
		_elf_seterr(EREQ_UPDATE, 0);
		ELFUNLOCK(elf)
		return (-1);

	case ELF_C_WRIMAGE:
		if ((elf->ed_myflags & EDF_WRITE) == 0) {
			_elf_seterr(EREQ_UPDWRT, 0);
			ELFUNLOCK(elf)
			return (-1);
		}
		break;
	case ELF_C_WRITE:
		if ((elf->ed_myflags & EDF_WRITE) == 0) {
			_elf_seterr(EREQ_UPDWRT, 0);
			ELFUNLOCK(elf)
			return (-1);
		}
		if (elf->ed_wrimage) {
			if (elf->ed_myflags & EDF_WRALLOC) {
				free(elf->ed_wrimage);
				/*
				 * The size is still returned even
				 * though nothing is actually written
				 * out.  This is just to be consistant
				 * with the rest of the interface.
				 */
				sz = elf->ed_wrimagesz;
				elf->ed_wrimage = 0;
				elf->ed_wrimagesz = 0;
				ELFUNLOCK(elf);
				return ((off_t)sz);
			}
			sz = _elf_outsync(elf->ed_fd, elf->ed_wrimage,
				elf->ed_wrimagesz,
				(elf->ed_myflags & EDF_IMALLOC ? 0 : 1));
			elf->ed_myflags &= ~EDF_IMALLOC;
			elf->ed_wrimage = 0;
			elf->ed_wrimagesz = 0;
			ELFUNLOCK(elf);
			return ((off_t)sz);
		}
		/* FALLTHROUGH */
	case ELF_C_NULL:
		break;
	}

	if (eh == 0) {
		_elf_seterr(ESEQ_EHDR, 0);
		ELFUNLOCK(elf)
		return (-1);
	}

	if ((u = eh->e_version) > EV_CURRENT) {
		_elf_seterr(EREQ_VER, 0);
		ELFUNLOCK(elf)
		return (-1);
	}

	if (u == EV_NONE)
		eh->e_version = EV_CURRENT;

	if ((u = eh->e_ident[EI_DATA]) == ELFDATANONE) {
		unsigned	encode;

		ELFACCESSDATA(encode, _elf_encode)
		if (encode == ELFDATANONE) {
			_elf_seterr(EREQ_ENCODE, 0);
			ELFUNLOCK(elf)
			return (-1);
		}
		/* LINTED */
		eh->e_ident[EI_DATA] = (Byte)encode;
	}

	u = 1;
	if (elf->ed_uflags & ELF_F_LAYOUT) {
		sz = _elf_upd_usr(elf);
		u = 0;
	} else
		sz = _elf_upd_lib(elf);

	if ((sz != 0) && ((cmd == ELF_C_WRITE) || (cmd == ELF_C_WRIMAGE)))
		sz = wrt(elf, (Xword)sz, u, cmd);

	if (sz == 0) {
		ELFUNLOCK(elf)
		return (-1);
	}

	ELFUNLOCK(elf)
	return ((off_t)sz);
}


#ifndef _ELF64
/* class-independent, only needs to be compiled once */

off_t
elf_update(Elf *elf, Elf_Cmd cmd)
{
	if (elf == 0)
		return (-1);

	if (elf->ed_class == ELFCLASS32)
		return (_elf32_update(elf, cmd));
	else if (elf->ed_class == ELFCLASS64) {
		return (_elf64_update(elf, cmd));
	}

	_elf_seterr(EREQ_CLASS, 0);
	return (-1);
}

/*
 * 4106312, 4106398, This is an ad-hoc means for the 32-bit
 * Elf64 version of libld.so.3 to get around the limitation
 * of a 32-bit d_off field.  This is only intended to be
 * used by libld to relocate symbols in large NOBITS sections.
 */
Elf64_Off
_elf_getxoff(Elf_Data * d)
{
	return (((Dnode *)d)->db_xoff);
}
#endif /* !_ELF64 */
