/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _NLIST_H
#define	_NLIST_H

#pragma ident	"@(#)nlist.h	1.9	92/07/14 SMI"	/* SVr4.0 1.8.2.4 */

#ifdef	__cplusplus
extern "C" {
#endif

struct nlist {
	char		*n_name;	/* symbol name */
	long		n_value;	/* value of symbol */
	short		n_scnum;	/* section number */
	unsigned short	n_type;		/* type and derived type */
	char		n_sclass;	/* storage class */
	char		n_numaux;	/* number of aux. entries */
};

#if defined(__STDC__)
extern int nlist(const char *, struct nlist *);
#else	/* __STDC__ */
extern int nlist();
#endif  /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _NLIST_H */
