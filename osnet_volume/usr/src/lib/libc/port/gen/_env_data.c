/*
 *	Copyright (c) 1988 AT&T
 *	All Rights Reserved.
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1995-1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)_env_data.c	1.5	96/12/04 SMI"

/*LINTLIBRARY*/

#include <synch.h>

/*
 * NOTE: This symbol definition may occur in crt1.o.  This duplication is
 * required for building ABI compliant applications (see bugid 1181124).
 * To avoid any possible incompatibility with crt1.o the initialization of
 * this variable must not change.  If change is required a new mutex variable
 * should be created.
 */

#ifdef _REENTRANT
mutex_t __environ_lock = DEFAULTMUTEX;
#endif
