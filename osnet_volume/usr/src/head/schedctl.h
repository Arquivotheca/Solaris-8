/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SCHEDCTL_H
#define	_SCHEDCTL_H

#pragma ident	"@(#)schedctl.h	1.1	96/05/20 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/schedctl.h>

typedef struct sc_public schedctl_t;

#define	schedctl_start(p) \
		{ \
			if (p != NULL) { \
				((schedctl_t *)(p))->sc_nopreempt = 1; \
			} \
		}

#define	schedctl_stop(p) \
		{ \
			if (p != NULL) { \
				((schedctl_t *)(p))->sc_nopreempt = 0; \
				if (((schedctl_t *)(p))->sc_yield == 1) { \
					yield(); \
				} \
			} \
		}

/*
 * libsched API
 */
#if	defined(__STDC__)
schedctl_t	*schedctl_init(void);
schedctl_t	*schedctl_lookup(void);
void		schedctl_exit(void);
#else
schedctl_t	*schedctl_init();
schedctl_t	*schedctl_lookup();
void		schedctl_exit();
#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif	/* _SCHEDCTL_H */
