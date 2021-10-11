/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_STDIO_SPEC_H
#define	_STDIO_SPEC_H

#pragma ident	"@(#)stdio_spec.h	1.1	99/01/25 SMI"

/* undefine aliases that were defined in <stdio.h> */
#include <stdio.h>

#undef	clearerr
#undef	feof
#undef	ferror
#undef	getc
#undef	getchar
#undef	putc
#undef	putchar

#endif	/* _STDIO_SPEC_H */
