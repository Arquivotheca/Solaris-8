/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)opt_data.c	1.8	96/12/04 SMI"	/* SVr4.0 1.3	*/

/*LINTLIBRARY*/

/*
 * Global variables
 * used in getopt
 */

#include "synonyms.h"

int	opterr = 1;
int	optind = 1;
int	optopt = 0;
char	*optarg = 0;
