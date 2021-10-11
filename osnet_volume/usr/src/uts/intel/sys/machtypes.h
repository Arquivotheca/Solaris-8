/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_MACHTYPES_H
#define	_SYS_MACHTYPES_H

#pragma ident	"@(#)machtypes.h	1.8	99/05/04 SMI"

#include <sys/feature_tests.h>

#if defined(i386) || defined(__i386)

#include <ia32/sys/machtypes.h>

#elif defined(__ia64)

#include <ia64/sys/machtypes.h>

#endif

#endif	/* _SYS_MACHTYPES_H */
