/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ACPI_BST_H
#define	_ACPI_BST_H

#pragma ident	"@(#)acpi_bst.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* ACPI byte stream interface */

typedef struct byst {
	unsigned char *base;
	unsigned char *ptr;	/* position for next read */
	struct acpi_stk *sp;
	int index;		/* 0 to length - 1 */
	int length;		/* current segment length */
} bst;


/* returns bst * or NULL on error */
extern bst *bst_open(char *base, int length);
extern void bst_close(bst *bp);

/* returns char value or EXC on EOF */
extern int bst_get(bst *bp);
extern int bst_buffer(bst *bp, char *buffer, int length);
extern int bst_peek(bst *bp);
/* returns EXC on unget past beginning */
extern int bst_unget(bst *bp);

#define	bst_length(BP) ((BP)->length)
#define	bst_index(BP)  ((BP)->index)
/* returns new position or EXC on error */
extern int bst_seek(bst *bp, int position, int relative);
extern int bst_strlen(bst *bp);
extern int exc_bst(int code, bst *bp);

extern int bst_stack(bst *bp, int st_len);
extern int bst_push(bst *bp, int pos);
extern int bst_pop(bst *bp);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_BST_H */
