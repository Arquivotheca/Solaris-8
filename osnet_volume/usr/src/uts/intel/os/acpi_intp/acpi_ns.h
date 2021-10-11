/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_NS_H
#define	_ACPI_NS_H

#pragma ident	"@(#)acpi_ns.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/* interface for name space */

typedef struct ns_elem {
	struct node node;	/* associated node struct */
#define	nse_ddbh node.data	/* DDB handle or execution context */
#define	nse_offset node.elem	/* where defined in DDB */
	struct acpi_val *valp;
	struct ns_elem *dyn;	/* dynamic object list */
	acpi_nameseg_t name_seg;
	int flags;
} ns_elem_t;

#define	ns_first_child(NSP)	(ns_elem_t *)((NSP)->node.child)
#define	ns_next(NSP)		(ns_elem_t *)((NSP)->node.next)
#define	ns_parent(NSP)		(ns_elem_t *)((NSP)->node.parent)

/* flags field */
#define	NS_DYN_METHOD	0x0001  /* dynamic method scope */
#define	NS_ROOT		0x0004	/* root scope, consistent with lookup */


/* root of name space */
ns_elem_t *root_ns;


/*
 * functions
 */
extern ns_elem_t *ns_new(void);
extern void ns_free(ns_elem_t *nsp);
extern ns_elem_t *ns_root_new(void *key);

extern ns_elem_t *ns_lookup_here(ns_elem_t *nsp, acpi_nameseg_t *segp,
    void *key);
extern int ns_lookup(ns_elem_t *rootp, ns_elem_t *curp, struct name *namep,
    void *key, int flags, ns_elem_t **targetpp, ns_elem_t **parentpp);
/* lookup flags, see acpi.h */

/* lookup return values, other than OK and EXC */
#define	NS_PONLY	0x0002	/* found parent only */

/* used with ns lookup */
#define	KEY_IF_EXE (pstp->pctx & PCTX_EXECUTE ? pstp->key : 0)


extern ns_elem_t *ns_define_here(ns_elem_t *parent, acpi_nameseg_t *segp,
    void *key, int offset);
extern ns_elem_t *ns_define(ns_elem_t *rootp, ns_elem_t *curp,
    struct name *namep, void *skey, void *dkey, int offset);
extern ns_elem_t *ns_dynamic_copy(ns_elem_t *src, void *key);
extern void ns_print(ns_elem_t *nsp, int indent);
extern void ns_undefine(ns_elem_t *nsp);
extern void ns_undefine_block(ns_elem_t *nsp, void *key);

/* for namespace stack */
typedef struct {
	ns_elem_t *elem;
} ns_entry_t;

extern char ns_buf[10240];

/* macros assume address of namespace stack is pstp->nssp */
#define	NS_PUSH(NP) \
if ((NP = (ns_entry_t *)stack_push(pstp->nssp)) == NULL) \
	return (ACPI_EXC)
#define	NS_POP if (stack_pop(pstp->nssp)) return (ACPI_EXC)
#define	NS_PTR ((ns_entry_t *)stack_top(pstp->nssp))
#define	NSP_ROOT ((ns_elem_t *)(pstp->ns))
#define	NSP_CUR (NS_PTR->elem)


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_NS_H */
