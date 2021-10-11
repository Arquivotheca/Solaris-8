/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * Global include file for all sgs.
 */

#ifndef	_SGS_H
#define	_SGS_H

#pragma ident	"@(#)sgs.h	1.32	99/10/12 SMI"

/*
 * Software identification.
 */
#define	SGS		""
#define	SGU_PKG		"Software Generation Utilities"
#define	SGU_REL		"(SGU) Solaris-ELF (4.0)"

#ifndef	_ASM

#include <sys/types.h>
#include <stdlib.h>
#include <libelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Macro to round to next double word boundary.
 */
#define	S_DROUND(x)	(((x) + sizeof (double) - 1) & ~(sizeof (double) - 1))

/*
 * General align and round macros.
 */
#define	S_ALIGN(x, a)	((x) & ~(((a) ? (a) : 1) - 1))
#define	S_ROUND(x, a)   ((x) + (((a) ? (a) : 1) - 1) & ~(((a) ? (a) : 1) - 1))

/*
 * Bit manipulation macros; generic bit mask and is `v' in the range
 * supportable in `n' bits?
 */
#define	S_MASK(n)	((1 << (n)) -1)
#define	S_INRANGE(v, n)	(((-(1 << (n)) - 1) < (v)) && ((v) < (1 << (n))))

/*
 * General typedefs.
 */
typedef enum {
	FALSE = 0,
	TRUE = 1
} Boolean;

/*
 * Types of errors (used by eprintf()), together with a generic error return
 * value.
 */
typedef enum {
	ERR_NONE,
	ERR_WARNING,
	ERR_FATAL,
	ERR_ELF,
	ERR_NUM				/* Must be last */
} Error;

#define	S_ERROR		(~(uintptr_t)0)

/*
 * LIST_TRAVERSE() is used as the only "argument" of a "for" loop to
 * traverse a linked list. The node pointer `node' is set to each node in
 * turn and the corresponding data pointer is copied to `data'.  The macro
 * is used as in
 * 	for (LIST_TRAVERSE(List *list, Listnode *node, void *data)) {
 *		process(data);
 *	}
 */
#define	LIST_TRAVERSE(L, N, D) \
	(void) (((N) = (L)->head) != NULL && ((D) = (N)->data) != NULL); \
	(N) != NULL; \
	(void) (((N) = (N)->next) != NULL && ((D) = (N)->data) != NULL)

typedef	struct listnode	Listnode;
typedef	struct list	List;

struct	listnode {			/* a node on a linked list */
	void		*data;		/* the data item */
	Listnode	*next;		/* the next element */
};

struct	list {				/* a linked list */
	Listnode	*head;		/* the first element */
	Listnode	*tail;		/* the last element */
};

#ifdef _SYSCALL32
typedef	struct listnode32	Listnode32;
typedef	struct list32		List32;

struct	listnode32 {			/* a node on a linked list */
	Elf32_Addr	data;		/* the data item */
	Elf32_Addr	next;		/* the next element */
};

struct	list32 {			/* a linked list */
	Elf32_Addr	head;		/* the first element */
	Elf32_Addr	tail;		/* the last element */
};
#endif	/* _SYSCALL32 */


/*
 * Data structures (defined in libld.h).
 */
typedef struct ent_desc		Ent_desc;
typedef struct gottable		Gottable;
typedef struct ifl_desc		Ifl_desc;
typedef struct is_desc		Is_desc;
typedef struct isa_desc		Isa_desc;
typedef struct isa_opt		Isa_opt;
typedef struct ofl_desc		Ofl_desc;
typedef struct os_desc		Os_desc;
typedef	struct rel_cache	Rel_cache;
typedef	struct sdf_desc		Sdf_desc;
typedef	struct sdv_desc		Sdv_desc;
typedef struct sg_desc		Sg_desc;
typedef struct sort_desc	Sort_desc;
typedef struct sec_order	Sec_order;
typedef struct sym_desc		Sym_desc;
typedef struct sym_aux		Sym_aux;
typedef struct sym_cache	Sym_cache;
typedef struct sym_names	Sym_names;
typedef	struct uts_desc		Uts_desc;
typedef struct ver_desc		Ver_desc;
typedef struct ver_index	Ver_index;
typedef	struct audit_desc	Audit_desc;
typedef	struct audit_info	Audit_info;

/*
 * Data structures defined in machrel.h.
 */
typedef struct rel_desc		Rel_desc;

/*
 * For the various utilities that include sgs.h
 */
extern char	*sgs_demangle(char *);

#ifdef	__cplusplus
}
#endif

#endif /* _ASM */

#endif /* _SGS_H */
