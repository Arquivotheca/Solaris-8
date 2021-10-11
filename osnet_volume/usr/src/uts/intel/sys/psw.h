/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PSW_H
#define	_SYS_PSW_H

#pragma ident	"@(#)psw.h	1.18	99/05/04 SMI"

#if defined(i386) || defined(__i386)

#include <ia32/sys/psw.h>

#elif defined(__ia64)

#include <ia64/sys/psw.h>

#endif

#endif	/* _SYS_PSW_H */
