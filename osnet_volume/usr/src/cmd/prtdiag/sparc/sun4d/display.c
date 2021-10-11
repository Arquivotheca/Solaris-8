/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)display.c 1.7	96/02/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <varargs.h>
#include <syslog.h>
#include <sys/openpromio.h>
#include <libintl.h>
#include "pdevinfo.h"
#include "display.h"

/*
 * The following macros for dealing with raw output from the Mostek 48T02
 * were borrowed from the kernel. Openboot passes the raw Mostek data
 * thru the device tree, and there are no library routines to deal with
 * this data.
 */

/*
 * Tables to convert a single byte from binary-coded decimal (BCD).
 */
static u_char bcd_to_byte[256] = {		/* CSTYLED */
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
	10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  0,  0,  0,  0,  0,  0,
	20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  0,  0,  0,  0,  0,  0,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,  0,  0,  0,  0,  0,  0,
	50, 51, 52, 53, 54, 55, 56, 57, 58, 59,  0,  0,  0,  0,  0,  0,
	60, 61, 62, 63, 64, 65, 66, 67, 68, 69,  0,  0,  0,  0,  0,  0,
	70, 71, 72, 73, 74, 75, 76, 77, 78, 79,  0,  0,  0,  0,  0,  0,
	80, 81, 82, 83, 84, 85, 86, 87, 88, 89,  0,  0,  0,  0,  0,  0,
	90, 91, 92, 93, 94, 95, 96, 97, 98, 99,
};

#define	BCD_TO_BYTE(x)	bcd_to_byte[(x) & 0xff]
#define	YRBASE	68

static int days_thru_month[64] = {
	0, 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365, 0, 0,
};

/*
 * This function takes the raw Mostek data from the device tree translates
 * it into UNIXC time (secs since Jan 1, 1970) and returns a string from
 * ctime(3c).
 */
char *
get_time(u_char *mostek)
{
	time_t utc;
	int sec, min, hour, day, month, year;

	year	= BCD_TO_BYTE(mostek[6]) + YRBASE;
	month	= BCD_TO_BYTE(mostek[5] & 0x1f) + ((year & 3) << 4);
	day	= BCD_TO_BYTE(mostek[4] & 0x3f);
	hour	= BCD_TO_BYTE(mostek[2] & 0x3f);
	min	= BCD_TO_BYTE(mostek[1] & 0x7f);
	sec	= BCD_TO_BYTE(mostek[0] & 0x7f);

	utc = (year - 70);		/* next 3 lines: utc = 365y + y/4 */
	utc += (utc << 3) + (utc << 6);
	utc += (utc << 2) + ((year - 69) >> 2);
	utc += days_thru_month[month] + day - 1;
	utc = (utc << 3) + (utc << 4) + hour;	/* 24 * day + hour */
	utc = (utc << 6) - (utc << 2) + min;	/* 60 * hour + min */
	utc = (utc << 6) - (utc << 2) + sec;	/* 60 * min + sec */

	return (ctime((time_t *)&utc));
}

void
disp_powerfail(Prom_node *root)
{
	Prom_node *pnode;
	char *option_str = "options";
	char *pf_str = "powerfail-time";
	char *value_str;
	time_t value;

	pnode = dev_find_node(root, option_str);
	if (pnode == NULL) {
		return;
	}

	value_str = get_prop_val(find_prop(pnode, pf_str));
	if (value_str == NULL) {
		return;
	}

	value = (time_t)atoi(value_str);
	if (value == 0)
		return;

	(void) log_printf(
		gettext("Most recent AC Power Failure:\n"));
	(void) log_printf("=============================\n");
	(void) log_printf("%s", ctime(&value));
	(void) log_printf("\n");
}


/*VARARGS1*/
void
log_printf(char *fmt, ...)
{
	va_list ap;
	int len;
	static char bigbuf[4096];
	char buffer[1024];

	if (print_flag == 0) {
		return;
	}

	va_start(ap);
	if (logging != 0) {
		len = vsprintf(buffer, fmt, ap);
		(void) strcat(bigbuf, buffer);

		/* we only call to syslog when we get the entire line. */
		if (buffer[len-1] == '\n') {
			syslog(LOG_DAEMON|LOG_NOTICE, bigbuf);
			bigbuf[0] = 0;
		}

	} else {
		(void) vprintf(fmt, ap);
	}
	va_end(ap);
}

void
print_header(int board)
{
	log_printf("\n");
	log_printf("Analysis for Board %d\n", board, 0);
	log_printf("--------------------\n", 0);
}
