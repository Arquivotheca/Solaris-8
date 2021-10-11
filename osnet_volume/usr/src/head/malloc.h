/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _MALLOC_H
#define	_MALLOC_H

#pragma ident	"@(#)malloc.h	1.11	97/08/23 SMI"	/* SVr4.0 1.7	*/

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Constants defining mallopt operations
 */
#define	M_MXFAST	1	/* set size of blocks to be fast */
#define	M_NLBLKS	2	/* set number of block in a holding block */
#define	M_GRAIN		3	/* set number of sizes mapped to one, for */
				/* small blocks */
#define	M_KEEP		4	/* retain contents of block after a free */
				/* until another allocation */
/*
 *	structure filled by
 */
struct mallinfo  {
	unsigned long arena;	/* total space in arena */
	unsigned long ordblks;	/* number of ordinary blocks */
	unsigned long smblks;	/* number of small blocks */
	unsigned long hblks;	/* number of holding blocks */
	unsigned long hblkhd;	/* space in holding block headers */
	unsigned long usmblks;	/* space in small blocks in use */
	unsigned long fsmblks;	/* space in free small blocks */
	unsigned long uordblks;	/* space in ordinary blocks in use */
	unsigned long fordblks;	/* space in free ordinary blocks */
	unsigned long keepcost;	/* cost of enabling keep option */
};

#if defined(__STDC__)

void *malloc(size_t);
void free(void *);
void *realloc(void *, size_t);
int mallopt(int, int);
struct mallinfo mallinfo(void);
void *calloc(size_t, size_t);

#else

void *malloc();
void free();
void *realloc();
int mallopt();
struct mallinfo mallinfo();
void *calloc();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _MALLOC_H */
