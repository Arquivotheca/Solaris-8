/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_AUXV_H
#define	_SYS_AUXV_H

#pragma ident	"@(#)auxv.h	1.26	98/01/13 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

#if !defined(_ASM)
typedef struct
{
	int	a_type;
	union {
		long	a_val;
#ifdef __STDC__
		void	*a_ptr;
#else
		char	*a_ptr;
#endif
		void	(*a_fcn)();
	} a_un;
} auxv_t;

#if defined(_SYSCALL32)

typedef struct {
	int32_t	a_type;
	union	{
		int32_t	a_val;
		caddr32_t a_ptr;
		caddr32_t a_fcn;
	} a_un;
} auxv32_t;

#endif	/* _SYSCALL32 */

#endif /* _ASM */

#define	AT_NULL		0
#define	AT_IGNORE	1
#define	AT_EXECFD	2
#define	AT_PHDR		3	/* &phdr[0] */
#define	AT_PHENT	4	/* sizeof(phdr[0]) */
#define	AT_PHNUM	5	/* # phdr entries */
#define	AT_PAGESZ	6	/* getpagesize(2) */
#define	AT_BASE		7	/* ld.so base addr */
#define	AT_FLAGS	8	/* processor flags */
#define	AT_ENTRY	9	/* a.out entry point */
#define	AT_DCACHEBSIZE	10	/* smallest data cache block size */
#define	AT_ICACHEBSIZE	11	/* smallest instruction cache block size */
#define	AT_UCACHEBSIZE	12	/* smallest unified cache block size */

#define	AT_SUN_UID	2000	/* effective user id */
#define	AT_SUN_RUID	2001	/* real user id */
#define	AT_SUN_GID	2002	/* effective group id */
#define	AT_SUN_RGID	2003	/* real group id */
/*
 * The following attributes are specific to the
 * kernel implementation of the linker/loader.
 */
#define	AT_SUN_LDELF	2004	/* dynamic linker's ELF header */
#define	AT_SUN_LDSHDR	2005	/* dynamic linker's section headers */
#define	AT_SUN_LDNAME	2006	/* name of dynamic linker */
#define	AT_SUN_LPAGESZ	2007	/* large pagesize */
/*
 * The following aux vector provides a null-terminated platform
 * identification string. This information is the same as provided
 * by sysinfo(2) when invoked with the command SI_PLATFORM.
 */
#define	AT_SUN_PLATFORM	2008	/* platform name */
/*
 * These attributes communicate performance -hints- about processor
 * hardware capabilities that might be useful to library implementations.
 */
#define	AT_SUN_HWCAP	2009

#if defined(_KERNEL)
extern int auxv_hwcap;		/* user info regarding machine attributes */
extern int kauxv_hwcap;		/* kernel info regarding machine attributes */
#endif	/* _KERNEL */

#define	AT_SUN_IFLUSH	2010	/* flush icache? */
#define	AT_SUN_CPU	2011	/* cpu name */

/*
 * Two additional Sun aux vectors added to support _iBCS2
 * compatibility.  These vectors are used when executing coff files
 * to point at the coff entry point, and coff file descriptor for
 * running coff binaries on i386.
 */

#define	AT_SUN_EMUL_ENTRY	2012	/* coff entry point */
#define	AT_SUN_EMUL_EXECFD	2013	/* coff file descriptor */

/*
 * The following aux vector provides a pointer to a null-terminated
 * path name, a copy of the path name passed to the exec() system
 * call but that has had all symlinks resolved (see resolvepath(2)).
 */
#define	AT_SUN_EXECNAME	2014	/* exec() path name */

#define	AT_SUN_MMU	2015	/* mmu module name */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_AUXV_H */
