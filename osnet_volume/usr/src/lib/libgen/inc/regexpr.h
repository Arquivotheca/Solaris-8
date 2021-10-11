/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _REGEXPR_H
#define	_REGEXPR_H

#pragma ident	"@(#)regexpr.h	1.9	93/11/10 SMI"	/* SVr4.0 1.1.3.1 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	NBRA 9
#ifdef	_REENTRANT
extern char **___braslist();
#define	braslist (___braslist())
extern char **___braelist();
#define	braelist (___braelist())
extern int *___nbra();
#define	nbra (*(___nbra()))
extern int *___regerrno();
#define	regerrno (*(___regerrno()))
extern int *___reglength();
#define	reglength (*(___reglength()))
extern char **___loc1();
#define	loc1 (*(___loc1()))
extern char **___loc2();
#define	loc2 (*(___loc2()))
extern char **___locs();
#define	locs (*(___locs()))
#else
extern char	*braslist[NBRA];
extern char	*braelist[NBRA];
extern int nbra, regerrno, reglength;
extern char *loc1, *loc2, *locs;
#endif
#ifdef	__STDC__
extern int step(const char *string, const char *expbuf);
extern int advance(const char *string, const char *expbuf);
extern char *compile(const char *instring, char *expbuf, char *endbuf);
#else
extern int step();
extern int advance();
extern char *compile();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _REGEXPR_H */
