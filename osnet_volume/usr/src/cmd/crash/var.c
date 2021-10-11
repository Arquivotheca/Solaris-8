/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)var.c	1.3	97/09/07 SMI"

/*
 * This file contains code for the crash function var.
 */

#include <stdio.h>
#include <sys/var.h>
#include <sys/elf.h>
#include "crash.h"

/* print var structure */
static void
prvar()
{
	extern Sym *V;

	kvm_read(kd, V->st_value, (char *)&vbuf, sizeof (vbuf));

	fprintf(fp, "v_buf: %3d\nv_call: %3d\nv_proc: %3d\n",
		vbuf.v_buf,
		vbuf.v_call,
		vbuf.v_proc);
	fprintf(fp, "v_nglobpris: %3d\nv_maxsyspri: %3d\n",
		vbuf.v_nglobpris,
		vbuf.v_maxsyspri);
	fprintf(fp, "v_clist: %3d\nv_maxup: %3d\n",
		vbuf.v_clist,
		vbuf.v_maxup);
	fprintf(fp, "v_hbuf: %3d\nv_hmask: %3d\nv_pbuf: %3d\n",
		vbuf.v_hbuf,
		vbuf.v_hmask,
		vbuf.v_pbuf);
	fprintf(fp, "v_sptmap: %3d\n",
		vbuf.v_sptmap);
	fprintf(fp, "v_maxpmem: %d\nv_autoup: %d\nv_bufhwm: %d\n",
		vbuf.v_maxpmem,
		vbuf.v_autoup,
		vbuf.v_bufhwm);
}


/* get arguments for var function */
void
getvar()
{
	int c;

	optind = 1;
	while ((c = getopt(argcnt, args, "w:")) != EOF) {
		switch (c) {
			case 'w' :	redirect();
					break;
			default  :	longjmp(syn, 0);
		}
	}
	if (args[optind])
		longjmp(syn, 0);
	else prvar();
}
