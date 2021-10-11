/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)userdisp.h	1.4	99/04/07 SMI"       /* SVr4.0 1.1 */

/* Flag values for dispusrdefs() */
#define	D_GROUP	0x1
#define	D_BASEDIR	0x2
#define	D_RID	0x4
#define	D_SKEL	0x8
#define	D_SHELL	0x10
#define	D_INACT	0x20
#define	D_EXPIRE	0x40
#define	D_AUTH	0x50
#define	D_PROF	0x60
#define	D_ROLE	0x70

#define	D_ALL	( D_GROUP | D_BASEDIR | D_RID | D_SKEL | D_SHELL \
		| D_INACT | D_EXPIRE | D_AUTH | D_PROF | D_ROLE )
