/*
 * ns_fnutils.h
 *
 * Copyright (c) 1995 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _NS_FNUTILS_H
#define	_NS_FNUTILS_H

#pragma ident	"@(#)ns_fnutils.h	1.8	96/04/26 SMI"

#include <rpc/rpc.h>
#include <xfn/xfn.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Enable compilation for either XFN1 (pre-2.6) or XFN2 environment.
 */
#ifdef	XFN1ENV
#define	XFN1(x) /* cstyle */, x
#define	XFN2(x)
#define	_fn_ctx_handle_from_initial_with_uid(uid, auth, status) \
	    fn_ctx_handle_from_initial(status)
#else
#define	XFN1(x)
#define	XFN2(x) x,
#endif

/*
 * FNS file system reference and address types.  Each (char *) array is indexed
 * using the corresponding enumeration.
 */
extern const char *reftypes[];

typedef enum {
	REF_FN_FS,
	NUM_REFTYPES	/* Not a ref type, but rather a count of them */
} reftype_t;

extern const char *addrtypes[];

typedef enum {
	ADDR_MOUNT,
	ADDR_HOST,
	ADDR_USER,
	ADDR_USER_NISPLUS,
	NUM_ADDRTYPES	/* Not an addr type, but rather a count of them */
} addrtype_t;


/*
 * Initialization for FNS.  Return 0 on success.
 */
extern int
init_fn(void);

/*
 * Allocate a new composite name.  On error, log an error message and
 * return NULL.
 */
extern FN_composite_name_t *
new_cname(const char *);

/*
 * Return the type of a reference, or NUM_REFTYPES if the type is unknown.
 */
extern reftype_t
reftype(const FN_ref_t *);

/*
 * Return the type of an address, or NUM_ADDRTYPES if the type is unknown.
 */
extern addrtype_t
addrtype(const FN_ref_addr_t *);

/*
 * Determine whether two identifiers match.
 */
extern bool_t
ident_equal(const FN_identifier_t *, const FN_identifier_t *);

/*
 * Determine whether an identifier and a string match.
 */
extern bool_t
ident_str_equal(const FN_identifier_t *, const char *);

/*
 * Syslog an error message and status info (with detail level DETAIL)
 * if "verbose" is set.
 */
#define	DETAIL	0
extern void
logstat(const FN_status_t *, const char *msg1, const char *msg2);

/*
 * Determine whether an error is potentially transient.
 */
extern bool_t
transient(const FN_status_t *);

/*
 * Log a memory allocation failure if "verbose" is true.
 */
extern void
log_mem_failure(void);

extern FN_ctx_t *
_fn_ctx_handle_from_initial_with_uid(uid_t, unsigned int, FN_status_t *);

extern FN_string_t		*empty_string;
extern FN_composite_name_t	*empty_cname;
extern FN_composite_name_t	*slash_cname;	/* "/" */


#ifdef	__cplusplus
}
#endif

#endif	/* _NS_FNUTILS_H */
