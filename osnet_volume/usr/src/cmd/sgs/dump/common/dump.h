/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)dump.h	6.6	98/10/09 SMI"

#include	<sys/elf.h>
#include	<sys/machelf.h>
#include	<gelf.h>

#define	DATESIZE 60

typedef struct scntab {
	char *		scn_name;
	Elf_Scn *	p_sd;
	GElf_Shdr	p_shdr;
} SCNTAB;


extern void	print_reloc_type(int machine, int type);
