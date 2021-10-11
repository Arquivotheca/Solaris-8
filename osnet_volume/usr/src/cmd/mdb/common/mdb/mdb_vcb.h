/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_MDB_VCB_H
#define	_MDB_VCB_H

#pragma ident	"@(#)mdb_vcb.h	1.1	99/08/11 SMI"

#include <mdb/mdb_addrvec.h>
#include <mdb/mdb_nv.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _MDB

struct mdb_frame;			/* Forward declaration */
struct mdb_cmd;				/* Forward declaration */

typedef struct mdb_vcb {
	mdb_var_t *vc_var;		/* Pointer to dependent variable */
	mdb_addrvec_t vc_addrv;		/* List of address values */
	size_t vc_adnext;		/* Next index for vc_addrv */
	struct mdb_vcb *vc_link;	/* Pointer to next vcb in list */
	struct mdb_vcb *vc_parent;	/* Pointer to parent vcb */
} mdb_vcb_t;

extern mdb_vcb_t *mdb_vcb_create(mdb_var_t *);
extern void mdb_vcb_destroy(mdb_vcb_t *);

extern void mdb_vcb_propagate(mdb_vcb_t *);
extern void mdb_vcb_purge(mdb_vcb_t *);

extern void mdb_vcb_inherit(struct mdb_cmd *, struct mdb_cmd *);
extern void mdb_vcb_insert(mdb_vcb_t *, struct mdb_frame *);
extern void mdb_vcb_update(struct mdb_frame *, uintptr_t);

#endif	/* _MDB */

#ifdef	__cplusplus
}
#endif

#endif	/* _MDB_VCB_H */
