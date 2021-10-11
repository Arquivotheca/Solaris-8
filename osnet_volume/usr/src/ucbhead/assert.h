/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

#ifndef _ASSERT_H
#define	_ASSERT_H

#pragma ident	"@(#)assert.h	1.4	97/06/17 SMI"	/* SVr4.0 1.1	*/

#ifdef NDEBUG
#undef assert
#define	assert(EX)

#else

#define	_assert(ex)	{if (!(ex)) \
			{(void)fprintf(stderr, "Assertion failed: file \"%s\", \
			line %d\n", __FILE__, __LINE__); exit(1); }}
#define	assert(ex)	_assert(ex)

#endif	/* NDEBUG */

#endif	/* _ASSERT_H */
