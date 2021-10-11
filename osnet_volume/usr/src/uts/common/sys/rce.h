/*
 * Copyright (c) 1989-1998 by Softway Pty Ltd, Sydney Australia.
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This is unpublished proprietary source code of Softway Pty Ltd.
 * The contents of this file are protected by copyright laws and
 * international copyright treaties, as well as other intellectual
 * property laws and treaties.  These contents may not be extracted,
 * copied,  modified or redistributed either as a whole or part thereof
 * in any form, and may not be used directly in, or to assist with,
 * the creation of derivative works of any nature without prior
 * written permission from Softway Pty Ltd. The above copyright notice
 * does not evidence any actual or intended publication of this file.
 */

/*
 * RCE System kernel support.
 */

#ifndef	_SYS_RCE_H
#define	_SYS_RCE_H

#pragma ident	"@(#)rce.h	1.21	99/07/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/priocntl.h>
#include <sys/proc.h>

/*
 * The purpose of SRM_IF_MAJOR and SRM_IF_MINOR is to ensure consistency
 * of the version of RCE interface mechanism used. We want to ensure
 * that the interface that a vendor uses to compile their kernel with
 * is the same version (or compatible with) that is used to compile the
 * RCE module/library.
 * _SRM_INTERFACE_VERSION is a string "SRM_IF_MAJOR.SRM_IF_MINOR".
 *
 * At initialisation RCE checks the content of _srm_interface_version
 * and may require that a certain version is current or may adapt how it
 * presents the interface based on what the kernel expects.
 *
 * NOTE: Subsequent minor level versions at the same major level are
 * expected to be backwards compatible with the preceeding minor level.
 * However, no compatibility is guaranteed between different major levels.
 */

#define	SRM_IF_MAJOR		6
#define	SRM_IF_MINOR		0
#define	SRM_STRINGIT(s)		#s
#define	SRM_MK_VERSION(m, n)	SRM_STRINGIT(m) "." SRM_STRINGIT(n)
#define	_SRM_INTERFACE_VERSION	SRM_MK_VERSION(SRM_IF_MAJOR, SRM_IF_MINOR)

extern const char _srm_interface_version[];

/*
 * Prototypes for various SRM hooks called from the mainline kernel code.
 */

struct as;

typedef struct rce_interface {
	int	(*rce_start)(id_t, int);
	int	(*rce_limitmemory)(size_t, struct as *, int);
	void	(*rce_exit)(proc_t *);
	void	(*rce_flush)(ulong_t);
	int	(*rce_proccreate)(proc_t *);
	void	(*rce_procdestroy)(proc_t *);
	void	(*rce_setuid)(int, cred_t *);
	void	(*rce_lwpfail)(proc_t *, kthread_id_t);
	void	(*rce_lwpexit)(proc_t *, kthread_id_t);
	int	(*rce_lwpnew)(proc_t *);
	uid_t	(*rce_limid)(proc_t *);
} rce_interface_t;

/*
 * The rce_ops pointer is NULL while the RCE module is not loaded or has
 * not yet initialised. During initialization it is set to point at the
 * structure populated with addresses of the hooks.
 */

extern volatile rce_interface_t	*rce_ops;

#define	SRM_ON			rce_ops
#define	SRM_ACTIVE		(rce_ops != NULL)
#define	SRM_START(c, p)		(SRM_ON ? rce_ops->rce_start((c), (p)) : 0)
#define	SRM_LIMITMEMORY(n, a, l) \
	(SRM_ON ? rce_ops->rce_limitmemory((n), (a), (l)) : 0)
#define	SRM_EXIT(p)		(SRM_ON ? rce_ops->rce_exit((p)) : (void)0)
#define	SRM_FLUSH(a)		(SRM_ON ? rce_ops->rce_flush((a)) : (void)0)
#define	SRM_PROCCREATE(p)	(SRM_ON ? rce_ops->rce_proccreate((p)) : 0)
#define	SRM_PROCDESTROY(p)	\
	(SRM_ON ? rce_ops->rce_procdestroy((p)) : (void)0)
#define	SRM_SETUID(e, c)	\
	(SRM_ON ? rce_ops->rce_setuid((e), (c)) : (void)0)
#define	SRM_LWPFAIL(p, t)	\
	(SRM_ON ? rce_ops->rce_lwpfail((p), (t)) : (void)0)
#define	SRM_LWPEXIT(p, t)	\
	(SRM_ON ? rce_ops->rce_lwpexit((p), (t)) : (void)0)
#define	SRM_LWPNEW(p)		(SRM_ON ? rce_ops->rce_lwpnew((p)) : 0)
#define	SRM_LIMID(p)		(SRM_ON ? rce_ops->rce_limid((p)) : 0)

/*
 * These are used to control the behaviour of SRM_LIMITMEMORY()
 */
#define	LI_ENFORCE	(0x01)	/* return error if denied & don't update */
#define	LI_UPDATE	(0x02)	/* update allocation record */
#define	LI_ALLOC	(0x04)	/* allocation increase */
#define	LI_FREE		(0x08)	/* allocation decrease */
#define	LI_DUP		(0x10)	/* adjust the allocation when duplicating */

/*
 * These are used to notify SRM that system is going to be suspended or resumed.
 */
#define	SH_NOW		(0UL)
#define	SH_CPR		(-1UL)
#define	SH_SUSPEND	SH_CPR
#define	SH_RESUME	SH_CPR

/*
 * These are used in SHR_SETUID() to determine which uid is changing.
 */
#define	SH_RUID	0
#define	SH_EUID	1

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_RCE_H */
