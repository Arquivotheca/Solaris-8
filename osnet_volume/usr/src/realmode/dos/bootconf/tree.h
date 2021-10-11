/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * tree.h -- public definitions for tree building routines
 */

#ifndef	_TREE_H
#define	_TREE_H

#ident "@(#)tree.h   1.9   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Public function prototypes
 */

void init_tree();
void build_tree(bef_dev *boot_bdp);
int ffbs(long mask);
int weak_binding_tree(Board *bp);

#ifdef	__cplusplus
}
#endif

#endif	/* _TREE_H */
