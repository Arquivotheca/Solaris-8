/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SWAP_H
#define	_SWAP_H

#pragma ident	"@(#)swap.h	1.1	98/05/01 SMI"

#include <sys/types.h>
#include <sys/swap.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern swaptbl_t *swap_list(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _SWAP_H */
