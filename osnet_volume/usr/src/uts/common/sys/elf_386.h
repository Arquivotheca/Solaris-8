/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_ELF_386_H
#define	_SYS_ELF_386_H

#pragma ident	"@(#)elf_386.h	1.13	99/07/19 SMI"	/* SVr4.0 1.2	*/

#ifdef	__cplusplus
extern "C" {
#endif

#define	R_386_NONE		0	/* relocation type */
#define	R_386_32		1
#define	R_386_PC32		2
#define	R_386_GOT32		3
#define	R_386_PLT32		4
#define	R_386_COPY		5
#define	R_386_GLOB_DAT		6
#define	R_386_JMP_SLOT		7
#define	R_386_RELATIVE		8
#define	R_386_GOTOFF		9
#define	R_386_GOTPC		10
#define	R_386_32PLT		11
#define	R_386_NUM		12

#define	ELF_386_MAXPGSZ		0x10000	/* maximum page size */

#define	SHF_ORDERED	0x40000000
#define	SHF_EXCLUDE	0x80000000

#define	SHN_BEFORE	0xff00
#define	SHN_AFTER	0xff01

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ELF_386_H */
