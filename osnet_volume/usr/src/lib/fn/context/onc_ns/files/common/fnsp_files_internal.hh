/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_FILES_INTERNAL_HH
#define	_FNSP_FILES_INTERNAL_HH

#pragma ident	"@(#)fnsp_files_internal.hh	1.5	97/10/24 SMI"

#include <fnsp_internal_common.hh>

// Constants defined for NIS maps
#define	FNS_FILES_SIZE	1024
#define	FNS_FILES_INDEX	256
#define	FNS_MAX_ENTRY	(64*1024)

#define	FNSP_FILES_MAP_DIR	"/var/fn"
#define	FNSP_USER_MAP_PRE	"fns_user"
#define	FNSP_MAP_SUFFIX	".ctx"

// Routine to bind to specific NIS domain
extern unsigned FNSP_files_update_map(const char *,
    const char *, const void *, FNSP_map_operation);
extern unsigned FNSP_files_lookup(char *, char *,
    int, char **, int *);
extern int FNSP_files_is_fns_installed(const FN_ref_addr* = 0);
extern int FNSP_change_user_ownership(const char *username);

extern unsigned FNSP_files_lookup_host_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *file_name);

extern unsigned FNSP_files_lookup_user_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *file_name);

extern unsigned FNSP_files_lookup_service_like_entry(const char *map_index,
    char **mapentry, int *maplen, const char *file_name);

#endif	/* _FNSP_FILES_INTERNAL_HH */
