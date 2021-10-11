/*  Copyright (c) 1988 AT&T */
/*    All Rights Reserved   */

/*  THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T */
/*  The copyright notice above does not evidence any    */
/*  actual or intended publication of such source code. */

/*
 *      Copyright (c) 1997, by Sun Microsystems, Inc.
 *      All rights reserved.
 */

#pragma ident  "@(#)use_env.c 1.4 97/08/14 SMI"

/*LINTLIBRARY*/

extern	char	_use_env;	/* in curses.c */

void
use_env(int bf)
{
	/* LINTED */
	_use_env = (char)bf;
}
