/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_WIDEC_SPEC_H
#define	_WIDEC_SPEC_H

#pragma ident	"@(#)widec_spec.h	1.1	99/01/25 SMI"

#include <widec.h>
#include <wchar.h>
#include <wctype.h>

/* undefine aliases that were defined in widec.h */
#undef	getwc
#undef	putwc
#undef	getwchar
#undef	putwchar
#undef	watol
#undef	watoll
#undef	watoi
#undef	watof

#endif	/* _WIDEC_SPEC_H */
