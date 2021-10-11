/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_STK_H
#define	_ACPI_STK_H

#pragma ident	"@(#)acpi_stk.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for stacks */

typedef struct acpi_stk {
	char *base;
	char *ptr;
	int index;
	int size;		/* of element */
	int max;		/* max index */
} acpi_stk_t;


/*
 * Top of stack is always in range.  After creating a new stack, you
 * should assign the top of stack before the first push, or you will
 * waste a slot.
 */

/* for static initialization */
#define	STACK_INIT(TYPE, ARRAY) \
{(char *)&ARRAY[0], (char *)&ARRAY[0], 0, sizeof (TYPE), \
sizeof (ARRAY)/sizeof (TYPE) - 1}

/* size of element, length of stack in elements */
extern acpi_stk_t *stack_new(int size, int len);
extern void stack_free(acpi_stk_t *sp);

#define	stack_top(SP) ((SP)->ptr)

extern void *stack_push(acpi_stk_t *sp);
extern int stack_pop(acpi_stk_t *sp);
extern int stack_popn(acpi_stk_t *sp, int n);
#define	stack_index(SP) ((SP)->index)
extern int stack_seek(acpi_stk_t *sp, int n);


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_STK_H */
