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
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 * External function definitions
 * for routines described in string(3).
 */

#ifndef	_STRINGS_H
#define	_STRINGS_H

#pragma ident	"@(#)strings.h	1.2	97/06/17 SMI"	/* SVr4.0 1.2	*/

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__STDC__)
extern	char *index(char *, char);
extern	char *rindex(char *, char);
extern	int bcmp(const void *, const void *, size_t);
extern	void bcopy(const void *, void *, size_t);
extern	void bzero(void *, size_t);
#else
extern	char	*index();
extern	char	*rindex();
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _STRINGS_H */
