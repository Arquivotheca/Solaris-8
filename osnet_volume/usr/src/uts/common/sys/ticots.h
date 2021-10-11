/*
 * Copyright (c) 1993 by Sun Microsystems Inc.
 */

#ifndef _SYS_TICOTS_H
#define	_SYS_TICOTS_H

#pragma ident	"@(#)ticots.h	1.17	95/07/27 SMI"	/* SVr4.0 1.4 */

#ifdef	__cplusplus
extern "C" {
#endif

#include	<sys/tl.h>

/*
 * Old error codes exposed in old man pages. Only for compatability.
 * Do not use in any new program.
 */
#define	TCO_NOPEER ECONNREFUSED		/* no listener on dest addr */
#define	TCO_PEERNOROOMONQ ECONNREFUSED	/* no room on incoming queue */
#define	TCO_PEERBADSTATE ECONNREFUSED	/* peer in wrong state */
#define	TCO_PEERINITIATED ECONNRESET	/* peer-initiated disconnect */
#define	TCO_PROVIDERINITIATED ECONNRESET /* provider-initiated discon */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_TICOTS_H */
