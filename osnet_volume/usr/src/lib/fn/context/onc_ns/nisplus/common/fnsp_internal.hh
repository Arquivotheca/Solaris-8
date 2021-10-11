/*
 * Copyright (c) 1992 - 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_INTERNAL_HH
#define	_FNSP_INTERNAL_HH

#pragma ident	"@(#)fnsp_internal.hh	1.11	96/03/31 SMI"

#include <rpc/rpc.h>		// for netobj
#include <rpcsvc/nis.h>		// for nis_object

#include <FNSP_Address.hh>
#include <xfn/FN_nameset.hh>
#include <xfn/FN_bindingset.hh>

#define	FNSP_nisflags	USE_DGRAM
#define	FNSP_SELF_NAME  "_FNS_self_"

/* column names */
#define	FNSP_ATTRID_COL_LABEL	"attridentifier"
#define	FNSP_ATTRVAL_COL_LABEL	"attrvalue"
#define	FNSP_NAME_COL_LABEL	"atomicname"
#define	FNSP_CONTEXTNAME	"contextname"
#define	FNSP_NIS_DEFAULT_PATH	""
#define	FNSP_NIS_DEFAULT_TABTYPE "H"
#define	FNSP_NIS_DEFAULT_SEP	' '

// interface to NIS+ routines
extern const FN_identifier &
FNSP_nisplus_address_type_name(void);

// interface to directory routines
extern
unsigned FNSP_create_directory(const FN_string &, unsigned int access_flags);

extern unsigned
FNSP_context_exists(const FNSP_Address&);

extern FN_nameset*
FNSP_list_names(const FNSP_Address& parent,
    unsigned & status, int children_only = 0);

extern FN_bindingset*
FNSP_list_bindings(const FNSP_Address& parent,
    unsigned &status);

extern FN_nameset *
FNSP_list_orgnames(const FN_string &org_name, unsigned int access_flags,
		    unsigned &status);

extern FN_bindingset *
FNSP_list_orgbindings(const FN_string &org_name, unsigned int access_flags,
		    unsigned &status);

extern FN_ref *
FNSP_resolve_orgname(const FN_string &directory,
    const FN_string &target,
    unsigned int access_flags,
    unsigned &status,
    FN_status &stat,
    int &stat_set);


extern FN_string *
FNSP_orgname_of(const FN_string &internal_name, unsigned &status,
    int org = 0);

extern FN_string *
FNSP_compose_ctx_tablename(const FN_string &short_tabname,
    const FN_string &domain_name);

extern char *
FNSP_read_first(const char *tab_name, netobj &iter_pos,
    unsigned int &status, FN_ref **ref = 0);

extern char *
FNSP_read_next(const char *tab_name, netobj &iter_pos,
    unsigned int &status, FN_ref **ref = 0);


// change ownership of context (and all inclusive bindings)
// If 'ref' is 'host' or 'user' nns type, change all subcontexts too
extern int
FNSP_change_context_ownership(const FN_ref &ref,
    const FN_string &owner);

// change ownership associated with binding associated with given name
extern int
FNSP_change_binding_ownership(const FN_ref &ref,
    const FN_string &atomic_name,
    const FN_string &owner);

// used amongst fnsp_*
extern unsigned
FNSP_get_binding_entries(const FN_string &tabname,
    unsigned int access_flags,
    int (*add_func)(char *, nis_object*, void *),
    void *add_params,
    const FN_string *cname = 0);

extern unsigned
FNSP_map_result(nis_result *res, char *msg = NULL);

#endif	/* _FNSP_INTERNAL_HH */
