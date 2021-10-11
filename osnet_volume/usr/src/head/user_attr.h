/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_USER_ATTR_H
#define	_USER_ATTR_H

#pragma ident	"@(#)user_attr.h	1.1	99/06/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/types.h>
#include <secdb.h>

/*
 * Some macros used internally by the nsswitch code
 */
#define	USERATTR_FILENAME		"/etc/user_attr"
#define	USERATTR_DB_NAME		"user_attr.org_dir"
#define	USERATTR_DB_NCOL		5	/* total columns */
#define	USERATTR_DB_NKEYCOL		2	/* total searchable columns */
#define	USERATTR_DB_TBL			"user_attr_tbl"
#define	USERATTR_NAME_DEFAULT_KW	"nobody"

#define	USERATTR_COL0_KW		"name"
#define	USERATTR_COL1_KW		"qualifier"
#define	USERATTR_COL2_KW		"res1"
#define	USERATTR_COL3_KW		"res2"
#define	USERATTR_COL4_KW		"attr"

/*
 * indices of searchable columns
 */
#define	USERATTR_KEYCOL0		0	/* name */
#define	USERATTR_KEYCOL1		1	/* qualifier */

/*
 * Key words used in the user_attr database
 */
#define	USERATTR_LOCK_KW		"lock"
#define	USERATTR_LOCK_LOCKED_KW		"locked"
#define	USERATTR_LOCK_OPEN_KW		"open"
#define	USERATTR_LOCK_FIXED_KW		"fixed"
#define	USERATTR_GEN_KW			"gen"
#define	USERATTR_GEN_AUTOMATIC_KW	"automatic"
#define	USERATTR_GEN_MANUAL_KW		"manual"
#define	USERATTR_GEN_SYSDEF_KW		"sysdef"
#define	USERATTR_PROFILES_KW		"profiles"
#define	USERATTR_PROFILES_NONE_KW	"none"
#define	USERATTR_ROLES_KW		"roles"
#define	USERATTR_ROLES_NONE_KW		"none"
#define	USERATTR_IDLETIME_KW		"idletime"
#define	USERATTR_IDLECMD_KW		"idlecmd"
#define	USERATTR_IDLECMD_LOCK_KW	"lock"
#define	USERATTR_IDLECMD_LOGOUT_KW	"logout"
#define	USERATTR_TYPE_KW		"type"
#define	USERATTR_TYPE_NORMAL_KW		"normal"
#define	USERATTR_TYPE_ADMIN_KW		"admin"
#define	USERATTR_TYPE_NONADMIN_KW	"role"
#define	USERATTR_AUTHS_KW		"auths"

/*
 * Nsswitch representation of user attributes.
 */
typedef struct userstr_s {
	char   *name;		/* user name */
	char   *qualifier;	/* reserved for future use */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	char   *attr;		/* string of key-value pair attributes */
} userstr_t;

/*
 * API representation of user attributes.
 */
typedef struct userattr_s {
	char   *name;		/* user name */
	char   *qualifier;	/* reserved for future use */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	kva_t  *attr;		/* array of key-value pair attributes */
} userattr_t;

#ifdef	__STDC__
extern userattr_t *getusernam(const char *);
extern userattr_t *getuseruid(uid_t uid);
extern userattr_t *getuserattr(void);
extern void setuserattr(void);
extern void enduserattr(void);
extern void free_userattr(userattr_t *);

#else				/* not __STDC__ */

extern userattr_t *getusernam();
extern userattr_t *getuseruid();
extern userattr_t *getuserattr();
extern void setuserattr();
extern void enduserattr();
extern void free_userattr();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _USER_ATTR_H */
