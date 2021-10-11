/*
 *	Copyright (c) 1988 AT&T
 *	All Rights Reserved.
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1995 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
/*
 * Copyright(c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)env_data.c	1.9	97/05/15 SMI"

/*
 * NOTE: The environment symbol pair may also occur in crt1.o.  The definitions
 * within crt1.o are required for the generation of ABI compliant applications
 * (see bugid 1181124).  No other symbol definitions should be added to this
 * file.
 */
#pragma weak environ = _environ
long	_environ = 0;
