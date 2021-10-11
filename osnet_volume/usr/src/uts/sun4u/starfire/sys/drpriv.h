/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved
 */

#ifndef _SYS_DRPRIV_H
#define	_SYS_DRPRIV_H

#pragma ident	"@(#)drpriv.h	1.8	98/05/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>

/*
 * Contains definitions used internally by the PIM-DR layer.
 */

/*
 * wrappers for dr_ops_t calls.
 */
#define	_DR_PLAT_OP(f, p, r)	((f) ? ((*f)p) : (r))

#define	DR_PLAT_GET_HANDLE(d, s, a, f, i) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_handle, \
				((d), (s), (a), (f), (i)), NULL)
#define	DR_PLAT_RELEASE_HANDLE(h, f) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_release_handle, \
				((h), (f)), (void)0)
#define	DR_PLAT_PRE_OP(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_pre_op, (h), 0)
#define	DR_PLAT_POST_OP(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_post_op, (h), (void)0)
#define	DR_PLAT_PROBE_BOARD(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_probe_board, (h), 0)
#define	DR_PLAT_DEPROBE_BOARD(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_deprobe_board, (h), 0)
#define	DR_PLAT_CONNECT(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_connect, (h), 0)
#define	DR_PLAT_DISCONNECT(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_disconnect, (h), 0)
#define	DR_PLAT_GET_ATTACH_DEVLIST(h, n, p) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_attach_devlist, \
				((h), (n), (p)), NULL)
#define	DR_PLAT_PRE_ATTACH_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_pre_attach_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_POST_ATTACH_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_post_attach_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_GET_RELEASE_DEVLIST(h, n, p) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_release_devlist, \
				((h), (n), (p)), NULL)
#define	DR_PLAT_PRE_RELEASE_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_pre_release_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_POST_RELEASE_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_post_release_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_RELEASE_DONE(h, t, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_release_done, \
				((h), (t), (n)), B_TRUE)
#define	DR_PLAT_GET_DETACH_DEVLIST(h, n, p) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_detach_devlist, \
				((h), (n), (p)), NULL)
#define	DR_PLAT_PRE_DETACH_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_pre_detach_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_POST_DETACH_DEVLIST(h, d, n) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_post_detach_devlist, \
				((h), (d), (n)), 0)
#define	DR_PLAT_CANCEL(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_cancel, (h), 0)
#define	DR_PLAT_STATUS(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_status, (h), 0)
#define	DR_PLAT_IOCTL(h) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_ioctl, (h), 0)
#define	DR_PLAT_GET_MEMHANDLE(h, d, m) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_memhandle, \
				((h), (d), (m)), -1)
#define	DR_PLAT_GET_MEMLIST(d) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_memlist, (d), NULL)
#define	DR_PLAT_DETACH_MEM(h, d) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_detach_mem, ((h), (d)), 0)
#define	DR_PLAT_GET_DEVTYPE(h, d) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_devtype, \
				((h), (d)), DR_NT_UNKNOWN)
#define	DR_PLAT_GET_CPUID(h, d) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_get_cpuid, \
				((h), (d)), (processorid_t)-1)
#define	DR_PLAT_MAKE_NODES(d) \
		_DR_PLAT_OP(dr_g.ops->dr_platform_make_nodes, (d), 0)

/*
 * macros for sanity checks and programming sanity.
 */
#define	GET_HERRNO(h)		((h)->h_err->e_errno)
#define	GET_SOFTC(i)		ddi_get_soft_state(dr_g.softsp, (i))
#define	ALLOC_SOFTC(i)		ddi_soft_state_zalloc(dr_g.softsp, (i))
#define	FREE_SOFTC(i)		ddi_soft_state_free(dr_g.softsp, (i))

/*
 * handle needs dr_ops_t and a board_t.
 */
#define	VALID_HANDLE(h)		((((h)->h_bd != NULL) && \
				((h)->h_err != NULL)) ? B_TRUE : B_FALSE)

/*
 * global DR (PIM) lock management.
 */
#define	DR_DRV_LOCK()		(mutex_enter(&dr_g.lock))
#define	DR_DRV_UNLOCK()		(mutex_exit(&dr_g.lock))

/*
 * Per instance soft-state structure.
 */
typedef struct dr_softstate {
	dev_info_t	*dip;
	void		*machsoftsp;
} dr_softstate_t;

typedef const char *const fn_t;

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DRPRIV_H */
