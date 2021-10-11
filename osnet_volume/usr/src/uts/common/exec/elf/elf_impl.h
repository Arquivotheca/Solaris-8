/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ELF_ELF_IMPL_H
#define	_ELF_ELF_IMPL_H

#pragma ident	"@(#)elf_impl.h	1.5	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#if	!defined(_LP64) || defined(_ELF32_COMPAT)

/*
 * Definitions for ELF32, native 32-bit or 32-bit compatibility mode.
 */
#define	ELFCLASS	ELFCLASS32
typedef	unsigned int	aux_val_t;
typedef	auxv32_t	aux_entry_t;

#else	/* !_LP64 || _ELF32_COMPAT */

/*
 * Definitions for native 64-bit ELF
 */
#define	ELFCLASS	ELFCLASS64
typedef	unsigned long	aux_val_t;
typedef	auxv_t		aux_entry_t;

#endif	/* !_LP64 || _ELF32_COMPAT */

/*
 * Start of an ELF Note.
 */
typedef struct {
	Nhdr	nhdr;
	char	name[8];
} Note;

#ifdef	_ELF32_COMPAT
/*
 * These are defined only for the 32-bit compatibility
 * compilation mode of the 64-bit kernel.
 */
#define	elfexec	elf32exec
#define	elfnote	elf32note
#define	elfcore	elf32core
#define	setup_note_header	setup_note_header32
#define	write_elfnotes		write_elfnotes32
#define	setup_old_note_header	setup_old_note_header32
#define	write_old_elfnotes	write_old_elfnotes32

#if defined(sparc) || defined(__sparc)
#define	gwindows_t	gwindows32_t
#define	rwindow		rwindow32
#endif

#define	psinfo_t	psinfo32_t
#define	pstatus_t	pstatus32_t
#define	lwpsinfo_t	lwpsinfo32_t
#define	lwpstatus_t	lwpstatus32_t

#define	prgetpsinfo	prgetpsinfo32
#define	prgetstatus	prgetstatus32
#define	prgetlwpsinfo	prgetlwpsinfo32
#define	prgetlwpstatus	prgetlwpstatus32
#define	prgetwindows	prgetwindows32

#define	prpsinfo_t	prpsinfo32_t
#define	prstatus_t	prstatus32_t
#if defined(prfpregset_t)
#undef prfpregset_t
#endif
#define	prfpregset_t	prfpregset32_t

#define	oprgetstatus	oprgetstatus32
#define	oprgetpsinfo	oprgetpsinfo32
#define	prgetprfpregs	prgetprfpregs32

#endif	/*	_ELF32_COMPAT	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _ELF_ELF_IMPL_H */
