/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_TSS_H
#define	_SYS_TSS_H

#pragma ident	"@(#)tss.h	1.3	99/05/04 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Maximum I/O address that will be in TSS bitmap
 */
#define	MAXTSSIOADDR	0x3ff	/* XXX - needs to support 64K I/O space */

#ifndef _ASM

/*
 * 386 TSS definition
 */

struct tss386 {
	unsigned long t_link;
	unsigned long t_esp0;
	unsigned long t_ss0;
	unsigned long t_esp1;
	unsigned long t_ss1;
	unsigned long t_esp2;
	unsigned long t_ss2;
	paddr_t t_cr3;
	unsigned long t_eip;
	unsigned long t_eflags;
	unsigned long t_eax;
	unsigned long t_ecx;
	unsigned long t_edx;
	unsigned long t_ebx;
	unsigned long t_esp;
	unsigned long t_ebp;
	unsigned long t_esi;
	unsigned long t_edi;
	unsigned long t_es;
	unsigned long t_cs;
	unsigned long t_ss;
	unsigned long t_ds;
	unsigned long t_fs;
	unsigned long t_gs;
	unsigned long t_ldt;
	unsigned long t_bitmapbase;
};

#endif	/* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TSS_H */
