/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfmt_print.c	1.5	96/11/19 SMI"

/*LINTLIBRARY*/

/*
 * pfmt_print() - format and print
 */
#include "synonyms.h"
#include "mtlib.h"
#include <pfmt.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <thread.h>
#include <ctype.h>
#include "pfmt_data.h"

/* Catalogue for local messages */
#define	fmt_cat		"uxlibc"
#define	def_colon	": "
#define	def_colonid	2

/* Table of default severities */
static const char *sev_list[] = {
	"SEV = %d",
	"TO FIX",
	"ERROR",
	"HALT",
	"WARNING",
	"INFO"
};

int
__pfmt_print(FILE *stream, long flag, const char *format,
	const char **text_ptr, const char **sev_ptr, va_list args)
{
	const char *ptr;
	char catbuf[DB_NAME_LEN];
	int i, status;
	int length = 0;
	int txtmsgnum = 0;
	int dofmt = (flag & (long) MM_NOSTD) == 0;
	long doact = (flag & (long) MM_ACTION);

	if (format && !(flag & (long) MM_NOGET)) {
		char c;
		ptr = format;
		for (i = 0; i < DB_NAME_LEN - 1 && (c = *ptr++) && c != ':';
			i++)
			catbuf[i] = c;
		/* Extract the message number */
		if (i != DB_NAME_LEN - 1 && c) {
			catbuf[i] = '\0';
			while (isdigit(c = *ptr++)) {
				txtmsgnum *= 10;
				txtmsgnum += c - '0';
			}
			if (c != ':')
				txtmsgnum = -1;
		}
		else
			txtmsgnum = -1;
		format = __gtxt(catbuf, txtmsgnum, ptr);

	}

	if (text_ptr)
		*text_ptr = format;
	if (dofmt) {
		int severity, sev, d_sev;
		const char *psev, *colon = NULL;

		(void) rw_rdlock(&_rw_pfmt_label);
		if (*__pfmt_label && stream) {
			if ((status = fprintf(stream, __pfmt_label)) < 0) {
				(void) rw_unlock(&_rw_pfmt_label);
				return (-1);
			}
			length += status;
			colon = __gtxt(fmt_cat, def_colonid, def_colon);
			if ((status = fprintf(stream, colon)) < 0) {
				(void) rw_unlock(&_rw_pfmt_label);
				return (-1);
			}
			length += status;
		}
		(void) rw_unlock(&_rw_pfmt_label);
		severity = (int) (flag & 0xff);

		if (!colon)
			colon = __gtxt(fmt_cat, def_colonid, def_colon);
		if (doact) {
			d_sev = sev = 1;
		} else if (severity <= MM_INFO) {
			sev = severity + 3;
			d_sev = severity + 2;
		} else {
			int i;
			(void) rw_rdlock(&_rw_pfmt_sev_tab);
			for (i = 0; i < __pfmt_nsev; i++) {
				if (__pfmt_sev_tab[i].severity == severity) {
					psev = __pfmt_sev_tab[i].string;
					d_sev = sev = -1;
					break;
				}
			}
			(void) rw_unlock(&_rw_pfmt_sev_tab);
			if (i == __pfmt_nsev)
				d_sev = sev = 0;
		}

		if (sev >= 0) {
			psev = __gtxt(fmt_cat, sev, sev_list[d_sev]);
		}

		if (sev_ptr)
			*sev_ptr = psev;

		if (stream) {
			if ((status = fprintf(stream, psev, severity)) < 0)
				return (-1);
			length += status;
			if ((status = fprintf(stream, colon)) < 0)
				return (-1);
			length += status;
		} else
			return (-1);
	} else if (sev_ptr)
		*sev_ptr = NULL;

	if (stream) {
		if ((status = vfprintf(stream, format, args)) < 0)
			return (-1);
		length += status;
	} else
		return (-1);

	return (length);
}
