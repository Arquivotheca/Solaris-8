/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_ns.c	1.1	99/05/21 SMI"


/* ACPI name space */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#endif

#ifdef ACPI_USER
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#endif

#include "acpi_exc.h"
#include "acpi_node.h"

#include "acpi_name.h"
#include "acpi_ns.h"
#include "acpi_val.h"


/* forward decl */
ns_elem_t *ns_define_here(ns_elem_t *parent, acpi_nameseg_t *segp, void *key,
    int offset);

/* uninit value by default */
ns_elem_t *
ns_new(void)
{
	ns_elem_t *new;

	if ((new = kmem_alloc(sizeof (ns_elem_t), KM_SLEEP)) == NULL)
		return (exc_null(ACPI_ERES));
	bzero(new, sizeof (ns_elem_t));
	if ((new->valp = uninit_new()) == NULL)
		return (NULL);
	return (new);
}

void
ns_free(ns_elem_t *nsp)
{
	if (nsp)
		kmem_free(nsp, sizeof (ns_elem_t));
}

ns_elem_t *
ns_root_new(void *key)
{
	static acpi_nameseg_t gpe = { 0x4550475F, }; /* _GPE */
	static acpi_nameseg_t pr = { 0x5F52505F, }; /* _PR */
	static acpi_nameseg_t sb = { 0x5F42535F, }; /* _SB */
	static acpi_nameseg_t si = { 0x5F49535F, }; /* _SI */
	static acpi_nameseg_t tz = { 0x5F5A545F, }; /* _TZ */
	static acpi_nameseg_t gl = { 0x5F4C475F, }; /* _GL */
	static acpi_nameseg_t os = { 0x5F534F5F, }; /* _OS */
	static acpi_nameseg_t rev = { 0x5645525F, }; /* _REV */
	ns_elem_t *new, *gl_nsp, *os_nsp, *rev_nsp;

	if ((new = ns_new()) == NULL)
		return (NULL);
	new->flags = NS_ROOT;
	new->nse_ddbh = key;
				/* add predefined names */
	if (ns_define_here(new, &tz, key, 0) == NULL ||
	    ns_define_here(new, &si, key, 0) == NULL ||
	    ns_define_here(new, &sb, key, 0) == NULL ||
	    (rev_nsp = ns_define_here(new, &rev, key, 0)) == NULL ||
	    ns_define_here(new, &pr, key, 0) == NULL ||
	    (os_nsp = ns_define_here(new, &os, key, 0)) == NULL ||
	    ns_define_here(new, &gpe, key, 0) == NULL ||
	    (gl_nsp = ns_define_here(new, &gl, key, 0)) == NULL) {
		ns_free(new);
		return (NULL);
	}
	if ((gl_nsp->valp = mutex_new(0)) == NULL ||
	    /* the Solaris string just happens to be a multiple of 4 already */
	    (os_nsp->valp = string_new("Solaris")) == NULL ||
	    (rev_nsp->valp = integer_new(acpi_interpreter_revision)) == NULL)
		return (NULL);
	return (new);
}


int
cseg_lt(unsigned int ui1, unsigned int ui2)
{
	acpi_nameseg_t seg1, seg2;

	seg1.iseg = ui1;
	seg2.iseg = ui2;
	if (seg1.cseg[0] < seg2.cseg[0])
		return (1);
	if (seg1.cseg[0] > seg2.cseg[0])
		return (0);
	if (seg1.cseg[1] < seg2.cseg[1])
		return (1);
	if (seg1.cseg[1] > seg2.cseg[1])
		return (0);
	if (seg1.cseg[2] < seg2.cseg[2])
		return (1);
	if (seg1.cseg[2] > seg2.cseg[2])
		return (0);
	if (seg1.cseg[3] < seg2.cseg[3])
		return (1);
	return (0);
}


/* search in the immediate children of this ns element */
ns_elem_t *
ns_lookup_here(ns_elem_t *nsp, acpi_nameseg_t *segp, void *key)
{
	ns_elem_t *ptr;

	for (ptr = ns_first_child(nsp); ptr; ptr = ns_next(ptr)) {
		if (cseg_lt(segp->iseg, ptr->name_seg.iseg))
			break;
		if (segp->iseg == ptr->name_seg.iseg)
		    if ((ptr->flags & NS_DYN_METHOD) == 0 || key == NULL ||
			key == ptr->nse_ddbh)
			    return (ptr);
	}
	return (NULL);
}

int
ns_lookup(ns_elem_t *rootp, ns_elem_t *curp, name_t *namep, void *key,
    int flags, ns_elem_t **targetpp, ns_elem_t **parentpp)
{
	ns_elem_t *ptr, *result;
	acpi_nameseg_t *segp;
	int i;

	if (namep->flags & NAME_ROOT) /* which starting point */
		ptr = rootp;
	else
		ptr = curp;

	if (NAME_PARENTS(namep)) /* apply parent operators */
		for (i = 0; i < namep->gens; i++) {
			if (ns_parent(ptr))
				ptr = ns_parent(ptr);
			else
				return (ACPI_EXC);
		}
	result = ptr;		/* just in case we have zero segs */

	segp = NAMESEG_BASE(namep);
	/* look for ancestor matches, if appropriate */
	if ((namep->flags & NAME_ROOT) == 0 &&
	    namep->gens == 0 &&
	    namep->segs == 1 &&
	    (flags & ACPI_EXACT) == 0)
		for (;;) {
			if (result = ns_lookup_here(ptr, segp, key)) {
				if (targetpp)
					*targetpp = result;
				if (parentpp)
					*parentpp = ptr;
				return (ACPI_OK);
			}
			if (ns_parent(ptr))
				ptr = ns_parent(ptr);
			else
				return (ACPI_EXC);
		}

	/* match each segment */
	for (i = 0; i < namep->segs; i++, segp++) {
		if ((result = ns_lookup_here(ptr, segp, key)) == NULL) {
			if (i == namep->segs - 1) {
				if (parentpp)
					*parentpp = ptr;
				return (NS_PONLY); /* found parent only */
			}
			return (ACPI_EXC);
		}
		ptr = result;
	}
	if (targetpp)
		*targetpp = result;
	if (parentpp)
		*parentpp = ptr;
	return (ACPI_OK);
}



ns_elem_t *
ns_define_here(ns_elem_t *parent, acpi_nameseg_t *segp, void *key, int offset)
{
	ns_elem_t *new, *ptr, *trail;

	if ((new = ns_new()) == NULL) /* setup new ns */
		return (NULL);
	new->name_seg = *segp;
	new->nse_ddbh = key;
	new->nse_offset = offset;

	/* connect */
	new->node.parent = (node_t *)parent;

	if ((ptr = ns_first_child(parent)) == NULL) {
		parent->node.child = (node_t *)new;
		return (new);
	}
	for (trail = NULL; ptr; trail = ptr, ptr = ns_next(ptr)) {
		if (cseg_lt(new->name_seg.iseg, ptr->name_seg.iseg) ||
		    (new->name_seg.iseg == ptr->name_seg.iseg &&
			new->nse_ddbh < ptr->nse_ddbh))
			break;
	}
	if (trail) {
		new->node.prev = (node_t *)trail;
		trail->node.next = (node_t *)new;
	} else
		parent->node.child = (node_t *)new;
	if (ptr) {
		new->node.next = (node_t *)ptr;
		ptr->node.prev = (node_t *)new;
	}
	return (new);
}


ns_elem_t *
ns_define(ns_elem_t *rootp, ns_elem_t *curp, name_t *namep, void *skey,
    void *dkey, int offset)
{
	ns_elem_t *parentp;
	acpi_nameseg_t *segp;

				/* already exist? */
	if (ns_lookup(rootp, curp, namep, skey, ACPI_EXACT, NULL, &parentp) !=
	    NS_PONLY)
		return (NULL);

	segp = NAMESEG_BASE(namep) + namep->segs - 1; /* last segment */
	return (ns_define_here(parentp, segp, dkey, offset));
}

ns_elem_t *
ns_dynamic_copy(ns_elem_t *src, void *key)
{
	ns_elem_t *new;

	if ((new = ns_define_here(ns_parent(src), &(src->name_seg), key,
	    src->nse_offset)) == NULL)
		return (NULL);
	new->valp = src->valp;
	new->flags = src->flags | NS_DYN_METHOD;
	new->dyn = NULL;	/* will be part of a new chain */
	value_hold(new->valp);
	return (new);
}

void
ns_undefine(ns_elem_t *nsp)
{
	ns_elem_t *save;

	if (nsp->flags & NS_ROOT)
		return;
	if (nsp->valp)
		value_free(nsp->valp);
	node_unlink_subtree(&nsp->node);
	for (nsp = ns_first_child(nsp); nsp; nsp = save) {
		save = ns_next(nsp);
		ns_undefine(nsp);
	}
}

void
ns_undefine_block(ns_elem_t *nsp, void *key)
{
	ns_elem_t *ptr, *next;

	/* don't descend into dynamic object */
	if ((nsp->flags & NS_DYN_METHOD) || nsp->dyn)
		return;
				/* undefine children */
	for (ptr = ns_first_child(nsp); ptr; ptr = next) {
		next = ns_next(ptr); /* get next before it may be undefined */
		ns_undefine_block(ptr, key);
	}
	if (ns_first_child(nsp)) /* still have children? */
		return;

	if (nsp->nse_ddbh != key) /* check other criteria */
		return;
	if (nsp->flags & NS_ROOT)
		return;

	if (nsp->valp)		/* do it */
		value_free(nsp->valp);
	node_unlink_subtree(&nsp->node);
}

void
ns_print(ns_elem_t *nsp, int indent)
{
	ns_elem_t *ep;
	char *ptr;
	char ns_buf[128];
	int i;

	exc_cont("0x%05x 0x%08x ", nsp->nse_offset, nsp->valp);
	ptr = &ns_buf[0];
	for (i = 0; i < indent; i++) {
		*ptr++ = '|';
		*ptr++ = ' ';
	}
	if (nsp->flags & NS_ROOT)
		*ptr++ = '\\';
	else {
		*ptr++ = nsp->name_seg.cseg[0];
		*ptr++ = nsp->name_seg.cseg[1];
		*ptr++ = nsp->name_seg.cseg[2];
		*ptr++ = nsp->name_seg.cseg[3];
	}
	*ptr++ = ' ';
	*ptr = NULL;
	exc_cont(&ns_buf[0]);

	value_print(nsp->valp);
	exc_cont("\n");

	for (ep = ns_first_child(nsp); ep; ep = ns_next(ep))
		ns_print(ep, indent + 1);
}


/* eof */
