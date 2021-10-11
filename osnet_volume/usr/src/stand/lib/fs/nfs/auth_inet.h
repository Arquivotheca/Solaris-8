/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AUTH_INET_H
#define	_AUTH_INET_H

#pragma ident	"@(#)auth_inet.h	1.1	99/02/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern struct opaque_auth _null_auth;
extern AUTH *authnone_create(void);
extern AUTH *authunix_create(char *, uid_t, gid_t, int, gid_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _AUTH_INET_H */
