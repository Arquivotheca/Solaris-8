/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _XTD_PUBDCL_H
#define	_XTD_PUBDCL_H

#pragma ident	"@(#)xtd.pubdcl.h	1.9	96/06/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern td_err_e td_ta_map_lwp2thr(const td_thragent_t *ta_p,
	lwpid_t lwpid, td_thrhandle_t *th_p);


extern td_err_e td_thr_getgregs(const td_thrhandle_t *th_p,
	prgregset_t regset);
extern td_err_e td_thr_setgregs(const td_thrhandle_t *th_p,
	const prgregset_t regset);

#ifdef	__cplusplus
}
#endif

#endif /* _XTD_PUBDCL_H */
