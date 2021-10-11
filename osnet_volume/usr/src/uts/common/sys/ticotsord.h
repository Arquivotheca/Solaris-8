/*
 * Copyright (c) 1993 by Sun Microsystems Inc.
 */

#ifndef _SYS_TICOTSORD_H
#define	_SYS_TICOTSORD_H

#pragma ident	"@(#)ticotsord.h	1.17	95/07/27 SMI"	/* SVr4 1.5 */

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/tl.h>

/*
 * Old error codes exposed in old man pages. Only for compatability.
 * Do not use in any new program.
 */
#define	TCOO_NOPEER ECONNREFUSED		/* no listener on dest addr */
#define	TCOO_PEERNOROOMONQ ECONNREFUSED	/* no room on incoming queue */
#define	TCOO_PEERBADSTATE ECONNREFUSED	/* peer in wrong state */
#define	TCOO_PEERINITIATED ECONNRESET	/* peer-initiated disconnect */
#define	TCOO_PROVIDERINITIATED ECONNRESET /* provider-initiated discon */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_TICOTSORD_H */
