/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * eisa.h -- public definitions for eisa routines
 */

#ifndef	_EISA_H
#define	_EISA_H

#ident "@(#)eisa.h   1.15   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

void init_eisa();
void enumerator_eisa(void);

/*
 * Public eisa globals
 */
extern int Eisa; /* set after init_eisa(), non zero if eisa bus */


#ifdef	__cplusplus
}
#endif

#endif	/* _EISA_H */
