/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfmt_data.h	1.4	96/11/19 SMI"

/*LINTLIBRARY*/

#include "mtlib.h"
#include <synch.h>

/* Severity */
struct sev_tab {
	int severity;
	char *string;
};

extern char __pfmt_label[MAXLABEL];
extern struct sev_tab *__pfmt_sev_tab;
extern int __pfmt_nsev;

extern rwlock_t _rw_pfmt_label;
extern rwlock_t _rw_pfmt_sev_tab;

extern const char * __gtxt(const char *, int, const char *);
extern int __pfmt_print(FILE *, long, const char *,
	const char **, const char **, va_list);
extern int __lfmt_log(const char *, const char *, va_list, long, int);
