/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)funcs.h	1.1	99/04/07 SMI"

#ifndef	_FUNCS_H
#define	_FUNCS_H

#ifdef	__cplusplus
extern "C" {
#endif

#define	CMD_PREFIX_USER	"user"

#define AUTH_SEP	","
#define PROF_SEP	","
#define ROLE_SEP	","

#define	MAX_TYPE_LENGTH	64

char *getusertype(char *cmdname);
char *check_auth(char *auths);
char *check_prof(char *profs);
char *check_role(char *roles);

int is_role(char *usertype);

#ifdef	__cplusplus
}
#endif

#endif	/* _FUNCS_H */
