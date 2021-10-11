/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_RULES_H
#define	_RULES_H

#pragma ident "@(#)rules.h	1.2	96/01/16 SMI"

#define	MAX_RULE_SZ	MAXPATHLEN+80

#define	BASE	"BASE"
#define	IGNORE	"IGNORE"
#define	LIST	"LIST"
#define	VERSION "PACKINGRULES"

#define	VERMAJOR	1
#define	VERMINOR	1

#define	TMPRULES	".packingrules"
#define	CMDCHAR	'!'

struct item {
	int i_flag;
	char *i_str;
	struct item *i_next;
};

#ifdef MAIN
#define	EXTERN
#else
#define	EXTERN	extern
#endif

EXTERN char *basedir;
EXTERN struct item list_hd;
EXTERN struct item gign_hd;
EXTERN struct item lign_hd;
EXTERN struct item *last_list;
EXTERN struct item *last_gign;
EXTERN struct item *last_lign;
EXTERN int def_gign_flags;
EXTERN int def_lign_flags;
EXTERN int def_list_flags;
EXTERN int bang_list_flags;

EXTERN int global_flags;

#undef EXTERN

/*
 * Define values for item struct flags
 */
#define	LF_NULL			0
#define	LF_STRIP_DOTSLASH	1
#define	LF_REGEX		2
#define	LF_SYMLINK		4

#define	WILDCARD(x, y) (x = strpbrk(y, "*?.^[]{}$"))

#endif /* _RULES_H */
