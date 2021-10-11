/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _XTD_EXTDCL_H
#define	_XTD_EXTDCL_H

#pragma ident	"@(#)xtd.extdcl.h	1.12	96/12/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include "xtd_to.h"
extern	td_err_e __td_write_thr_struct(td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread_t * thr_struct_p);

#ifdef	__cplusplus
}
#endif

#endif /* _XTD_EXTDCL_H */
