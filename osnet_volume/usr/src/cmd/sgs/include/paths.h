/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1994 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)paths.h	1.9	98/02/04 SMI"

/*
 * Pathnames for SGS
 */
#ifndef	_PATHS_H
#define	_PATHS_H


/*
 * Default directory search path for link-editor.  YLDIR says which directory
 * in LIBPATH is replaced by the -YL option to cc and ld.  YUDIR says which
 * directory is replaced by -YU.
 */
#define	LIBPATH		"/usr/ccs/lib:/usr/lib"
#define	LIBPATH64	"/usr/lib/sparcv9"
#define	YLDIR		1
#define	YUDIR		2

/*
 * Directories for run-time loader library lookups and `trusted' comparisons.
 */
#define	LIBDIR		"/usr/lib"
#define	LIBDIRLEN	8
#define	LIBDIRLEN64	16
#define	LIBDIR4X	"/usr/4lib"
#define	LIBDIR4XLEN	9
#define	LIBDIRUCB	"/usr/ucblib"
#define	LIBDIRUCBLEN	11
#define	LIBDIRLCL	"/usr/local/lib"
#define	LIBDIRLCLLEN	14

#define	ETCDIR		"/etc/lib"
#define	ETCDIRLEN	8

/*
 * Dynamic linker names.
 */
#define	LDSO_PATH	"/usr/lib/ld.so.1"
#define	LDSO_NAME	"ld.so.1"

#endif
