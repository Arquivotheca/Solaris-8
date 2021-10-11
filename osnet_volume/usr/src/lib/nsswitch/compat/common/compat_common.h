/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 *
 * Common code and structures used by name-service-switch "compat" backends.
 */

#ifndef _COMPAT_COMMON_H
#define	_COMPAT_COMMON_H

#pragma ident	"@(#)compat_common.h	1.8	93/05/28 SMI"

#include <nss_common.h>
#include <nss_dbdefs.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct compat_backend *compat_backend_ptr_t;
typedef nss_status_t	(*compat_backend_op_t)(compat_backend_ptr_t, void *);

/*
 * ===> Fix da comments (and in files_common.h too...)
 * Iterator function for _nss_files_do_all(), which probably calls yp_all().
 *   NSS_NOTFOUND means "keep enumerating", NSS_SUCCESS means"return now",
 *   other values don't make much sense.  In other words we're abusing
 *   (overloading) the meaning of nss_status_t, but hey...
 * _nss_compat_XY_all() is a wrapper around _nss_files_do_all() that does the
 *   generic work for nss_XbyY_args_t backends (calls cstr2ent etc).
 */
typedef nss_status_t	(*files_do_all_func_t)(const char *, int, void *args);
/* ===> ^^ nuke this line */
typedef int		(*compat_XY_check_func)(nss_XbyY_args_t *);
typedef const char *	(*compat_get_name)(nss_XbyY_args_t *);
typedef int		(*compat_merge_func)(compat_backend_ptr_t,
					    nss_XbyY_args_t	*,
					    const char		**fields);

#if defined(__STDC__)
extern nss_backend_t	*_nss_compat_constr(compat_backend_op_t	*ops,
					    int			n_ops,
					    const char		*filename,
					    int			min_bufsize,
					    nss_db_root_t	*rootp,
					    nss_db_initf_t	initf,
					    int			netgroups,
					    compat_get_name	getname_func,
					    compat_merge_func	merge_func);
extern nss_status_t	_nss_compat_destr (compat_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_compat_setent(compat_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_compat_endent(compat_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_compat_getent(compat_backend_ptr_t, void *);
extern nss_status_t 	_nss_compat_XY_all(compat_backend_ptr_t,
					nss_XbyY_args_t	*args,
					compat_XY_check_func	check,
					nss_dbop_t		op_num);
#else
extern nss_backend_t	*_nss_compat_constr();
extern nss_status_t	_nss_compat_destr ();
extern nss_status_t	_nss_compat_setent();
extern nss_status_t	_nss_compat_endent();
extern nss_status_t	_nss_compat_getent();
extern nss_status_t	_nss_compat_XY_all();
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _COMPAT_COMMON_H */
