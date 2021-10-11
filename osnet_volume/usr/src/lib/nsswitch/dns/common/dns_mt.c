/*
 * Copyright (c) 1993, 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dns_mt.c	1.5	99/09/20 SMI"

/*
 * dns_mt.c
 *
 * This file contains all the MT related routines for the DNS backend.
 */

#include "dns_common.h"
#include <dlfcn.h>

/*
 * If the DNS name service switch routines are used in a binary that depends
 * on an older libresolv (libresolv.so.1, say), then having nss_dns.so.1 or
 * libnss_dns.a depend on a newer libresolv (libresolv.so.2) will cause
 * relocation problems. In particular, copy relocation of the _res structure
 * (which changes in size from libresolv.so.1 to libresolv.so.2) could
 * cause corruption, and result in a number of strange problems, including
 * core dumps. Hence, we check if a libresolv is already loaded.
 */

#pragma init	(_nss_dns_init)
static void	_nss_dns_init(void);

extern struct hostent *res_gethostbyname(const char *);
#pragma weak	res_gethostbyname

#define		RES_SET_NO_HOSTS_FALLBACK	"__res_set_no_hosts_fallback"
extern void	__res_set_no_hosts_fallback(void);
#pragma weak	__res_set_no_hosts_fallback

#define		RES_GET_RES	"__res_get_res"
extern struct __res_state	*__res_get_res(void);
#pragma weak	__res_get_res

#define		RES_ENABLE_MT			"__res_enable_mt"
extern int	__res_enable_mt(void);
#pragma weak	__res_enable_mt

#define		RES_DISABLE_MT			"__res_disable_mt"
extern int	__res_disable_mt(void);
#pragma weak	__res_disable_mt

#define		RES_GET_H_ERRNO			"__res_get_h_errno"
extern int	*__res_get_h_errno();
#pragma weak	__res_get_h_errno

void	(*set_no_hosts_fallback)() = 0;
struct __res_state	*(*set_res_retry)() = 0;
int	(*enable_mt)() = 0;
int	(*disable_mt)() = 0;
int	*(*get_h_errno)() = 0;

static int	*__h_errno(void);

/* Usually set from the Makefile */
#ifndef	NSS_DNS_LIBRESOLV
#define	NSS_DNS_LIBRESOLV	"libresolv.so.2"
#endif

/* From libresolv */
extern	int	h_errno;

mutex_t	one_lane = DEFAULTMUTEX;

void
_nss_dns_init(void)
{
	struct hostent	*(*f_hostent_ptr)();
	struct __res_state	*(*f_res_state_ptr)();
	void		*reslib, (*f_void_ptr)();

	/* If no libresolv library, then load one */
	if ((f_hostent_ptr = res_gethostbyname) == 0) {
		if ((reslib =
		dlopen(NSS_DNS_LIBRESOLV, RTLD_LAZY|RTLD_GLOBAL)) != 0) {
			/* Turn off /etc/hosts fall back in libresolv */
			if ((f_void_ptr = (void (*)(void))dlsym(reslib,
				RES_SET_NO_HOSTS_FALLBACK)) != 0) {
				set_no_hosts_fallback = f_void_ptr;
			}
			/* Set number of resolver retries */
			if ((f_res_state_ptr =
			    (struct __res_state * (*)(void))dlsym(reslib,
				RES_GET_RES)) != 0) {
				set_res_retry = f_res_state_ptr;
			}
			/* Try to bind the MT enable/disable functions */
			if ((enable_mt = (int (*)(void))dlsym(reslib,
				RES_ENABLE_MT)) != 0 &&
				(disable_mt = (int (*)(void))dlsym(reslib,
				RES_DISABLE_MT)) == 0) {
				enable_mt = 0;
			}
			/* Select h_errno retrieval function */
			if ((get_h_errno = (int * (*)(void))dlsym(reslib,
				RES_GET_H_ERRNO)) == 0) {
				get_h_errno = __h_errno;
			}
		}
	} else {
		/* Libresolv already loaded */
		if ((f_void_ptr = __res_set_no_hosts_fallback) != 0) {
			set_no_hosts_fallback = f_void_ptr;
		}
		if ((f_res_state_ptr = __res_get_res) != 0) {
			set_res_retry = f_res_state_ptr;
		}
		if ((enable_mt = __res_enable_mt) != 0 &&
			(disable_mt = __res_disable_mt) == 0) {
			enable_mt = 0;
		}
		if ((get_h_errno = __res_get_h_errno) == 0) {
			get_h_errno = __h_errno;
		}
	}
}


/*
 * Return pointer to the global h_errno variable
 */
static int *
__h_errno(void) {
	return (&h_errno);
}
