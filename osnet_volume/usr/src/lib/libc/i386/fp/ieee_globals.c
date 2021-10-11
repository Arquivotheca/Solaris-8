/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)ieee_globals.c 1.7	99/03/11 SMI"

/*
		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.
*/

/*
 * Copyright (c) 1986-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * (c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 * All rights reserved.
 */

/*
 * contains definitions for variables for IEEE floating-point arithmetic
 * modes; IEEE floating-point arithmetic exception handling;
 */

#include "synonyms.h"
#include <floatingpoint.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include "tsd.h"

extern int _thr_main();

sigfpe_handler_type ieee_handlers[N_IEEE_EXCEPTION];
/*
 * Array of pointers to functions to handle SIGFPE's corresponding to IEEE
 * fp_exceptions. sigfpe_default means do not generate SIGFPE. An invalid
 * address such as sigfpe_abort will cause abort on that SIGFPE. Updated by
 * ieee_handler.
 */

fp_exception_field_type _fp_current_exceptions;	/* Current FP exceptions */
enum fp_direction_type _fp_current_direction;	/* Current rounding direction */
enum fp_precision_type _fp_current_precision;	/* Current rounding precision */
double __base_conversion_write_only_double;	/* Area for writing garbage */
int __inf_read, __inf_written, __nan_read, __nan_written;
	/*
	 * Flags to record reading or writing ASCII inf/nan representations
	 * for ieee_retrospective.
	 */

#ifdef _REENTRANT

int *
_thr_get_exceptions()
{
	static thread_key_t key = 0;

	return (_thr_main() ? (int *)&_fp_current_exceptions :
		(int *)_tsdbufalloc(_T_GET_EXCEPTIONS, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_direction()
{
	static thread_key_t key = 0;

	return (_thr_main() ? (int *)&_fp_current_direction :
		(int *)_tsdbufalloc(_T_GET_DIRECTION, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_precision()
{
	static thread_key_t key = 0;

	return (_thr_main() ? (int *)&_fp_current_precision :
		(int *)_tsdbufalloc(_T_GET_PRECISION, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_nan_written()
{
	static thread_key_t key = 0;

	return (_thr_main() ? &__nan_written :
		(int *)_tsdbufalloc(_T_GET_NAN_WRITTEN, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_nan_read()
{
	static thread_key_t key = 0;

	return (_thr_main() ? &__nan_read :
		(int *)_tsdbufalloc(_T_GET_NAN_READ, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_inf_written()
{
	static thread_key_t key = 0;

	return (_thr_main() ? &__inf_written :
		(int *)_tsdbufalloc(_T_GET_INF_WRITTEN, (size_t)1,
		sizeof (int)));
}

int *
_thr_get_inf_read()
{
	static thread_key_t key = 0;

	return (_thr_main() ? &__inf_read :
		(int *)_tsdbufalloc(_T_GET_INF_READ, (size_t)1,
		sizeof (int)));
}

#endif _REENTRANT
