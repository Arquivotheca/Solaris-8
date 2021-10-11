/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Implementation-specific header file for table library.
 */

#ifndef _DD_IMPL_H
#define	_DD_IMPL_H

#pragma ident	"@(#)dd_impl.h	1.24	99/03/22 SMI"

#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/dhcp.h>
#include <dhcdata.h>
#include <rpcsvc/nis.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TBL_MAX_ARGS	30

#define	DEFAULT_COLUMN_SEP	"\t "
#define	DEFAULT_COMMENT_SEP	"#"
#define	DEFAULT_ALIAS_SEP	" "


#define	NIS_PATH		"NIS_PATH"
#define	NIS_GROUP_ENV_STR	"NIS_GROUP"
#define	TBL_DHCP_DB		"/var/dhcp/"
#define	TBL_DFLT_PREFIX		"/etc/"
#define	TBL_INET_PREFIX		"/etc/inet/"
#define	TBL_DFLT_SUFFIX		".org_dir"
#define	TBL_NULL_FIX		""
#define	TBL_NULL_NAME		""

#define	TBL_NS_RESOURCE		"RESOURCE"
#define	TBL_NS_PATH		"PATH"
#define	TBL_NS_UFS_STRING	"files"
#define	TBL_NS_NISPLUS_STRING	"nisplus"

#define	TBL_MULT_FLAG	1	/* Entry consumes multiple NIS+ objects */

#define	COL_KEY		1
#define	COL_CASEI	2
#define	COL_UNIQUE	4

#define	COL_NOT_MATCHED	2
#define	COL_MATCHED	1
#define	EXACT_MATCH	COL_MATCHED
#define	MIX_MATCH	(COL_NOT_MATCHED | COL_MATCHED)
#define	NO_MATCH	0

#define	TBL_BASE_VALID_TYPE	0
#define	TBL_VALID_HOST_IP	(TBL_BASE_VALID_TYPE + 1)
#define	TBL_VALID_DOMAINNAME	(TBL_BASE_VALID_TYPE + 2)
#define	TBL_VALID_HOSTNAME	(TBL_BASE_VALID_TYPE + 3)
#define	TBL_VALID_CLIENT_ID	(TBL_BASE_VALID_TYPE + 4)
#define	TBL_VALID_DHCP_FLAG	(TBL_BASE_VALID_TYPE + 5)
#define	TBL_VALID_LEASE_EXPIRE	(TBL_BASE_VALID_TYPE + 6)
#define	TBL_VALID_PACKET_MACRO	(TBL_BASE_VALID_TYPE + 7)
#define	TBL_VALID_DHCPTAB_KEY	(TBL_BASE_VALID_TYPE + 8)
#define	TBL_VALID_DHCPTAB_TYPE	(TBL_BASE_VALID_TYPE + 9)
#define	TBL_MAX_VALID_TYPE	TBL_VALID_DHCPTAB_TYPE

struct col_fmt {
	uint_t flags;		/* Column flags from COL_* list */
	uint_t argno;		/* Index into args array */
	int m_argno;		/* Index into match portion of args array */
	int first_match;	/* First column to match against */
	int last_match;		/* Last column to match against */
};

struct tbl_fmt {
	char *prefix;		/* Prefix for table name; e.g. /etc */
	char *suffix;		/* Suffix for table name; e.g. .org_dir */
	char *name;		/* Name of table within this nameservice */
	int alias_col;		/* Which column aliases are in */
	int comment_col;	/* Which column has the comment */
	int (*sort_function)();	/* Function qsort will use */
	uint_t cols;		/* Number of columns */
	struct col_fmt cfmts[TBL_MAX_COLS];	/* Info for each column */
};

struct arg_valid {
	uint_t valid_function;	/* Validation function */
	ulong_t err_index;	/* Index into error message list */
};

struct tbl_trans_data {
	uint_t type;			/* Type from the TBL_ list in tbl.h */
	uint_t search_args;		/* Number of search args in call */
	uint_t args;			/* Number of data args in call */
	char *column_sep;		/* Column separator */
	char *comment_sep;		/* Comment separator */
	char *alias_sep;		/* Alias separator */
	uint_t yp_compat;		/* YP compat +/- syntax used? */
	struct arg_valid av[TBL_MAX_ARGS];	/* Validation functions */
	struct tbl_fmt fmts[TBL_NUM_NAMESERVICES];	/* Per-ns format */
};

struct tbl_make_data {
	char  *ta_type;		/* NIS+ table object type string */
	char  *ta_path;		/* NIS+ table object concat path */
	int    ta_maxcol;	/* Maximum columns for the table */
	u_char ta_sep;		/* Column separator for entry display */
	struct table_col col_info[TBL_MAX_COLS]; /* Info for each column */
};

extern struct tbl_trans_data *_dd_ttd[];

extern struct tbl_make_data *_dd_tmd[];

extern int _list_dd_ufs(char *, int *, Tbl *, struct tbl_trans_data *,
    char **);
extern int _list_dd_nisplus(char *, char *, int *, Tbl *,
    struct tbl_trans_data *, char **);
extern int _make_dd_ufs(char *, int *, struct tbl_trans_data *,
    char *, char *);
extern int _make_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *, struct tbl_make_data *, char *, char *);
extern int _del_dd_ufs(char *, int *, struct tbl_trans_data *);
extern int _del_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *);
extern int _set_nis_path(const char *);
extern int _stat_dd_ufs(char *, int *, struct tbl_trans_data *,
    Tbl_stat **);
extern int _stat_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *, Tbl_stat **);
extern int _add_dd_ufs(char *, int *, struct tbl_trans_data *, char **);
extern int _add_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *, char **);
extern int _mod_dd_ufs(char *, int *, struct tbl_trans_data *, char **);
extern int _mod_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *, char **);
extern int _rm_dd_ufs(char *, int *, struct tbl_trans_data *, char **);
extern int _rm_dd_nisplus(char *, char *, int *,
    struct tbl_trans_data *, char **);
extern int _dd_compare_ufs_col0(Row **, Row **);
extern int _dd_compare_ufs_col1(Row **, Row **);
extern int _dd_compare_ufs_dhcptab(Row **, Row **);
extern int _dd_compare_nisplus_col0(nis_object *, nis_object *);
extern int _dd_compare_nisplus_col0_ci(nis_object *, nis_object *);
extern int _dd_compare_nisplus_col1_ci(nis_object *, nis_object *);
extern int _dd_compare_nisplus_col2(nis_object *, nis_object *);
extern int _dd_compare_nisplus_col2_ci(nis_object *, nis_object *);
extern int _dd_compare_nisplus_aliased(nis_object *, nis_object *);
extern int _dd_compare_nisplus_services(nis_object *, nis_object *);
extern int _dd_compare_nisplus_dhcptab(nis_object *, nis_object *);
extern Row *_dd_new_row(void);
extern void _dd_free_row(Row *);
extern int _dd_append_row(Tbl *, Row *);
extern int _dd_set_col_val(Row *, int, char *, char *);
extern int _dd_validate(uint_t, const char *);
extern int _dd_destroy_hosts_context(const char *, const char *);
extern int _dd_update_hosts_context(const char *, const char *);
extern char **_dd_ls_ufs(int *, const char *);
extern char **_dd_ls_nisplus(int *, const char *);
extern char *_dd_tempfile(const char *);
extern int _dd_lock_db(char *, int, int *);
extern int _dd_unlock_db(int *);

#ifdef	__cplusplus
}
#endif

#endif	/* !_DD_IMPL_H */
