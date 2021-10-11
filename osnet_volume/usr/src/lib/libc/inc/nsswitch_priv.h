/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * nsswitch_priv.h
 *
 * The original switch low level interface to switch was defined in
 * /usr/include/nsswitch.h.  Although it was marked "Project Private",
 * it was none-the-less "exposed" and used by several apps.  This
 * file, nsswitch_priv.h will *not* go in /usr/include to limit "exposure"
 * and contains new versions of the switch low level interface.
 *
 * This is a Project Private interface.  It may change in future releases.
 *	==== ^^^^^^^^^^^^^^^ ?
 */

#ifndef _NSSWITCH_PRIV_H
#define	_NSSWITCH_PRIV_H

#pragma ident	"@(#)nsswitch_priv.h	1.2	99/09/20 SMI"

#include <nsswitch.h>

#ifdef	__cplusplus
extern "C" {
#endif


#define	__NSW_STD_ERRS_V1	5 /* V1 number of reserved errors */

/*
 * from /usr/include/nsswitch.h
 *
 * #define	__NSW_SUCCESS	0	 found the required data
 * #define	__NSW_NOTFOUND	1 the naming service returned lookup failure
 * #define	__NSW_UNAVAIL	2	 could not call the naming service
 * #define	__NSW_TRYAGAIN	3	 bind error to suggest a retry
 */

/* nis server in dns forwarding mode and dns server non-response */
#define	__NSW_NISSERVDNS_TRYAGAIN	4

/* #define	__NSW_CONTINUE	0 the action is to continue to next service */
/* #define	__NSW_RETURN	1 the action is to return to the user */

/* the action is to retry the request until we get an answer */
#define	__NSW_TRYAGAIN_FOREVER	2
/* the action is to retry the request N times maximum */
#define	__NSW_TRYAGAIN_NTIMES	3

/* is this action available to all switch errs? */
#define	__NSW_COMMON_ACTION(act)\
	(((act) == __NSW_CONTINUE) || ((act) == __NSW_RETURN))

#define	__NSW_SUCCESS_ACTION(act)	__NSW_COMMON_ACTION(act)
#define	__NSW_NOTFOUND_ACTION(act)	__NSW_COMMON_ACTION(act)
#define	__NSW_UNAVAIL_ACTION(act)	__NSW_COMMON_ACTION(act)
#define	__NSW_TRYAGAIN_ACTION(act) \
	(__NSW_COMMON_ACTION(act) || \
	    ((act) == __NSW_TRYAGAIN_FOREVER) || \
	    ((act) == __NSW_TRYAGAIN_NTIMES))

#define	__NSW_STR_FOREVER	"forever"

#ifdef __NSS_PRIVATE_INTERFACE
struct __nsw_lookup_v1 {
	char *service_name;
	action_t actions[__NSW_STD_ERRS_V1];
	int max_retries;  /* for TRYAGAIN=N */
	struct __nsw_long_err *long_errs;
	struct __nsw_lookup_v1 *next;
};

struct __nsw_switchconfig_v1 {
	int vers;
	char *dbase;
	int num_lookups;
	struct __nsw_lookup_v1 *lookups;
};

#define	__NSW_ACTION_V1(lkp, err) 	\
	((lkp)->next == NULL ? \
		__NSW_RETURN \
	: \
		((err) >= 0 && (err) < __NSW_STD_ERRS_V1 ? \
			(lkp)->actions[err] \
		: \
			__nsw_extended_action_v1(lkp, err)))


#ifdef __STDC__

struct __nsw_switchconfig_v1 *__nsw_getconfig_v1
	(const char *, enum __nsw_parse_err *);
int __nsw_freeconfig_v1(struct __nsw_switchconfig_v1 *);
action_t __nsw_extended_action_v1(struct __nsw_lookup_v1 *, int);

#else

struct __nsw_switchconfig_v1 *__nsw_getconfig_v1();
int __nsw_freeconfig_v1();
action_t __nsw_extended_action_v1();

#endif /* __STDC__ */
#endif /* __NSS_PRIVATE_INTERFACE */

#ifdef	__cplusplus
}
#endif

#endif /* _NSSWITCH_PRIV_H */
