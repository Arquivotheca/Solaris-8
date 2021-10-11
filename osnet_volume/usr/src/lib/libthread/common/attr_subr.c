/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)attr_subr.c	1.10	97/08/29 SMI"

#include <sys/types.h>
#include <stdlib.h>
#include "libpthr.h"

/*
 * To allocate space for attribute objects, currently we use
 * malloc only. In future we may implement a better solution.
 */
caddr_t
_alloc_attr(size_t size)
{
	return (malloc(size));
}

/*
 * To free the attribute object space. Currently we use free()
 * but in future we may have better solution.
 */
int
_free_attr(caddr_t attr)
{
	free(attr);
	return (0);
}
