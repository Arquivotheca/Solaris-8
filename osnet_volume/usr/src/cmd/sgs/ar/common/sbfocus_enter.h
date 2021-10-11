#pragma ident	"@(#)sbfocus_enter.h	1.2	93/06/04 SMI"

/*
 *	Copyright (c) 1989,1990 Sun Microsystems, Inc.  All Rights Reserved.
 *	Sun considers its source code as an unpublished, proprietary
 *	trade secret, and it is available only under strict license
 *	provisions.  This copyright notice is placed here only to protect
 *	Sun in the event the source is deemed a published work.  Dissassembly,
 *	decompilation, or other means of reducing the object code to human
 *	readable form is prohibited by the license agreement under which
 *	this code is provided to the user or company in possession of this
 *	copy.
 *	RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 *	Government is subject to restrictions as set forth in subparagraph
 *	(c)(1)(ii) of the Rights in Technical Data and Computer Software
 *	clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 *	NASA FAR Supplement.
 */

#ifndef sbfocus_enter_h_INCLUDED
#define sbfocus_enter_h_INCLUDED

#include <stdio.h>
#include <sys/param.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

/*
 * Datastructure that holds persistent data that sbfocus_symbol & sbfocus_close
 * needs. Passing in a pointer to this struct makes them re-entrant.
 */
typedef struct Sbld_tag *Sbld, Sbld_rec;

struct Sbld_tag {
	FILE	*fd;
	int	failed;
};

/*
 * fragment of SunOS <machine/a.out.h>
 *         Format of a symbol table entry
 */
struct nlist {
	union {
		char *n_name;              /* for use when in-core */
		long n_strx;               /* index into file string table */
	} n_un;
	unsigned char  n_type;             /* type flag (N_TEXT...) */
	char     n_other;                  /* unused */
	short    n_desc;                   /* see <stab.h> */
	unsigned long  n_value;            /* value of symbol (or sdb offset) */
};

extern  void    sbfocus_symbol();
extern  void    sbfocus_close();
Sbld_rec   sb_data; 
#endif
