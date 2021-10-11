/*
 * Copyright (c) 1993 by Sun Microsystems Inc.
 */

#ifndef _SYS_TICLTS_H
#define	_SYS_TICLTS_H

#pragma ident	"@(#)ticlts.h	1.14	93/11/10 SMI"	/* SVr4.0 1.6	*/

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/tl.h>

/*
 * Old error codes exposed in old man pages. Only for compatability.
 * Do not use in any new program.
 */
#define	TCL_BADADDR	EINVAL	/* bad addr specification */
#define	TCL_BADOPT	EINVAL	/* bad option specification */
#define	TCL_NOPEER	EFAULT	/* dest addr is unbound */
#define	TCL_PEERBADSTATE EPROTO	/* peer in wrong state */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_TICLTS_H */
