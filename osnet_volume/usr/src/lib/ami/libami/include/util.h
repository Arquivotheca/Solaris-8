/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 * #pragma ident "@(#)util.h	1.1 99/07/11 SMI"
 *
 */

#ifndef	_AMI_UTIL_H
#define	_AMI_UTIL_H

#pragma ident	"@(#)util.h	1.49	97/01/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * PROTOTYPES should be set to one if and only if the compiler supports
 * function argument prototyping.
 * The following makes PROTOTYPES default to 1 if it has not already been
 * defined as 0 with C compiler flags.
 */

#ifndef	PROTOTYPES
#define	PROTOTYPES 1
#endif

/*
 * PROTO_LIST is defined depending on how PROTOTYPES is defined above.
 * If using PROTOTYPES, then PROTO_LIST returns the list, otherwise it
 * returns an empty list.
 */

#if PROTOTYPES
#define	PROTO_LIST(list) list
#else
#define	PROTO_LIST(list) ()
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _AMI_UTIL_H */
