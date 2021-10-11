/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_LEX_H
#define	_ACPI_LEX_H

#pragma ident	"@(#)acpi_lex.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for lex table */

typedef struct lex_entry {
	short flags;
	unsigned short token;
	unsigned short look;
} lex_entry_t;

/* flags */
#define	CTX_LOOK  0x0001	/* lookahead context flag */
#define	CTX_PRI	  0x0002	/* primary context */
#define	CTX_EXT   0x0004    	/* extended operation context */
#define	CTX_LNOT  0x0008
#define	CTX_LNAME 0x0010	/* lead name segment character */
#define	CTX_DNAME 0x0020	/* digit name character */
#define	CTX_MNAME 0x0040	/* meta name character */
#define	CTX_DATA  0x0080	/* data */
#define	CTX_AAL   0x0100	/* args and locals */
#define	CTX_OSUP  0x0200	/* other supernames */
#define	CTX_TYPE1 0x0400	/* type 1 opcodes */
#define	CTX_TYPE2 0x0800	/* type 2 opcodes */
#define	CTX_OBJ   0x1000	/* objects */

#define	CTX_SNAME (CTX_LNAME|CTX_DNAME)	/* name segment characters */
#define	CTX_NAME  (CTX_LNAME|CTX_MNAME)	/* name lookahead characters */
#define	CTX_PDATA (CTX_NAME|CTX_DATA) /* package data = data + names */
#define	CTX_SUPER (CTX_NAME|CTX_AAL|CTX_OSUP) /* supernames (could use alt) */

/* names for term and termarg may actually be method calls */
#define	CTX_TERMARG (CTX_NAME|CTX_AAL|CTX_TYPE2|CTX_DATA) /* term arg */
#define	CTX_TERM (CTX_NAME|CTX_OBJ|CTX_TYPE1|CTX_TYPE2)	/* term */


#define	LEX_TABLE_SIZE (256)
extern lex_entry_t lex_table[LEX_TABLE_SIZE];

extern int lex(struct byst *bp, int lctx, int consume);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_LEX_H */
