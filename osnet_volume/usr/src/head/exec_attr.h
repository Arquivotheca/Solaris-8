/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_EXEC_ATTR_H
#define	_EXEC_ATTR_H

#pragma ident	"@(#)exec_attr.h	1.1	99/06/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#include <sys/types.h>
#include <secdb.h>


#define	EXECATTR_FILENAME		"/etc/security/exec_attr"
#define	EXECATTR_DB_NAME		"exec_attr.org_dir"
#define	EXECATTR_DB_NCOL		7	/* total columns */
#define	EXECATTR_DB_NKEYCOL		2	/* total searchable columns */
#define	EXECATTR_DB_TBLT		"exec_attr_tbl"
#define	EXECATTR_NAME_DEFAULT_KW	"nobody"

#define	EXECATTR_COL0_KW		"name"
#define	EXECATTR_COL1_KW		"policy"
#define	EXECATTR_COL2_KW		"type"
#define	EXECATTR_COL3_KW		"res1"
#define	EXECATTR_COL4_KW		"res2"
#define	EXECATTR_COL5_KW		"id"
#define	EXECATTR_COL6_KW		"attr"

/*
 * indices of searchable columns
 */
#define	EXECATTR_KEYCOL0		0	/* name */
#define	EXECATTR_KEYCOL1		5	/* id */


/*
 * Some macros used internally by the nsswitch code
 */

#define	GET_NEXT	0	/* get next exec_attr in list */
#define	GET_NO_MORE	1	/* get no more exec_attrs from list */
#define	GET_ONE		0	/* get only one exec_attr from list */
#define	GET_ALL		1	/* get all matching exec_attrs in list */


/*
 * Key words used in the exec_attr database
 */
#define	EXECATTR_EUID_KW	"euid"
#define	EXECATTR_EGID_KW	"egid"
#define	EXECATTR_UID_KW		"uid"
#define	EXECATTR_GID_KW		"gid"

/*
 * Nsswitch representation of execution attributes.
 */
typedef struct execstr_s {
	char   *name;		/* profile name */
	char   *policy;		/* suser/rbac/tsol */
	char   *type;		/* cmd/act */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	char   *id;		/* unique ID */
	char   *attr;		/* string of key-value pair attributes */
	struct execstr_s *next;	/* pointer to next entry */
} execstr_t;

typedef struct execattr_s {
	char   *name;		/* profile name */
	char   *policy;		/* suser/rbac/tsol */
	char   *type;		/* cmd/act */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	char   *id;		/* unique ID */
	kva_t  *attr;		/* array of key-value pair attributes */
	struct execattr_s *next;	/* pointer to next entry */
} execattr_t;

typedef struct __private_execattr {
	const char *name;
	const char *type;
	const char *id;
	const char *policy;
	int search_flag;
	execstr_t *head_exec;
	execstr_t *prev_exec;
} _priv_execattr;		/* Un-supported. For Sun internal use only */


#ifdef    __STDC__
extern execattr_t *getexecattr(void);
extern execattr_t *getexecuser(const char *, const char *, const char *, int);
extern execattr_t *getexecprof(const char *, const char *, const char *, int);
extern execattr_t *match_execattr(execattr_t *, const char *, const char *, \
	const char *);
extern void free_execattr(execattr_t *);
extern void setexecattr(void);
extern void endexecattr(void);

#else				/* not __STDC__ */

extern execattr_t *getexecattr();
extern execattr_t *getexecuser();
extern execattr_t *getexecprof();
extern execattr_t *match_execattr();
extern void setexecattr();
extern void endexecattr();
extern void free_execattr();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _EXEC_ATTR_H */
