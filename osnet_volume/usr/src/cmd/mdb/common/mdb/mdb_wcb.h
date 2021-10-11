/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_WCB_H
#define	_MDB_WCB_H

#pragma ident	"@(#)mdb_wcb.h	1.1	99/08/11 SMI"

#include <mdb/mdb_module.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	WCB_TAG_ACTIVE	0xcba1cba1	/* Magic tag for active wcb */
#define	WCB_TAG_PASSIVE	0xcbdeadcb	/* Magic tag for inactive wcb */

struct mdb_frame;			/* Forward declaration */

typedef struct mdb_wcb {
	mdb_walk_state_t w_state;	/* Walk soft state */
	uint32_t w_buftag;		/* Boundary tag for validation */
	int w_inited;			/* Set if we've called walk_init */
	struct mdb_wcb *w_lyr_head;	/* Link to head wcb in layer chain */
	struct mdb_wcb *w_lyr_link;	/* Link to next wcb in layer chain */
	struct mdb_wcb *w_link;		/* Link to next wcb in global chain */
	const mdb_iwalker_t *w_walker;	/* Walker corresponding to this wcb */
} mdb_wcb_t;

extern mdb_wcb_t *mdb_wcb_create(mdb_iwalker_t *, mdb_walk_cb_t,
    void *, uintptr_t);

extern void mdb_wcb_destroy(mdb_wcb_t *);
extern mdb_wcb_t *mdb_wcb_from_state(mdb_walk_state_t *);

extern void mdb_wcb_insert(mdb_wcb_t *, struct mdb_frame *);
extern void mdb_wcb_delete(mdb_wcb_t *, struct mdb_frame *);
extern void mdb_wcb_purge(mdb_wcb_t **);

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_WCB_H */
