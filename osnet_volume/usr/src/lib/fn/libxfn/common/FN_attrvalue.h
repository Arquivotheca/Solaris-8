/*
 * Copyright (c) 1993 - 1994 by Sun Microsystems, Inc.
 */

#ifndef _XFN_ATTRVALUE_H
#define	_XFN_ATTRVALUE_H

#pragma ident	"@(#)FN_attrvalue.h	1.3	94/11/20 SMI"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	size_t	length;
	void	*contents;
} FN_attrvalue_t;

#ifdef __cplusplus
}
#endif

#endif /* _XFN_ATTRVALUE_H */
