/*
 *      Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _TD_PO_H
#define	_TD_PO_H

#pragma ident	"@(#)td_po.h	1.35	97/11/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "tdb_agent.h"

/*
* MODULE_td_po.h____________________________________________________
*
*  Description:
*	Header for libthread_db thread agents.
____________________________________________________________________ */

struct td_thragent {
	struct ps_prochandle		*ph_p;
	rwlock_t			rwlock;
	tdb_agent_data_t		tdb_agent_data;
	tdb_invar_data_t		tdb_invar;
#ifdef  _SYSCALL32_IMPL
	tdb_invar_data32_t		tdb_invar32;
	tdb_agent_data32_t		tdb_agent_data32;
#endif /* _SYSCALL32_IMPL */
};

/*
 * This is the name of the structure in libthread containing all
 * the addresses we will need.
 */
#define	TD_LIBTHREAD_NAME	"libthread.so"
#define	TD_INVAR_DATA_NAME	"__tdb_invar_data"
#define	TD_T0_NAME		"_t0"

#ifdef	__cplusplus
}
#endif

#endif /* _TD_PO_H */
