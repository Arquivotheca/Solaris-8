/*
 *	Copyright (c) 1991-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	Common code used by name-service-switch "nisplus" backends
 */

#pragma ident "@(#)nisplus_common.h	1.16	99/03/21	 SMI"

#ifndef _NISPLUS_COMMON_H
#define	_NISPLUS_COMMON_H

#include <nss_dbdefs.h>
#include <rpcsvc/nis.h>

/*
 * We want these flags turned on in all nis_list() requests that we perform;
 *   other flags (USE_DGRAM, EXPAND_NAME) are only wanted for some requests.
 */
#define	NIS_LIST_COMMON	(FOLLOW_LINKS | FOLLOW_PATH)

typedef struct nisplus_backend	*nisplus_backend_ptr_t;

typedef nss_status_t (*nisplus_backend_op_t)(nisplus_backend_ptr_t, void *);
typedef int (*nisplus_obj2ent_func)(int nobjs,
						nis_object		*obj,
						nss_XbyY_args_t	*arg);
struct nisplus_backend {
	nisplus_backend_op_t	*ops;
	nss_dbop_t		n_ops;
	const char		*directory;	/* fully qualified directory */
						/* name  */
	char		*table_name;
	/*
	 * table_name is fully qualified (includes org_dir and
	 * directory name) and cached here using one time malloc.
	 */
	nisplus_obj2ent_func	obj2ent;
	struct {
		struct netobj		no;
		u_int			max_len;
	}			cursor;

	/* 
	 * Fields for handling table paths during enumeration.
	 * The path_list field is allocated dynamically because
	 * it is kind of big and most applications don't do
	 * enumeration.
	 */
	char *table_path;
	int path_index;
	int path_count;
	nis_name *path_list;
};
typedef struct nisplus_backend nisplus_backend_t;

#if defined(__STDC__)
extern nss_backend_t	*_nss_nisplus_constr	(nisplus_backend_op_t	*ops,
						int			n_ops,
						const char		*rdn,
						nisplus_obj2ent_func	func);
extern nss_status_t	_nss_nisplus_destr	(nisplus_backend_ptr_t,
						void *dummy);
extern nss_status_t	_nss_nisplus_setent	(nisplus_backend_ptr_t,
						void *dummy);
extern nss_status_t  	_nss_nisplus_endent	(nisplus_backend_ptr_t,
						void *dummy);
extern nss_status_t  	_nss_nisplus_getent	(nisplus_backend_ptr_t,
						void 			*arg);
extern nss_status_t	_nss_nisplus_lookup	(nisplus_backend_ptr_t,
						nss_XbyY_args_t	*arg,
						const char		*key,
						const char		*val);
extern nss_status_t	_nss_nisplus_expand_lookup	(nisplus_backend_ptr_t,
						nss_XbyY_args_t	*arg,
						const char		*key,
						const char		*val,
						const char		*table);
extern int netdb_aliases_from_nisobj(nis_object *obj,
						int nobj,
						const char *proto,
						char **alias_list,
						char **aliaspp,
						char **cnamep,
						int *count);
extern int __netdb_aliases_from_nisobj(nis_object *obj,
						int nobj,
						const char *proto,
						char **alias_list,
						char **aliaspp,
						char **cnamep,
						int *count,
						int af_type);
#else
extern nss_backend_t	*_nss_nisplus_constr();
extern nss_status_t	_nss_nisplus_destr ();
extern nss_status_t	_nss_nisplus_setent();
extern nss_status_t  	_nss_nisplus_endent();
extern nss_status_t  	_nss_nisplus_getent();
extern nss_status_t	_nss_nisplus_lookup ();
extern nss_status_t	_nss_nisplus__expand_lookup();
extern int build_aliases_from_nisobj();
#endif __STDC__

/* Lower-level interface */
extern nss_status_t	_nss_nisplus_list(const char	*name,
					int		extra_flags,
					nis_result	**r);
extern int __nis_parse_path();
extern int _thr_main(void);
extern int __nss2herrno();
extern char *inet_ntoa_r();

#endif	_NISPLUS_COMMON_H
