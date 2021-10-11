/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_AUTH_ATTR_H
#define	_AUTH_ATTR_H

#pragma ident	"@(#)auth_attr.h	1.1	99/06/03 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <secdb.h>

/*
 * Some macros used internally by the nsswitch code
 */
#define	AUTH_MMAPLEN			1024
#define	AUTH_POLICY			"/etc/security/policy.conf"
#define	DEF_AUTH			"AUTHS_GRANTED="
#define	AUTHATTR_FILENAME		"/etc/security/auth_attr"
#define	AUTHATTR_DB_NAME		"auth_attr.org_dir"
#define	AUTHATTR_DB_NCOL		6	/* total columns */
#define	AUTHATTR_DB_NKEYCOL		1	/* total searchable columns */
#define	AUTHATTR_DB_TBLT		"auth_attr_tbl"
#define	AUTHATTR_NAME_DEFAULT_KW	"nobody"

#define	AUTHATTR_COL0_KW		"name"
#define	AUTHATTR_COL1_KW		"res1"
#define	AUTHATTR_COL2_KW		"res2"
#define	AUTHATTR_COL3_KW		"short_desc"
#define	AUTHATTR_COL4_KW		"long_desc"
#define	AUTHATTR_COL5_KW		"attr"

/*
 * indices of searchable columns
 */
#define	AUTHATTR_KEYCOL0		0	/* name */


/*
 * Key words used in the auth_attr database
 */
#define	AUTHATTR_HELP_KW		"help"

/*
 * Nsswitch internal representation of authorization attributes.
 */
typedef struct authstr_s {
	char   *name;		/* authorization name */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	char   *short_desc;	/* short description */
	char   *long_desc;	/* long description */
	char   *attr;		/* string of key-value pair attributes */
} authstr_t;

/*
 * API representation of authorization attributes.
 */
typedef struct authattr_s {
	char   *name;		/* authorization name */
	char   *res1;		/* reserved for future use */
	char   *res2;		/* reserved for future use */
	char   *short_desc;	/* short description */
	char   *long_desc;	/* long description */
	kva_t  *attr;		/* array of key-value pair attributes */
} authattr_t;

#ifdef __STDC__
extern authattr_t *getauthnam(const char *);
extern authattr_t *getauthattr(void);
extern void setauthattr(void);
extern void endauthattr(void);
extern void free_authattr(authattr_t *);
extern int chkauthattr(const char *, const char *);

#else				/* not __STDC__ */

extern authattr_t *getauthnam();
extern authattr_t *getauthattr();
extern void setauthattr();
extern void endauthattr();
extern void free_authattr();
extern int chkauthattr();
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _AUTH_ATTR_H */
