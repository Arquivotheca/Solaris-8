/*
 *		Copyright (C) 1995  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

#ifndef _MEM_H
#define	_MEM_H

#pragma ident	"@(#)mem.h	1.1	97/08/07 SMI"

#include <sys/types.h>
#include <malloc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This is the header corresponding to a set of routines which
 * interpose on malloc, realloc, and calloc.  These routines call
 * an error handler if they fail, allowing one to write code which
 * does not concern itself with out-of-memory conditions.
 *
 * The actual allocation routines have the same signatures as usual,
 * specified in malloc.h.
 *
 * This is the type for an error handler and a routine to get and set it.
 * The value passed to the handler is the number of bytes requested.  If
 * the handler returns 0, these routines return NULL (just as their normal
 * counterparts would); otherwise they attempt the allocation again.
 */
typedef int (*alloc_err_func_t)(size_t);
alloc_err_func_t set_alloc_err_func(alloc_err_func_t);

/*
 * If this package can't find malloc and realloc, put the path to the
 * dynamic object in this variable.
 */
extern char *alloc_func_object;

#ifdef	__cplusplus
}
#endif

#endif /* _MEM_H */
