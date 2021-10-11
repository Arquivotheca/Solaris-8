/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DR_AP_H
#define	_DR_AP_H

#pragma ident	"@(#)dr_ap.h	1.18	99/02/26 SMI"

/*
 * dr_ap.h:	Defines the DR to AP interface through which DR queries AP
 *		devices and notifies the AP librarian of DR operations.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef AP

#include <sys/param.h>

#define	MAX_DB	10

/*
 * The following define AP structures necessary to make use of the interfaces
 * that AP exports.
 */

struct ctlr_t {
	char *name;
	int instance;
};
typedef struct ctlr_t ctlr_t;

struct db_info {
	char *path;
	uint_t major;
	uint_t minor;
	long tv_secs;
	uint_t checksum;
	bool_t default_db;
	bool_t synced;
	bool_t corrupt;
	bool_t inaccessible;
};
typedef struct db_info db_info;

struct all_db_info {
	int numdb;
	struct db_info db[MAX_DB];
};
typedef struct all_db_info all_db_info;

struct db_query {
	int errno;
	union {
		struct all_db_info info;
	} db_query_u;
};
typedef struct db_query db_query;

struct dr_arg {
	struct {
		uint_t ctlrs_len;
		ctlr_t *ctlrs_val;
	} ctlrs;
};
typedef struct dr_arg dr_arg;

typedef char *path_t;

struct ap_alias_t {
	path_t name;
	uint_t devid;
};
typedef struct ap_alias_t ap_alias_t;

struct dr_info {
	int is_alternate;
	int is_active;
	struct {
		uint_t ap_aliases_len;
		ap_alias_t *ap_aliases_val;
	} ap_aliases;
};
typedef struct dr_info dr_info;

struct dr_query {
	int errno;
	union {
		struct {
			uint_t info_len;
			struct dr_info *info_val;
		} info;
	} dr_query_u;
};
typedef struct dr_query dr_query;

struct dr_query_arg {
	struct {
		uint_t ctlrs_len;
		ctlr_t *ctlrs_val;
	} ctlrs;
	int return_aliases;
};
typedef struct dr_query_arg dr_query_arg;

/*
 * _ap_host is the name of the host domain the AP library is dealing with
 */
extern char	*_ap_host;

/*
 * The following prototypes define routines that the AP library exports for
 * the DR.
 */
extern int		ap_init_rpc(void);
extern int		apd_drain(dr_arg * da);
extern int		apd_detach_start(dr_arg * da);
extern int		apd_detach_complete(dr_arg * da);
extern int		apd_pathgroup_reset(void);
extern int		apd_resume(dr_arg * da);
extern int		apd_attach(dr_arg * da);
extern const char	*ap_strerror(int errnum, int console_and_log_msg);
extern void		ap_format_error(char *string, char *module, char *proc,
				int class, char *format);
extern void		ap_set_error_handler(void (*proc_ptr)(int class,
				char *proc, char *buffer));
extern db_query 	apd_db_query(void);
extern dr_query 	apd_dr_query(dr_query_arg * dqa);

/*
 * XDR definitions
 */
#define	xdr_dr_arg		(*(xdrproc_t)libapsyms[12].apaddr)
#define	xdr_dr_query_arg	(*(xdrproc_t)libapsyms[13].apaddr)
#define	xdr_db_query		(*(xdrproc_t)libapsyms[14].apaddr)
#define	xdr_dr_query		(*(xdrproc_t)libapsyms[15].apaddr)

/*
 * Prototypes of DR->AP related functions defined by the DR daemon
 */
struct dr_info		*do_ap_query(int, ctlr_t *);
struct all_db_info	*do_ap_db_query(void);
int 			dr_controller_names(int, ctlr_t **);

#endif AP

#ifdef	__cplusplus
}
#endif

#endif /* _DR_AP_H */
