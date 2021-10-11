/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SH_POLICY_H
#define	_SH_POLICY_H

#pragma ident	"@(#)sh_policy.h	1.1	99/05/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <exec_attr.h>

#define	PFEXEC		"/usr/bin/pfexec"
#define	ALL_PROFILE	"All"

#define	ERR_PASSWD	"can't get passwd entry"
#define	ERR_MEM		"can't allocate memory"
#define	ERR_CWD		"can't get current working directory"
#define	ERR_PATH	"resolved pathname too large"
#define	ERR_PROFILE	"not in profile"
#define	ERR_REALPATH	"can't get real path"

#define	NOATTRS	0	/* command in profile but w'out attributes */

#define	SECPOLICY_WARN	1
#define	SECPOLICY_ERROR	2

/*
 * Shell Policy Interface Functions
 */
extern void secpolicy_init(void);
extern int secpolicy_pfexec(const char *, char **, const char **);
extern void secpolicy_print(int, const char *);


#ifdef	__cplusplus
}
#endif

#endif	/* _SH_POLICY_H */
