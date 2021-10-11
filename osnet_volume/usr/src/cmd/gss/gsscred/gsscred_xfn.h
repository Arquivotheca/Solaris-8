/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GSSCRED_XFN_H
#define	_GSSCRED_XFN_H

#pragma ident	"@(#)gsscred_xfn.h	1.5	97/10/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Structure to hold GSS credentials for each entry */
typedef struct GssCredEntry_t {
	char *principal_name;
	int  unix_uid;
	char *comment;
	struct GssCredEntry_t *next;
} GssCredEntry;


extern int
xfn_addGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *principal, const char *unix_uid,
	const char *comment, char **errDetails);

extern GssCredEntry *
xfn_getGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *principal_name, const char *unix_uid,
	char **errDetails);

extern int
xfn_deleteGssCredEntry(int xfnBackEnd, const char *mechanism,
	const char *uid, const char *principalName,
	char **errDetails);

extern int
xfn_getFirstGssCredEntry(int xfnBackEnd, const char *mechanism,
	void **searchHandle, GssCredEntry *entry, char **errDetails);

extern int
xfn_getNextGssCredEntry(void *searchHandle, GssCredEntry *entry);

extern int
xfn_deleteGssCredSearchHandle(void *searchHandle);

#ifdef	__cplusplus
}
#endif

#endif	/* _GSSCRED_XFN_H */
