/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Common code and structures used by name-service-switch "user" backends.
 */

#ifndef _USER_COMMON_H
#define	_USER_COMMON_H

#pragma ident	"@(#)user_common.h	1.2	99/05/06 SMI"

#include "synonyms.h"
#include <nss_common.h>
#include <nss_dbdefs.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct user_backend *user_backend_ptr_t;
typedef nss_status_t	(*user_backend_op_t)(user_backend_ptr_t, void *);



struct user_backend {
	user_backend_op_t	*ops;
	int			n_ops;
	const char		*filename;
	FILE			*f;
	int			minbuf;
	char			*buf;
};

/*
 * Iterator function for _nss_user_do_all()
 *   NSS_NOTFOUND means "keep enumerating", NSS_SUCCESS means"return now",
 *   other values don't make much sense.  In other words we're abusing
 *   (overloading) the meaning of nss_status_t, but hey...
 * _nss_user_XY_all() is a wrapper around _nss_user_do_all() that does the
 *   generic work for nss_XbyY_args_t backends (calls cstr2ent etc).
 */
typedef nss_status_t	(*user_do_all_func_t)(const char *, int, void *args);
typedef int		(*user_XY_check_func)(nss_XbyY_args_t *);

#if defined(__STDC__)
extern nss_backend_t	*_nss_user_constr(user_backend_op_t	*ops,
					int			n_ops,
					const char		*filename,
					int			min_bufsize);
extern nss_status_t	_nss_user_destr (user_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_user_setent(user_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_user_endent(user_backend_ptr_t, void *dummy);
extern nss_status_t 	_nss_user_do_all(user_backend_ptr_t,
					void			*func_priv,
					const char		*filter,
					user_do_all_func_t	func);
extern nss_status_t 	_nss_user_XY_all(user_backend_ptr_t	be,
					nss_XbyY_args_t		*args,
					int 			netdb,
					const char		*filter,
					user_XY_check_func	check);
extern int		_nss_user_read_line(FILE		*f,
					char			*buffer,
					int			buflen);
#else
extern nss_backend_t	*_nss_user_constr();
extern nss_status_t	_nss_user_destr ();
extern nss_status_t	_nss_user_setent();
extern nss_status_t	_nss_user_endent();
extern nss_status_t	_nss_user_do_all();
extern nss_status_t	_nss_user_XY_all();
extern int		_nss_user_read_line();
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _USER_COMMON_H */
