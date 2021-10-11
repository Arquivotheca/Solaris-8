/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)fcntl.c	1.5	97/08/28 SMI"

/*
 * fcntl() is being interposed here so that it can be made a cancellation
 * point. But fcntl() is a cancellation point only if cmd == F_SETLKW.
 *
 * fcntl()'s prototype has variable arguments. This makes very difficult
 * to pass arguments to libc version and cancel version.
 *
 * We are including <sys/fcntl.h> to bypass the prototype and to include
 * F_SETLKW definition. We are also assuming that fcntl() can only have
 * three arguments at the most.
 */
#ifdef __STDC__
#pragma	weak _ti_fcntl = fcntl
#endif /* __STDC__ */

#include <sys/types.h>
#include <sys/fcntl.h>

int	_fcntl_cancel(int, int, intptr_t);
int	_fcntl(int, int, intptr_t);

int
fcntl(int fildes, int cmd, intptr_t arg)
{
	int ret = 0;

	if (cmd == F_SETLKW) {
		ret = _fcntl_cancel(fildes, cmd, arg);
	} else {
		ret = _fcntl(fildes, cmd, arg);
	}
	return (ret);
}
