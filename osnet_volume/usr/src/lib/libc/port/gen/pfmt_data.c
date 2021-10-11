/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfmt_data.c	1.3	96/12/04 SMI"

/*LINTLIBRARY*/

#include "mtlib.h"
#include <pfmt.h>
#include <thread.h>
#include "pfmt_data.h"

char __pfmt_label[MAXLABEL];
struct sev_tab *__pfmt_sev_tab;
int __pfmt_nsev;

rwlock_t _rw_pfmt_label = DEFAULTRWLOCK;
rwlock_t _rw_pfmt_sev_tab = DEFAULTRWLOCK;
