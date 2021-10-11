/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TD_IMPL_H
#define	_TD_IMPL_H

#pragma ident	"@(#)td_impl.h	1.3	96/12/13 SMI"

/*
* MODULE_TD_IMPL_H___________________________________________________
*
*  Description:
*	Local header file for td.c module
*____________________________________________________________________ */

/*
 * Local variables for td.c
 */

/*
 * Flag that indicates that td_init has already been called once.  This
 * is used to prevent re-initialization of locks.
 */

static td_initialized = 0;

#include "td.h"

#endif /* _TD_IMPL_H */
