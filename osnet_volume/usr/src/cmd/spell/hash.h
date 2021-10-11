/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)hash.h	1.7	95/03/16 SMI"	/* SVr4.0 1.2	*/
#define	HASHWIDTH 27
#define	HASHSIZE 134217689L	/* prime under 2^HASHWIDTH */
#define	INDEXWIDTH 9
#define	INDEXSIZE (1<<INDEXWIDTH)
#define	NI (INDEXSIZE+1)
#define	ND ((25750/2) * sizeof (*table))
#define	BYTE 8

extern unsigned *table;
extern int index[];	/* into dif table based on hi hash bits */

void	hashinit(void);
int	hashlook(char *);
unsigned long	hash(char *);
