/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DEVINFO_H
#define	_DEVINFO_H

#pragma ident	"@(#)devinfo.h	1.3	99/11/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <mdb/mdb_modapi.h>

extern int devinfo_walk_init(mdb_walk_state_t *);
extern int devinfo_walk_step(mdb_walk_state_t *);
extern void devinfo_walk_fini(mdb_walk_state_t *);

extern int devinfo_parents_walk_init(mdb_walk_state_t *);
extern int devinfo_parents_walk_step(mdb_walk_state_t *);
extern void devinfo_parents_walk_fini(mdb_walk_state_t *);

extern int devinfo_children_walk_init(mdb_walk_state_t *);
extern int devinfo_children_walk_step(mdb_walk_state_t *);
extern void devinfo_children_walk_fini(mdb_walk_state_t *);

extern int devnames_walk_init(mdb_walk_state_t *);
extern int devnames_walk_step(mdb_walk_state_t *);
extern void devnames_walk_fini(mdb_walk_state_t *);

extern int devi_next_walk_step(mdb_walk_state_t *);

extern int prtconf(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int devinfo(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int modctl2devinfo(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int devnames(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int devbindings(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int name2major(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int major2name(uintptr_t, uint_t, int, const mdb_arg_t *);
extern int softstate(uintptr_t, uint_t, int, const mdb_arg_t *);


extern void prtconf_help(void);

extern uintptr_t devinfo_root;	/* Address of root of devinfo tree */

#ifdef	__cplusplus
}
#endif

#endif	/* _DEVINFO_H */
