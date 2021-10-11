/*
 * Copyright (c) 1993 - 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _XFN_IDENTIFIER_H
#define	_XFN_IDENTIFIER_H

#pragma ident	"@(#)FN_identifier.h	1.4	96/03/31 SMI"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Predefined format values.
 */

enum {
	FN_ID_STRING,
	FN_ID_DCE_UUID,
	FN_ID_ISO_OID_STRING
};

typedef struct {
	unsigned int	format;
	size_t		length;
	void		*contents;
} FN_identifier_t;

#ifdef __cplusplus
}
#endif

#endif /* _XFN_IDENTIFIER_H */
