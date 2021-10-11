/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_POLICY_H
#define	_SYS_POLICY_H

#pragma ident	"@(#)boot_policy.h	1.1	98/06/16 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * External interfaces
 */
extern void policy_open(void);
extern void policy_close(void);
extern char *policy_lookup(char *pattern, int ignore_case);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_POLICY_H */
