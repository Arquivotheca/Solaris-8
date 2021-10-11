/*
 *	Copyright(c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright(c) 1995 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */

#pragma ident	"@(#)global.c	1.5	95/08/29 SMI"
#include "mcs.h"
int actmax = 0;
char *elftmpfile, *artmpfile;

action *Action;

int signum[] = {SIGHUP, SIGINT, SIGQUIT, 0};

char *prog;
S_Name *sect_head = NULL;
