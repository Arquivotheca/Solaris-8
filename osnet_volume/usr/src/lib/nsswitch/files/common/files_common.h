/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Common code and structures used by name-service-switch "files" backends.
 */

#ifndef _FILES_COMMON_H
#define	_FILES_COMMON_H

#pragma ident	"@(#)files_common.h	1.9	97/08/12 SMI"

#include "synonyms.h"
#include <nss_common.h>
#include <nss_dbdefs.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct files_backend *files_backend_ptr_t;
typedef nss_status_t	(*files_backend_op_t)(files_backend_ptr_t, void *);

typedef u_int (*files_hash_func)(nss_XbyY_args_t *, int);

typedef struct files_hashent {
	struct files_hashent	*h_first;
	struct files_hashent	*h_next;
	u_int			h_hash;
} files_hashent_t;

typedef struct {
	char			*l_start;
	int			l_len;
} files_linetab_t;

typedef struct {
	mutex_t		fh_lock;
	int		fh_resultsize;
	int		fh_bufsize;
	int		fh_nhtab;
	files_hash_func	*fh_hash_func;
	int		fh_refcnt;
	int		fh_size;
	timestruc_t	fh_mtime;
	char		*fh_file_start;
	char		*fh_file_end;
	files_linetab_t	*fh_line;
	files_hashent_t	*fh_table;
} files_hash_t;

struct files_backend {
	files_backend_op_t	*ops;
	int			n_ops;
	const char		*filename;
	FILE			*f;
	int			minbuf;
	char			*buf;
	files_hash_t		*hashinfo;
};

/*
 * Iterator function for _nss_files_do_all(), which probably calls yp_all().
 *   NSS_NOTFOUND means "keep enumerating", NSS_SUCCESS means"return now",
 *   other values don't make much sense.  In other words we're abusing
 *   (overloading) the meaning of nss_status_t, but hey...
 * _nss_files_XY_all() is a wrapper around _nss_files_do_all() that does the
 *   generic work for nss_XbyY_args_t backends (calls cstr2ent etc).
 */
typedef nss_status_t	(*files_do_all_func_t)(const char *, int, void *args);
typedef int		(*files_XY_check_func)(nss_XbyY_args_t *);

#if defined(__STDC__)
extern nss_backend_t	*_nss_files_constr(files_backend_op_t	*ops,
					int			n_ops,
					const char		*filename,
					int			min_bufsize,
					files_hash_t		*fhp);
extern nss_status_t	_nss_files_destr (files_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_files_setent(files_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_files_endent(files_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_files_getent_rigid(files_backend_ptr_t, void *);
extern nss_status_t	_nss_files_getent_netdb(files_backend_ptr_t, void *);
extern nss_status_t 	_nss_files_do_all(files_backend_ptr_t,
					void			*func_priv,
					const char		*filter,
					files_do_all_func_t	func);
extern nss_status_t 	_nss_files_XY_all(files_backend_ptr_t	be,
					nss_XbyY_args_t		*args,
					int 			netdb,
					const char		*filter,
					files_XY_check_func	check);
extern nss_status_t 	_nss_files_XY_hash(files_backend_ptr_t	be,
					nss_XbyY_args_t		*args,
					int 			netdb,
					files_hash_t		*fhp,
					int			hashop,
					files_XY_check_func	check);
int _nss_files_read_line(FILE *f, char *buffer, int	buflen);
#else
extern nss_backend_t	*_nss_files_constr();
extern nss_status_t	_nss_files_destr ();
extern nss_status_t	_nss_files_setent();
extern nss_status_t	_nss_files_endent();
extern nss_status_t	_nss_files_getent_rigid();
extern nss_status_t	_nss_files_getent_netdb();
extern nss_status_t	_nss_files_do_all();
extern nss_status_t	_nss_files_XY_all();
extern nss_status_t	_nss_files_XY_hash();
#endif

#ifdef	__cplusplus
}
#endif

#endif /* _FILES_COMMON_H */
