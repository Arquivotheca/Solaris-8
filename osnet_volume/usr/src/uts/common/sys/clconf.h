/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_CLCONF_H
#define	_SYS_CLCONF_H

#pragma ident	"@(#)clconf.h	1.1	98/07/17 SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This header file specifies the interface to access the
 * configuration data needed in order to boot and form a cluster.
 */

/*
 * Node identifiers are numbered 1 to clconf_maximum_nodeid().
 * The nodeid zero is used to mean unknown.
 */
#define	NODEID_UNKNOWN	0

typedef unsigned int	nodeid_t;

#if defined(_KERNEL)

extern void	clconf_init(void);
extern nodeid_t	clconf_get_nodeid(void);
extern nodeid_t	clconf_maximum_nodeid(void);
#endif /* defined(_KERNEL) */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_CLCONF_H */
