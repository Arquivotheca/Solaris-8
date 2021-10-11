/*
 * Copyright 1995 Sun Microsystems Inc.
 * All rights reserved.
 */
							    

#ifndef	__TABLE_H
#define	__TABLE_H

#pragma ident	"@(#)table.h	1.4	96/04/25 SMI"        /* SMI4.1 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#define NUMLETTERS 27 /* 26 letters  + 1 for anything else */
#define TABLESIZE (NUMLETTERS*NUMLETTERS)

typedef struct tablenode *tablelist;
struct tablenode {
	char *key;
	char *datum;
	tablelist next;
};
typedef struct tablenode tablenode;

typedef tablelist stringtable[TABLESIZE];

int tablekey();
char *lookup();
void store();

#ifdef	__cplusplus
}
#endif

#endif	/* __TABLE_H */
