/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1998-1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * An application should not include this header directly.  Instead it
 * should be included only through the inclusion of other Sun headers.
 *
 * The contents of this header is limited to identifiers specified in the
 * C Standard.  Any new identifiers specified in future amendments to the
 * C Standard must be placed in this header.  If these new identifiers
 * are required to also be in the C++ Standard "std" namespace, then for
 * anything other than macro definitions, corresponding "using" directives
 * must also be added to <signal.h>.
 */

#ifndef _ISO_SIGNAL_ISO_H
#define	_ISO_SIGNAL_ISO_H

#pragma ident	"@(#)signal_iso.h	1.1	99/08/09 SMI"
/* SVr4.0 1.5.3.4 */

#include <sys/iso/signal_iso.h>

#ifdef	__cplusplus
extern "C" {
#endif

#if __cplusplus >= 199711L
namespace std {
#endif

typedef int	sig_atomic_t;

#if defined(__STDC__)

#ifdef __cplusplus
extern "C" SIG_PF signal(int, SIG_PF);
#else
extern void (*signal(int, void (*)(int)))(int);
#endif
extern int raise(int);

#else /* __STDC__ */

extern	void(*signal())();
extern int raise();

#endif /* __STDC__ */

#if __cplusplus >= 199711L
}
#endif /* end of namespace std */

#ifdef	__cplusplus
}
#endif

#endif	/* _ISO_SIGNAL_ISO_H */
