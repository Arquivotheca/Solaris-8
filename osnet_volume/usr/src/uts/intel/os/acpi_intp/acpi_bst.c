/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_bst.c	1.1	99/05/21 SMI"


/* byte stream */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_stk.h"
#include "acpi_bst.h"


/* returns bst * or NULL on error */
bst *
bst_open(char *base, int length)
{
	bst *new;

	if ((new = kmem_alloc(sizeof (bst), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	new->ptr = (unsigned char *)base;
	new->base = (unsigned char *)base;
	new->index = 0;
	new->length = length;
	new->sp = NULL;
	return (new);
}

void
bst_close(bst *bp)
{
	if (bp == NULL)
		return;
	if (bp->sp)
		stack_free(bp->sp);
	kmem_free(bp, sizeof (bst));
}

/* returns char value or EXC on EOF */
int
bst_get(bst *bp)
{
	int ret;

	if (bp->index >= bp->length) {
		bp->index = bp->length;
		return (ACPI_EXC);
	}
	ret = *bp->ptr;
	bp->ptr++;
	bp->index++;
	return (ret);
}

int
bst_buffer(bst *bp, char *buffer, int length)
{
	if (length < 0 || bp->index + length > bp->length)
		return (ACPI_EXC);
	bcopy(bp->ptr, buffer, length);
	bp->ptr += length;
	bp->index += length;
	return (ACPI_OK);
}

int
bst_peek(bst *bp)
{
	if (bp->index >= bp->length) {
		bp->index = bp->length;
		return (ACPI_EXC);
	}
	return (*bp->ptr);
}

/* returns EXC on unget past beginning */
int
bst_unget(bst *bp)
{
	if (bp->index <= 0) {
		bp->index = 0;
		return (ACPI_EXC);
	}
	bp->ptr--;
	bp->index--;
	return (ACPI_OK);
}

/* returns new position or EXC on error */
int
bst_seek(bst *bp, int position, int relative)
{
	if (relative)
		position = position + bp->index;
	if (position < 0 || position > bp->length)
		return (ACPI_EXC);
	bp->ptr = bp->base + position;
	bp->index = position;
	return (position);
}

int
bst_strlen(bst *bp)
{
	int i, max, start, value;
	unsigned char *ptr;

	start = bst_index(bp);
	max = bp->length;
	ptr = bp->ptr;
	for (i = start; i <= max; i++) {
		value = *ptr++;
		if (value == NULL)
			break;
		if (value > 0x7f) /* bad ASCII */
			return (exc_code(ACPI_ERANGE));
	}
	if (i > max)
		return (ACPI_EXC);
	return (i - start);
}

/*LINTLIBRARY*/
int
exc_bst(int code, bst *bp)
{
	return (exc_offset(code, bst_index(bp)));
}

#define	STACK_PTR ((int *)stack_top(bp->sp))

int
bst_stack(bst *bp, int st_len)
{
	if ((bp->sp = stack_new(sizeof (int), st_len)) == NULL)
		return (exc_code(ACPI_ERES));
	*STACK_PTR = bp->length;
	return (ACPI_OK);
}

int
bst_push(bst *bp, int pos)
{
	if (bp->sp == NULL || pos < 0 || pos > bp->length ||
	    stack_push(bp->sp) == NULL)
		return (ACPI_EXC);
	*STACK_PTR = pos;
	bp->length = pos;
	return (ACPI_OK);
}

int
bst_pop(bst *bp)
{
	if (bp->sp == NULL || stack_pop(bp->sp))
		return (ACPI_EXC);
	bp->length = *STACK_PTR;
	return (ACPI_OK);
}


/* eof */
