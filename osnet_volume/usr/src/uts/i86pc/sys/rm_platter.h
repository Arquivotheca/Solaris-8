/*
 * Copyright (c) 1992,1997-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any		*/
/*	actual or intended publication of such source code.	*/

#ifndef	_SYS_RMPLATTER_H
#define	_SYS_RMPLATTER_H

#pragma ident	"@(#)rm_platter.h	1.10	98/01/09 SMI"

#include <sys/types.h>
#include <sys/segment.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef	struct rm_platter {
	char	rm_code[1024];
	ushort_t rm_debug;
	ushort_t rm_gdt_lim;		/* stuff for lgdt */
	struct	seg_desc *rm_gdt_base;
	ushort_t rm_filler2;		/* till I am sure that pragma works */
	ushort_t rm_idt_lim;		/* stuff for lidt */
	struct  gate_desc *rm_idt_base;
	uint_t	rm_pdbr;		/* cr3 value */
	uint_t	rm_cpu;			/* easy way to know which CPU we are */
	uint_t	rm_x86feature;		/* X86 supported features */
	uint_t	rm_cr4;			/* cr4 value on cpu0 */
} rm_platter_t;

/*
 * cpu tables put within a single structure all the tables which need to be
 * allocated when a CPU starts up. Makes it more memory effficient and easier
 * to allocate/release
 */
typedef struct cpu_tables {
	/* 1st level page directory */
	char	ct_stack[4096];		/* default stack for tss and startup */
	struct	seg_desc ct_gdt[GDTSZ];
	struct	tss386 ct_tss;
} cpu_tables_t;

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RMPLATTER_H */
