/*
 * Copyright (c) 1996-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _DHCDATA_H
#define	_DHCDATA_H

#pragma ident	"@(#)dhcdata.h	1.16	99/10/23 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	TBL_HOSTS		0
#define	TBL_DHCPIP		1
#define	TBL_DHCPTAB		2
#define	TBL_NUM_TBLS		3

#define	TBL_MAX_COLS	16

#define	TBL_SUCCESS	0
#define	TBL_FAILURE	-1

#define	TBL_FAILCLEAN	0
#define	TBL_FAILDIRTY	1

#define	TBL_NS_DFLT		-1
#define	TBL_NS_UFS		0
#define	TBL_NS_NISPLUS		1
#define	TBL_NUM_NAMESERVICES	2
#define	TBL_NS_FILE		"/etc/default/dhcp"

/* dhcptab specific */
#define	DT_NO_VALUE		"_NULL_VALUE_"
#define	DT_DHCP_SYMBOL		's'
#define	DT_DHCP_MACRO		'm'
#define	DT_ASCII		"ASCII"
#define	DT_IP			"IP"
#define	DT_BOOL			"BOOLEAN"
#define	DT_NUM			"NUMBER"
#define	DT_OCTET		"OCTET"
#define	DT_CONTEXT_EXTEND	"Extend"
#define	DT_CONTEXT_SITE		"Site"
#define	DT_CONTEXT_VEND		"Vendor="
#define	DT_MAX_CMD_LEN		2048
#define	DT_MAX_CID_LEN		64	/* max client id size */
#define	DT_MAX_MACRO_LEN	64	/* max macro name size */
#define	DT_MAX_SYMBOL_LEN	8	/* max macro name size */

/* per network specific */
#define	F_DYNAMIC		0x00	/* Non-permanent */
#define	F_AUTOMATIC		0x01	/* Lease is permanent */
#define	F_MANUAL		0x02	/* Manually (sacred) */
#define	F_UNUSABLE		0x04	/* Address is UNUSABLE */
#define	F_BOOTP_ONLY		0x08	/* Address reserved for BOOTP */
#define	PN_MAX_COMMENT_LEN	48	/* max comment size */
#define	PN_NAME_FORMAT		"%ld_%ld_%ld_%ld"
#define	PN_NAME_TEMPLATE	"XXX_XXX_XXX_XXX"
#define	PN_MAX_NAME_SIZE	(sizeof (PN_NAME_TEMPLATE))
#define	PN_LEASE_AS_SIZE	20
#define	PN_DYNAMIC		"DYNAMIC"
#define	PN_AUTOMATIC		"PERMANENT"
#define	PN_MANUAL		"MANUAL"
#define	PN_UNUSABLE		"UNUSABLE"
#define	PN_BOOTP		"BOOTP"

/* Command return codes - used by shell scripts... */
#define	DD_SUCCESS		0
#define	DD_EEXISTS		1
#define	DD_ENOENT		2
#define	DD_WARNING		3
#define	DD_CRITICAL		4

/* Error codes */
#define	TBL_BAD_DIRECTIVE	200
#define	TBL_BAD_DOMAIN		201
#define	TBL_BAD_NS		202
#define	TBL_BAD_SYNTAX		203
#define	TBL_CHMOD_ERROR		204
#define	TBL_CHOWN_ERROR		205
#define	TBL_ENTRY_EXISTS	206
#define	TBL_IS_BUSY		207
#define	TBL_MATCH_CRITERIA_BAD	208
#define	TBL_NISPLUS_ERROR	209
#define	TBL_NO_ACCESS		210
#define	TBL_NO_ENTRY		211
#define	TBL_NO_GROUP		212
#define	TBL_NO_MEMORY		213
#define	TBL_NO_TABLE		214
#define	TBL_NO_USER		215
#define	TBL_OPEN_ERROR		216
#define	TBL_READLINK_ERROR	217
#define	TBL_READ_ERROR		218
#define	TBL_RENAME_ERROR	219
#define	TBL_STAT_ERROR		220
#define	TBL_TABLE_EXISTS	221
#define	TBL_TOO_BIG		222
#define	TBL_UNLINK_ERROR	223
#define	TBL_UNSUPPORTED_FUNC	224
#define	TBL_WRITE_ERROR		225
#define	TBL_INV_CLIENT_ID	226
#define	TBL_INV_DHCP_FLAG	227
#define	TBL_INV_HOST_IP		228
#define	TBL_INV_LEASE_EXPIRE	229
#define	TBL_INV_PACKET_MACRO	230
#define	TBL_INV_DHCPTAB_KEY	231
#define	TBL_INV_DHCPTAB_TYPE	232
#define	TBL_INV_HOSTNAME	233
#define	TBL_NO_CRED		234

/*
 * Records in the /etc/default/dhcp file are of key=value form. Comments
 * begin a line with '#', and end with newline (\n). See dhcp(4) for more
 * details. This structure is used to represent them within a program.
 */

#define	DHCP_DEFAULTS_LOCK_FILE		DEFLT "/.dhcp_defaults_lock"
#define	DHCP_DEFAULTS_TEMP		DEFLT "/.tmp_dhcp"
#define	DHCP_DEFAULTS_FILE		DEFLT "/dhcp"
#define	DHCP_DEFAULTS_ORIG		DEFLT "/dhcp_orig"
#define	DHCP_DEFAULTS_RESOURCE		"RESOURCE"
#define	DHCP_DEFAULTS_PATH		"PATH"
#define	DHCP_DEFAULTS_AUTHTOKEN		"AUTHTOKEN"

/* from enterprise */
#define	DSVC_SUCCESS			0	/* success */
#define	DSVC_NO_MEMORY			11	/* operation ran out of */
						/* memory */
#define	DSVC_BAD_DEFAULT_RESOURCE	13	/* malformed/missing RESOURCE */
						/* default */
#define	DSVC_BAD_DEFAULT_PATH		14	/* malformed/missing PATH */
						/* default */

typedef	struct tbl_stat {
	char	*name;		/* Table name */
	int	ns;		/* Name service index */
	union {
		struct {	/* UFS file data */
			mode_t	mode;		/* Permissions mode */
			uid_t   owner_uid;	/* Owner user uid */
			gid_t   owner_gid;	/* Owner group gid */
		} ufs;
		struct {	/* NIS+ table data */
			ulong_t mode;		/* Permissions mode */
			char   *owner_user;	/* Owner principal name */
			char   *owner_group;	/* Owner group name */
		} nis;
	} perm;
	time_t  atime;		/* Time of last access */
	time_t  mtime;		/* Time of last modification */
} Tbl_stat;

typedef struct row {
	char *ca[TBL_MAX_COLS];
} Row;

typedef struct tbl {
	Row **ra;
	ulong_t rows;
} Tbl;

enum dhcp_default {
	DHCP_KEY,			/* key / value form */
	DHCP_COMMENT			/* comment form */
};

typedef struct {
	enum dhcp_default	def_type;
	char			*def_key;	/* identifier */
	char			*def_value;	/* data */
} dhcp_defaults_t;
#define	def_comment		def_key		/* key doubles as comment */

extern int	list_dd(uint_t, int, char *, char *, int *, Tbl *, ...);
extern int	make_dd(uint_t, int, char *, char *, int *, char *, char *);
extern int	del_dd(uint_t, int, char *, char *, int *);
extern int	stat_dd(uint_t, int, char *, char *, int *, Tbl_stat **);
extern int	add_dd_entry(uint_t, int, char *, char *, int *, ...);
extern int	mod_dd_entry(uint_t, int, char *, char *, int *, ...);
extern int	rm_dd_entry(uint_t, int, char *, char *, int *, ...);
extern int	check_dd_access(Tbl_stat *, int *);
extern char	**dd_ls(int, char *, int *);
extern int	dd_ns(int *, char **);
extern void	free_dd(Tbl *);
extern void	free_dd_stat(Tbl_stat *);
extern int	read_dhcp_defaults(dhcp_defaults_t **);
extern int	write_dhcp_defaults(dhcp_defaults_t *, const mode_t);
extern void	free_dhcp_defaults(dhcp_defaults_t *);
extern int	delete_dhcp_defaults(void);
extern int	add_dhcp_defaults(dhcp_defaults_t **, const char *,
		    const char *);
extern int	query_dhcp_defaults(dhcp_defaults_t *, const char *, char **);
extern int	replace_dhcp_defaults(dhcp_defaults_t **, const char *,
		    const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* !_DHCDATA_H */
