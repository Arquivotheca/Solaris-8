/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)debug.h	6.5	93/06/07 SMI"

/*
*	file: debug.h
*	desc: Debug macros for the profiler.
*	date: 11/09/88
*/
#include "stdio.h"


#ifdef DEBUG

#ifndef PROF_DEBUG
#define PROF_DEBUG 2
#endif

int	debug_value;
#define DEBUG_EXP(exp)	exp; fflush(stdout)
#define DEBUG_LOC(name)	printf("Location: %s\n",name); fflush(stdout)

#define NO_DEBUG(exp)	
#define NO_DEBUG_LOC(name)

#else

#define DEBUG_EXP(exp)
#define DEBUG_LOC(name)

#define NO_DEBUG(exp)
#define NO_DEBUG_LOC(name)
#endif

