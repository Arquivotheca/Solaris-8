/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)freesystem.c	1.3	90/03/01 SMI"	/* SVr4.0 1.3	*/
/* LINTLIBRARY */

# include	"lp.h"
# include	"systems.h"

/**
 **  freesystem() - FREE MEMORY ALLOCATED FOR SYSTEM STRUCTURE
 **/


#if	defined(__STDC__)
void freesystem ( SYSTEM * sp )
#else
void freesystem ( sp )
SYSTEM	*sp;
#endif
{
    if (!sp)
	return;

    if (sp->name)
	Free(sp->name);

    if (sp->passwd)
	Free(sp->passwd);

    if (sp->comment)
	Free(sp->comment);
}
