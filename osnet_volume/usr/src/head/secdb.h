/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 */

#ifndef	_SECDB_H
#define	_SECDB_H

#pragma ident	"@(#)secdb.h	1.2	99/09/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#ifndef TRUE
#define	TRUE	1
#endif

#ifndef FALSE
#define	FALSE	0
#endif

#ifndef	OK
#define	OK	TRUE
#endif

#ifndef	NOT_OK
#define	NOT_OK	FALSE
#endif

#define	DEFAULT_POLICY		"suser"

#define	KV_ACTION		"act"
#define	KV_COMMAND		"cmd"
#define	KV_JAVA_CLASS		"java_class"
#define	KV_JAVA_METHOD		"java_method"

#define	KV_ASSIGN		"="
#define	KV_DELIMITER		";"
#define	KV_EMPTY		""
#define	KV_ESCAPE		'\\'
#define	KV_ADD_KEYS		16    /* number of key value pairs to realloc */
#define	KV_SPECIAL		"=;:\\";
#define	KV_TOKEN_DELIMIT	":"
#define	KV_WILDCARD		"*"
#define	KV_WILDCHAR		'*'

#define	KV_FLAG_NONE		0x0000
#define	KV_FLAG_REQUIRED	0x0001

/*
 * return status macros for all attribute databases
 */
#define	ATTR_FOUND		0	/* Authoritative found */
#define	ATTR_NOT_FOUND		-1	/* Authoritative not found */
#define	ATTR_NO_RECOVERY	-2	/* Non-recoverable errors */


typedef struct kv_s {
	char   *key;
	char   *value;
} kv_t;					/* A key-value pair */

typedef struct kva_s {
	int	length;			/* array length */
	kv_t    *data;			/* array of key value pairs */
} kva_t;				/* Key-value array */


#ifdef	__STDC__
extern char *kva_match(kva_t *, char *);

extern char *_argv_to_csl(char **strings);
extern char **_csl_to_argv(char *csl);
extern char *_do_unescape(char *src);
extern void _free_argv(char **p_argv);
extern  _insert2kva(kva_t *, char *, char *);
extern int _kva2str(kva_t *, char *, int, char *, char *);
extern kva_t *_kva_dup(kva_t *);
extern void _kva_free(kva_t *);
extern kva_t *_new_kva(int size);
extern kva_t *_str2kva(char *, char *, char *);

#else				/* not __STDC__ */

extern char *kva_match();

extern char *_argv_to_csl();
extern char **_csl_to_argv();
extern char *_do_unescape();
extern void _free_argv();
extern  _insert2kva();
extern int _kva2str();
extern kva_t *_kva_dup();
extern void _kva_free(kva_t *);
extern kva_t *_new_kva();
extern int _str2kva();
#endif				/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _SECDB_H */
