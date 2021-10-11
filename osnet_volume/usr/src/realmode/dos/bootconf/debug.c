/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * debug.c -- debug routines
 */

#ident "@(#)debug.c   1.17   98/05/14 SMI"

#include <stdarg.h>
#include <stdio.h>
#include "debug.h"
#include "err.h"
#include "tty.h"

unsigned long Debug = 0; /* -d[mask] (turn on extra debugging info) */
FILE *Debug_file = NULL;

/*
 * init_debug -- initialize the debug module
 */

void
init_debug()
{
	if (Debug) {

		if ((Debug_file = fopen("debug.txt", "w")) == NULL)
			fatal("init_debug: can't open debug.txt");

		if (Debug) {
			(void) fprintf(Debug_file, "debug mask is 0x%lx\n",
				Debug);
			if ((Debug & D_NOFLUSH) == 0)
				fflush(Debug_file);
		}
	}
}

/*
 * debug -- log to a file, or print to the screen a debug message
 */

void
debug(long dtype, const char *fmt, ...)
{
	va_list ap;

	if (dtype & Debug) {
		va_start(ap, fmt);
		if ((Debug & D_TTY) != 0) {
			(void) viprintf_tty(fmt, ap);
		} else {
			(void) vfprintf(Debug_file, fmt, ap);
			if ((Debug & D_NOFLUSH) == 0)
			    fflush(Debug_file);
			va_end(ap);
		}
	}
}

/*
 *  memfail -- print "out of memory" message:
 *
 *  If the "debug" flag is non-zero, the message includes the file name and
 *  line number where the error occurred.
 */

void
memfail_debug(char *file, int line)
{
	char buf[128];

	if (Debug) (void) sprintf(buf, " - \"%s\":%d", file, line);
	else buf[0] = 0;

	fatal("out of memory%s", buf);
}

void
assfail_debug(char *expr, char *file, int line)
{
	debug(D_ERR, "Assertion failure: - \"%s\", \"%s\" line %d",
			expr, file, line);
	fatal("Assertion failure: - \"%s\", \"%s\" line %d",
		expr, file, line);
}
