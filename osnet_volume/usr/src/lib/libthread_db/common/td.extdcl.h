/*
 *	Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef _TD_EXTDCL_H
#define	_TD_EXTDCL_H

#pragma ident	"@(#)td.extdcl.h	1.25	97/11/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*  ../common/td.c */
extern td_err_e
__td_pdmodel(struct ps_prochandle *ph_p, int *model);


/*  ../common/td_po.c */
/*
 * Iterate over a process's threads.  Except for the leading
 * double underscore, this is identical to the declaration
 * in thread_db.h.
 */
td_err_e
__td_ta_thr_iter(const td_thragent_t *ta_p, td_thr_iter_f *cb, void *cbdata_p,
	td_thr_state_e state, int ti_pri, sigset_t *ti_sigmask_p,
	unsigned ti_user_flags);


/*  ../common/td_to.c */

#include "td_to.h"
extern td_err_e
__td_read_thr_struct(const td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread_t *thr_struct_p);
#ifdef  _SYSCALL32_IMPL
extern td_err_e
__td_read_thr_struct32(const td_thragent_t *ta_p,
	psaddr_t thr_addr, uthread32_t *thr_struct_p);
#endif /* _SYSCALL32_IMPL */
extern td_err_e
__td_thr_map_state(thstate_t ts_state, td_thr_state_e *to_state);
extern td_err_e
__td_read_thread_hash_tbl(const td_thragent_t *ta_p,
	thrtab_t * tab_p, int tab_size);
#ifdef  _SYSCALL32_IMPL
extern td_err_e
__td_read_thread_hash_tbl32(const td_thragent_t *ta_p,
	thrtab32_t * tab_p, int tab_size);
#endif /* _SYSCALL32_IMPL */
extern td_err_e __td_ti_validate(const td_thrinfo_t *ti_p);
extern void	__td_tsd_dump(const td_thrinfo_t *ti_p, const thread_key_t key);
extern void	__td_ti_dump(const td_thrinfo_t *ti_p, int full);
extern int	__td_sigmask_are_equal(sigset_t * mask1_p, sigset_t * mask2_p);

/*  ../common/td_so.c */

#include "td_so.h"
#ifdef TD_INTERNAL_TESTS
extern void	__td_si_dump(const td_syncinfo_t *si_p);
#endif

/*  ../common/td_error.c */

#ifdef TD_INTERNAL_TESTS
extern void	__td_report_db_err(ps_err_e error, char *s);
extern void	__td_report_po_err(td_err_e error, char *s);
extern void	__td_report_to_err(td_err_e error, char *s);
extern void	__td_report_so_err(td_err_e error, char *s);
#else
#define	__td_report_db_err(x1, x2)
#define	__td_report_po_err(x1, x2)
#define	__td_report_to_err(x1, x2)
#define	__td_report_so_err(x1, x2)
#endif

/*  ../common/td_event.c */

#ifdef	__cplusplus
}
#endif

#endif /* _TD_EXTDCL_H */
