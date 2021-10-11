/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)errlog.c	1.1	99/01/25 SMI"

/*
 *  errlog -- error logging facility for application programs
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "errlog.h"

/*  Statics (this object is not threadable). */
static Severity;
static struct location {
	char *l_file;
	int l_lineno;
	char *l_tag;
	char *l_line;
} Location;

/* Indentation */
static int  Trace_indent;
/* XXX YES, its 80 spaces !@#$%^ */
static char Trace_padding[] =
	"                                              "
	"                                  ";

#define	INDENT &Trace_padding[80-(Trace_indent*4)]
#define	PRINTHDR	INPUT

/*
 * errlog -- simulate the syslog printf-like interface, but
 *	with a first argument oriented toward reporting
 *	application errors.
 */
/*VARARGS2*/
void
errlog(const int descriptor, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if ((Severity < (descriptor & FATAL)) &&
	    ((descriptor & FATAL) != FATAL)) {
		/* We don't need to say/do anything. */
		return;
	}

	/* Produce the message. */
	(void) fflush(stdout); /* Synchronize streams. */
	if ((descriptor & PRINTHDR) != 0) {
		if (Location.l_file == NULL) {
			(void) fprintf(stderr, "programmer error, logerr "
			    "told to print file, line number, "
			    "but file was not set.\n");
		} else {
			(void) fprintf(stderr, "\"%s\", line %d: ",
			    Location.l_file, Location.l_lineno);
		}
	}

	/* Indent/outdent. */
	if (descriptor & INDENTED) {
		(void) fprintf(stderr, "%s", INDENT);
		Trace_indent = (Trace_indent < 20)? Trace_indent + 1: 20;
	} else if (descriptor & OUTDENTED) {
		Trace_indent = (Trace_indent > 0)? Trace_indent - 1: 0;
		(void) fprintf(stderr, "%s", INDENT);
	} else {
		(void) fprintf(stderr, "%s", INDENT);
	}

	/* Print the stuff we did all this for. */
	(void) vfprintf(stderr, format, ap);

	if ((descriptor & INPUT) && Location.l_line != NULL) {
		/* Emit trailers. Formatting TBD. */
		(void) putc('\n', stderr);
		(void) fprintf(stderr, "\tLine was: %s %s",
		    Location.l_tag, Location.l_line);
	}
	(void) putc('\n', stderr);
	(void) fflush(stderr); /* No-op on Solaris. */

	if ((descriptor & FATAL) == FATAL) {
		/* Say goodbye! */
		exit(1);
	}
	va_end(ap);
}

/*
 * seterrline -- tell errlog what the context of the error is.
 */
void
seterrline(const int lineno, const char *file,
    const char *tag, const char *line)
{
	Location.l_lineno = lineno;
	Location.l_file = (char *)file;
	Location.l_tag = (char *)tag;
	Location.l_line = (char *)line;
}

/*
 * seterrseverity -- set the severity/loudness variable.
 *	This is traditionally the ``Verbosity'' level.
 */
void
seterrseverity(const int loudness)
{
	Severity = (loudness & FATAL);
}
