/*
 *      Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _XTD_EXTDCL_H
#define	_XTD_EXTDCL_H

#pragma ident	"@(#)xtd.extdcl.h	1.15	97/09/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "xtd_to.h"
extern	td_err_e __td_write_thr_struct(td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread_t * thr_struct_p);
#ifdef  _SYSCALL32_IMPL
extern	td_err_e __td_write_thr_struct32(td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread32_t * thr_struct_p);
#endif /* _SYSCALL32_IMPL */

#ifdef	__cplusplus
}
#endif

#endif /* _XTD_EXTDCL_H */
