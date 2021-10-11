/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_REFSTR_IMPL_H
#define	_SYS_REFSTR_IMPL_H

#pragma ident	"@(#)refstr_impl.h	1.1	99/03/31 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Strings with reference counts.
 * The refstr_t definition is private to the implementation.
 * <sys/refstr.h> just declares it as a 'struct refstr'.
 * We require there never to be an allocation larger than 4 Gbytes.
 */

struct refstr {
	uint32_t	rs_size;	/* allocation size */
	uint32_t	rs_refcnt;	/* reference count */
	char		rs_string[1];	/* constant string */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_REFSTR_IMPL_H */
