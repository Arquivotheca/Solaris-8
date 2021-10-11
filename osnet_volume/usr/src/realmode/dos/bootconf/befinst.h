/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * befinst.h -- public definitions for bef installation routines
 */

#ifndef	_BEFINST_H
#define	_BEFINST_H

#ident "@(#)befinst.h   1.13   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void(cdecl interrupt far * cdecl int_13_initial)();

/*
 * Public befinst function prototypes
 */

void init_befinst();
struct board;
int install_bef_befinst(char *bef_name, struct board *bp, int bios_primary);
void deinstall_bef_befinst();

#ifdef	__cplusplus
}
#endif

#endif	/* _BEFINST_H */
