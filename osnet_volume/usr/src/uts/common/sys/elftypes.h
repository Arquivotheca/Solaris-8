/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ELFTYPES_H
#define	_SYS_ELFTYPES_H

#pragma ident	"@(#)elftypes.h	1.20	98/04/08 SMI"	/* SVr4.0 1.1	*/

#ifdef	__cplusplus
extern "C" {
#endif

#if defined(_LP64) || defined(_I32LPx)
typedef unsigned int		Elf32_Addr;
typedef unsigned short		Elf32_Half;
typedef unsigned int		Elf32_Off;
typedef int			Elf32_Sword;
typedef unsigned int		Elf32_Word;
#else
typedef unsigned long		Elf32_Addr;
typedef unsigned short		Elf32_Half;
typedef unsigned long		Elf32_Off;
typedef long			Elf32_Sword;
typedef unsigned long		Elf32_Word;
#endif

#if defined(_LP64)
typedef unsigned long		Elf64_Addr;
typedef unsigned short		Elf64_Half;
typedef unsigned long		Elf64_Off;
typedef int			Elf64_Sword;
typedef long			Elf64_Sxword;
typedef	unsigned int		Elf64_Word;
typedef	unsigned long		Elf64_Xword;
typedef unsigned long		Elf64_Lword;
typedef unsigned long		Elf32_Lword;
#elif ((__STDC__ - 0 == 0) && (!defined(_NO_LONGLONG)))
typedef unsigned long long	Elf64_Addr;
typedef unsigned short		Elf64_Half;
typedef unsigned long long	Elf64_Off;
typedef int			Elf64_Sword;
typedef long long		Elf64_Sxword;
typedef	unsigned int		Elf64_Word;
typedef	unsigned long long	Elf64_Xword;
typedef	unsigned long long	Elf64_Lword;
typedef unsigned long long	Elf32_Lword;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELFTYPES_H */
