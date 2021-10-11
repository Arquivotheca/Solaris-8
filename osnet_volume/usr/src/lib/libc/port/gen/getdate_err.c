/*
 * Copyright (c) 1987 - 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)getdate_err.c	1.1	93/09/20 SMI"	/* SVr4.0 1.8 */

#pragma weak getdate_err = _getdate_err

/*LINTLIBRARY*/
/*
#include "synonyms.h"
*/
#include <mtlib.h>
#include <time.h>
#include <thread.h>
#include <thr_int.h>
#include <libc.h>
#include "tsd.h"

int _getdate_err = 0;
#ifdef _REENTRANT
int *
_getdate_err_addr(void)
{

	if (_thr_main())
		return (&_getdate_err);
	return ((int *)_tsdbufalloc(_T_GETDATE_ERR_ADDR, (size_t)1,
		    sizeof (int)));
}
#endif /* _REENTRANT */
