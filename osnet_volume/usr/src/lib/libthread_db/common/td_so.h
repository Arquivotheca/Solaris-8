/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TD_SO_H
#define	_TD_SO_H

#pragma ident	"@(#)td_so.h	1.26	94/12/31 SMI"

/*
* MODULE_td_so.h____________________________________________________
*  Description:
____________________________________________________________________ */

#include "td_to.h"

#ifdef TD_INITIALIZER

char	*td_sync_type_names[] = {
	"S_COND",
	"S_MUTEX",
	"S_SEMA",
	"S_RWLOCK"
};

#else
extern char	*td_sync_type_names[];
#endif

union td_so_un {
	mutex_t		lock;
	rwlock_t	rwlock;
	sema_t		semaphore;
	cond_t		condition;
};

typedef union td_so_un td_so_un_t;

#endif /* _TD_SO_H */
