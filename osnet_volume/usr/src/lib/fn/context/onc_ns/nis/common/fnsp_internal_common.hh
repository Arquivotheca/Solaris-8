/*
 * Copyright (c) 1995, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _FNSP_INTERNAL_COMMON_HH
#define	_FNSP_INTERNAL_COMMON_HH

#pragma ident	"@(#)fnsp_internal_common.hh	1.7	97/08/15 SMI"

#include <xfn/xfn.hh>
#include <xfn/fn_xdr.hh>
#include <xfn/FN_nameset.hh>


enum FNSP_map_operation {
	FNSP_map_insert,  // Exculsive
	FNSP_map_delete,
	FNSP_map_modify,
	FNSP_map_store
};

enum FNSP_binding_type {
	FNSP_bound_reference = 0,
	FNSP_child_context = 1,
	FNSP_hu_reference = 2,
	FNSP_hu_context = 3
};

#define	FNSP_NIS_REFERENCE	"b"
#define	FNSP_NIS_CONTEXT	"c"
#define	FNSP_NIS_HU_REFERENCE	"d"
#define	FNSP_NIS_HU_CONTEXT	"e"

#define	FNSP_ORG_MAP		"fns_org.ctx"
#define	FNSP_USER_CTX_MAP	"fns_user.ctx"
#define	FNSP_USER_ATTR_MAP	"fns_user.attr"
#define	FNSP_HOST_CTX_MAP	"fns_host.ctx"
#define	FNSP_HOST_ATTR_MAP	"fns_host.attr"
#define	FNSP_THISORGUNIT_ATTR_MAP	"fns_org.attr"

#define	FNSP_ATTR_SUFFIX	"_a"
#define	FNSP_BIND_SUFFIX	"_b"
#define	FNSP_SUBCTX_SUFFIX	"_c"
#define	FNSP_INTERNAL_NAME_SEP	"_"
#define	FNSP_SELF_STR	"_FNS_self"

#define FNSP_HUGE_SUBCONTEXT	"TOO_BIG_SUBCONTEXT"
#define FNSP_HUGE_SUBCONTEXT_SIZE	(5*1024)

extern char *strparse(char *, const char *, char **);

extern int FNSP_match_map_index(const char *, const char *);

extern char *FNSP_nis_sub_context_serialize(const FN_nameset *,
    unsigned &);

extern FN_nameset *FNSP_nis_sub_context_deserialize(char *,
    unsigned &);

extern char *FNSP_nis_binding_serialize(const FN_ref &,
    FNSP_binding_type, unsigned &);

extern FN_ref *FNSP_nis_binding_deserialize(char *, int,
    FNSP_binding_type &, unsigned &);

extern unsigned FNSP_nis_split_internal_name(const FN_string &,
    FN_string **, FN_string **);

extern int FNSP_decompose_nis_index_name(const FN_string &,
    FN_string &, FN_string &);

extern char *FNSP_nis_attrset_serialize(const FN_attrset &, unsigned &);

extern FN_attrset *FNSP_nis_attrset_deserialize(char *, int,
    unsigned &);

extern void FNSP_construct_local_name(unsigned context_type, char *name);

extern unsigned FNSP_get_first_index_data(FNSP_map_operation op,
    const char *index, const void *data,
    char *new_index, void *new_data, size_t &length,
    char *next_index);

extern unsigned FNSP_get_next_index_data(FNSP_map_operation op,
    const char * /* index */, const void *data,
    char *new_index, void *new_data, size_t &length,
    char *next_index);

extern unsigned
FNSP_get_first_lookup_index(const char *map_index,
    char *new_index, char *next_index);

extern unsigned
FNSP_get_next_lookup_index(char *new_index, char *next_index,
    char *mapentry, int maplen);

extern void
FNSP_legalize_name(char *name);

extern void
FNSP_normalize_name(char *name);

extern char *
FNSP_check_if_subcontext(const char *parent_index, const char *fullname);

#endif /* _FNSP_INTERNAL_COMMON_HH */
