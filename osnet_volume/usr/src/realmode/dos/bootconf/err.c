/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * err.c -- generic error handling routines
 */

#ident	"<@(#)err.c	1.8	99/08/25 SMI>"

/*
 *      this file contains some "common" routines for handling error messages:
 *
 *              warning
 *              fatal
 *
 *                      these routines act like printf except:
 *
 *                              the program name and the string "warning: "
 *                              or "error: " is prepended to the message
 *                              unless inhibited by passing in a format
 *                              with '!' as the first character.
 *
 *                              a newline is always appended to the format.
 *
 *                              %! in the format expands to an error message
 *                              based on the current value of errno (similar
 *                              to the perror(3) routine).
 *
 *                              %~ in the format expands to the program name.
 *
 *                              fatal() calls exit(1) instead of returning.
 *
 *              done
 *
 *                      this routine just exits with the given exit code.
 *                      it should be used for non-error types of exit.
 *                      it calls the routines registered with ondoneadd.
 *
 *              ondoneadd
 *
 *                      add a routine/argument pair from the
 *                      list of routines that get called when fatal()
 *                      is called.  the routines get called BEFORE fatal()
 *                      prints its message, so they can do things like
 *                      take the screen out of graphics mode, etc.  the
 *                      registered callback routines get called with the
 *                      arg give when ondoneadd is called and the exit code
 *                      as the second arg (so the callback routines can
 *                      decide if it is an error exit or not).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "debug.h"
#include "err.h"
#include "gettext.h"
#include "menu.h"
#include "tty.h"

#ifndef MIN
#define	MIN(X, Y) ((unsigned long)(X) < (unsigned long)(Y) ? (X) : (Y))
#endif /* !MIN */

static struct cb {
	struct cb *next;
	int flags;
	void (*routine)(void *, int);
	void *arg;
} *Callbacks = 0;

/*
 * prepend_printf -- common print routine for warning() and fatal()
 */

static int
prepend_printf(const char *prefix, const char *fmt, va_list ap)
{
	int retval;
	int outcount = 0;

	if (fmt == NULL)
		return (0);	/* nothing to print */

	fmt = gettext(fmt);

	if (*fmt == '!')
		fmt++;
	else {
		if ((retval = fprintf(stderr, gettext(prefix))) < 0)
			return (retval);
		else
			outcount += retval;
	}
	if ((retval = vfprintf(stderr, fmt, ap)) < 0)
		return (retval);
	else
		outcount += retval;
	fputc('\n', stdout);
	(void) fflush(stderr);
	va_end(ap);
	return (outcount);
}


/*
 *  Execute thru callback list:
 */

static void
popback(int crashing, int exitcode)
{
	struct cb *cbp;

	while ((cbp = Callbacks) != 0) {

		Callbacks = cbp->next;

		if (!crashing || (cbp->flags & CB_EVEN_FATAL)) {

			(*cbp->routine)(cbp->arg, exitcode);
		}

		free(cbp);
	}
}

/*
 * fatal -- print a message and exit
 */

void
fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	/* run callbacks */
	popback(1, 0);

	/* print message */
	(void) prepend_printf("error: ", fmt, ap);

	va_end(ap);

	exit(1);
	/*NOTREACHED*/
}

/*
 * warning -- print a message
 */

void
warning(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	/* print message */
	(void) prepend_printf("warning: ", fmt, ap);

	va_end(ap);
}

/*
 * done -- exit the program
 */

void
done(int exitcode)
{
	popback(0, exitcode);

	exit(exitcode);
	/*NOTREACHED*/
}

/*
 * ondoneadd -- add a routine/argument pair to the callback list
 */

void
ondoneadd(void (*routine)(void *, int), void *arg, int flags)
{
	struct cb *cbp;

	cbp = (struct cb *)malloc(sizeof (struct cb));
	if (cbp == 0) MemFailure();

	cbp->routine = routine;
	cbp->flags = flags;
	cbp->arg = arg;

	cbp->next = Callbacks;
	Callbacks = cbp;
}

void
write_err(char *file)
{
	refresh_tty(1);
	enter_menu(0, "MENU_WRITE_ERR", file);
}
