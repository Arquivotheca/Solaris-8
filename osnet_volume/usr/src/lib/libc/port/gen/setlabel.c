/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setlabel.c	1.3	96/12/04 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include "mtlib.h"
#include <pfmt.h>
#include <thread.h>
#include <string.h>
#include "pfmt_data.h"

int
setlabel(const char *label)
{
	(void) rw_wrlock(&_rw_pfmt_label);
	if (!label)
		__pfmt_label[0] = '\0';
	else {
		(void) strncpy(__pfmt_label, label, sizeof (__pfmt_label) - 1);
		__pfmt_label[sizeof (__pfmt_label) - 1] = '\0';
	}
	(void) rw_unlock(&_rw_pfmt_label);
	return (0);
}
