/*
 * Copyright (c) 1992 - 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_SPI_H
#define	_XFN_SPI_H

#pragma ident	"@(#)fn_spi.h	1.5	96/03/31 SMI"

#include <xfn/xfn.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SPI reserves the range 128-256 */

#define	FN_E_SPI_CONTINUE 130
#define	FN_E_SPI_FOLLOW_LINK 131

typedef struct _FN_ctx_svc_t FN_ctx_svc_t;
/* SPI lookup flags */
/*
 * If the following flag is set, it means the lookup operation should not
 * follow the link if the link is bound to the terminal atomic name.
 * If this flag is not set, the terminal link will be followed, which means
 * the implementation has the choice of
 * 1. following the link itself, and returning the reference of the
 * final result
 * 2. return the link and set the status code to FN_E_SPI_FOLLOW_LINK
 */
#define	FN_SPI_LEAVE_TERMINAL_LINK  1

/* ************* prototypes exported by a context implementation ******** */
typedef FN_ctx_svc_t *(*FN_ctx_svc_from_ref_addr_func)(const FN_ref_addr_t *,
    const FN_ref_t *, unsigned int, FN_status_t *);
typedef FN_ctx_svc_t *(*FN_ctx_svc_from_ref_func)(const FN_ref_t *,
    unsigned int, FN_status_t *);
typedef FN_ctx_svc_t *(*FN_ctx_svc_from_initial_func)(unsigned int,
    FN_status_t *);

/* ************* generic interface types and values ****************** */
#define	 FN_CTX_SVC_FUNC_ARRAY_SIZE 21
enum {
	FN_CTX_FUNC_EQUIVALENT_NAME,
	FN_CTX_FUNC_GET_REF,
	FN_CTX_FUNC_LOOKUP,
	FN_CTX_FUNC_LIST_NAMES,
	FN_CTX_FUNC_LIST_BINDINGS,
	FN_CTX_FUNC_BIND,
	FN_CTX_FUNC_UNBIND,
	FN_CTX_FUNC_CREATE_SUBCONTEXT,
	FN_CTX_FUNC_DESTROY_SUBCONTEXT,
	FN_CTX_FUNC_RENAME,
	FN_CTX_FUNC_GET_SYNTAX_ATTRS,
	FN_CTX_FUNC_ATTR_GET,
	FN_CTX_FUNC_ATTR_MODIFY,
	FN_CTX_FUNC_ATTR_GET_VALUES,
	FN_CTX_FUNC_ATTR_GET_IDS,
	FN_CTX_FUNC_ATTR_MULTI_GET,
	FN_CTX_FUNC_ATTR_MULTI_MODIFY,
	FN_CTX_FUNC_ATTR_BIND,
	FN_CTX_FUNC_ATTR_CREATE,
	FN_CTX_FUNC_ATTR_SEARCH,
	FN_CTX_FUNC_ATTR_EXT_SEARCH
};
typedef void (*FN_ctx_svc_func_t)();
typedef void *FN_ctx_svc_data_t;
typedef FN_ctx_svc_func_t FN_ctx_svc_func_array_t[FN_CTX_SVC_FUNC_ARRAY_SIZE];

/* *************** Routines exported by Framework ******************* */
/* gets FN_ctx_t *handle from an FN_ctx_svc_t object */
extern FN_ctx_t *
fn_ctx_handle_from_fn_ctx_svc(const FN_ctx_svc_t *sp);
/* Return context-specific information */
extern FN_ctx_svc_data_t *
fn_ctx_svc_get_ctx_data(FN_ctx_svc_t *sp);
/* Set context-specific information */
extern int
fn_ctx_svc_set_ctx_data(FN_ctx_svc_t *sp, FN_ctx_svc_data_t *data);

/* ************* Interface for Partial Composite SPI *************** */
typedef struct _FN_status_psvc_t FN_status_psvc_t;
typedef FN_ref_t *(*FN_ctx_psvc_lookup_func_t)(FN_ctx_svc_t *ctx,
    FN_composite_name_t *name,
    unsigned int lookup_flag,
    FN_status_psvc_t *s);
/* %%% and so on for other p_ interfaces */
/* Registration function for partial composite SPI */
extern FN_ctx_svc_t *
fn_ctx_psvc_handle_create(FN_ctx_svc_func_array_t *p_funcs,
    FN_ctx_svc_data_t *dt,
    FN_status_t *status);

/* ***************** Interface for component SPI ******************* */
enum {
	FN_SPI_SINGLE_COMPONENT = 1,
	FN_SPI_MULTIPLE_COMPONENT
};
enum {
	FN_SPI_STATIC_BOUNDARY = 1,
	FN_SPI_DYNAMIC_BOUNDARY
};
enum {
	FN_SPI_STRONG_SEPARATION = 1,
	FN_SPI_WEAK_SEPARATION
};
typedef struct _FN_status_csvc_t FN_status_csvc_t;
typedef FN_string_t *(*FN_ctx_psvc_parser_func_t)(const FN_composite_name_t *,
    FN_composite_name_t **rest,
    FN_status_psvc_t *status);
typedef FN_ref_t *(*FN_ctx_csvc_lookup_func_t)(FN_ctx_svc_t *ctx,
    FN_string_t *name,
    unsigned int lookup_flag,
    FN_status_csvc_t *s);
/* %%% and so on for other c_ interfaces */
typedef struct _FN_status_cnsvc_t FN_status_cnsvc_t;
typedef FN_ref_t *(*FN_ctx_cnsvc_lookup_func_t)(FN_ctx_svc_t *ctx,
    FN_composite_name_t *name,
    unsigned int lookup_flag,
    FN_status_cnsvc_t *s);
/* %%% and so on for other cn_ interfaces */
/* Registration function for component SPI */
extern FN_ctx_svc_t *
fn_ctx_csvc_handle_create(unsigned int component_type,
    unsigned int boundary_type,
    FN_ctx_svc_func_array_t *c_or_cn_funcs,
    FN_ctx_svc_func_array_t *c_or_cn_nns_funcs,
    FN_ctx_psvc_parser_func_t *p_parser,
    FN_ctx_svc_data_t *dt,
    FN_status_t *status);

/* ********************* Interface for Atomic SPI ******************** */
typedef struct _FN_status_asvc_t FN_status_asvc_t;
typedef FN_string_t *(*FN_ctx_csvc_parser_func_t)(const FN_string_t *,
    FN_string_t **rest,
    FN_status_csvc_t *status);
typedef FN_ref_t *(*FN_ctx_asvc_lookup_func_t)(FN_ctx_svc_t *ctx,
    FN_string_t *name,
    unsigned int lookup_flag,
    FN_status_asvc_t *s);
/* %%% and so on for other a_ interfaces */
/* Registration function for atomic SPI */
extern FN_ctx_svc_t *
fn_ctx_asvc_handle_create(unsigned int separation_type,
    unsigned int boundary_type,
    FN_ctx_svc_func_array_t *a_funcs,
    FN_ctx_svc_func_array_t *a_nns_funcs,
    FN_ctx_psvc_parser_func_t *p_parser,
    FN_ctx_csvc_parser_func_t *c_parser,
    FN_ctx_svc_data_t *dt,
    FN_status_t *status);

#ifdef __cplusplus
}
#endif

#endif /* _XFN_SPI_H */
