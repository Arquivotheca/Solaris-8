/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SVR4MSG_SPEC_H
#define	_SVR4MSG_SPEC_H

#pragma ident	"@(#)svr4msg_spec.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern	"C"	{
#endif

#ifdef _XPG4_2
#undef	_XPG4_2
#endif

#ifdef _KERNEL
#undef	_KERNEL
#endif

#define	msghdr SVR4_msghdr

#include <sys/types.h>
#include <sys/socket.h>

#ifdef	__cplusplus
}
#endif

#endif /* _SVR4MSG_SPEC_H */
