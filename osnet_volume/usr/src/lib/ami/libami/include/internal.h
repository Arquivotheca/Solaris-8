/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)internal.h	1.1 99/07/11 SMI"
 *
 */
 
/*
 * Internal representation of Cryptoki objects and sessions
 */

#ifndef	_INTERNAL_H
#define	_INTERNAL_H

#pragma ident	"@(#)internal.h	1.38	97/01/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include "cryptoki.h"
#include "global.h"
#include "bsafe.h"

typedef struct {
	CK_ATTRIBUTE_PTR pTemplate;	/* object's template */
	CK_USHORT usAttrCount;		/* number of attributes in template */
} OBJECT, *OBJECT_PTR;

typedef struct {
	B_ALGORITHM_OBJ		algorithmObject;
	B_ALGORITHM_OBJ		randomAlgorithm;
	B_KEY_OBJ		keyObject;
	u_int			outbufSize;
	CK_MECHANISM_TYPE	mechanism;
	CK_BYTE_PTR		pSeed;
	CK_USHORT		seedLen;
} SESSION_OBJECT, *SESSION_OBJECT_PTR;

#ifdef	__cplusplus
}
#endif

#endif	/* _INTERNAL_H */
