/*
 * Copyright (c) 1999 by Sun Microsystems, Inc. All rights reserved.
 *
 * This is an internal header file. Not to be shipped.
 */

#ifndef	_AUTH_LIST_H
#define	_AUTH_LIST_H

#pragma ident	"@(#)auth_list.h	1.1	99/04/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Names of authorizations currently in use in the system
 */

#define	CRONADMIN_AUTH		"solaris.jobs.admin"
#define	CRONUSER_AUTH		"solaris.jobs.user"
#define	DEFAULT_DEV_ALLOC_AUTH	"solaris.device.allocate"
#define	DEVICE_REVOKE_AUTH	"solaris.device.revoke"
#define	SET_DATE_AUTH		"solaris.system.date"

#ifdef	__cplusplus
}
#endif

#endif	/* _AUTH_LIST_H */
