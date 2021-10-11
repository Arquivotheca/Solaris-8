/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_NIS_INTERNAL_HH
#define	_FNSP_NIS_INTERNAL_HH

#pragma ident	"@(#)fnsp_nis_internal.hh	1.3	97/08/15 SMI"

#include <ndbm.h>
#include <xfn/xfn.hh>
#include "fnsp_internal_common.hh"

/* Constants defined for NIS maps */
#define	FNS_NIS_SIZE 1024
#define	FNS_MAX_ENTRY (64*1024)
#define	FNS_NIS_INDEX 256

// Class to enumerate a map
class FN_nis_map_enumerate {
	char domain[FNS_NIS_INDEX], map[FNS_NIS_INDEX];
	int initial, from_file;
	char dbmfilename[FNS_NIS_INDEX];
	DBM *dbm;
	char *inkey;
	int inkeylen;

	int next_from_map(char **outkey, int *outkeylen,
	    char **outval, int *outvallen);
	int next_from_file(char **outkey, int *outkeylen,
	    char **outval, int *outvallen);
 public:
	FN_nis_map_enumerate(const char *d, const char *m);
	virtual ~FN_nis_map_enumerate();
	int next(char **outkey, int *outkeylen,
	    char **outval, int *outvallen);
};

// Routine to bind to specific NIS domain
extern unsigned FNSP_nis_bind(const char *);
extern unsigned FNSP_nis_map_status(int);
extern unsigned FNSP_update_makefile(const char *);
extern unsigned FNSP_update_map(const char *, const char *,
    const char *, const void *, FNSP_map_operation);
extern unsigned FNSP_yp_map_lookup(char *, char *, char *,
    int, char **, int *);
extern int FNSP_is_fns_installed(const FN_ref_addr *);
extern int
FNSP_decompose_nis_index_name(const FN_string &src,
    FN_string &tabname, FN_string &indexname);

extern unsigned FNSP_compose_next_map_name(char *map);


#endif /* _FNSP_NIS_INTERNAL_HH */
