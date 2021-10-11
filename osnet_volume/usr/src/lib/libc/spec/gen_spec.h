/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_GEN_SPEC_H
#define	_GEN_SPEC_H

#pragma ident	"@(#)gen_spec.h	1.1	99/01/25 SMI"

/* undefine aliases that were defined in <ndbm.h> */
#include <ndbm.h>

#undef	dbm_clearerr
#undef	dbm_error

/* undefine aliases that were defined in <ftw.h> */
#include <ftw.h>

#undef	ftw

/* undefine aliases that were defined in <dirent.h> */
#include <sys/types.h>
#include <dirent.h>

#undef	rewinddir

#endif	/* _GEN_SPEC_H */
