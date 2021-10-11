/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_IDTAB_H
#define	_IDTAB_H

#pragma ident	"@(#)idtab.h	1.1	97/12/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * We need to typedef idkey_t so it can safely deal with how pid_t, uid_t,
 * gid_t, and dev_t are defined in a 64-bit compilation environment and
 * avoid sign-extension problems.
 */

typedef unsigned long idkey_t;

typedef struct idtab {
	idkey_t *id_data;
	size_t id_nelems;
	size_t id_size;
} idtab_t;

extern void idtab_create(idtab_t *);
extern void idtab_destroy(idtab_t *);
extern void idtab_append(idtab_t *, idkey_t);
extern void idtab_sort(idtab_t *);
extern int idtab_search(idtab_t *, idkey_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _IDTAB_H */
