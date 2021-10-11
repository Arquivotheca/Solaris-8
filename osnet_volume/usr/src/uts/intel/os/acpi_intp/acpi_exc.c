/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)acpi_exc.c	1.1	99/05/21 SMI"


/* exception reporting and debugging */

#include "acpi_defs.h"
#include <sys/inttypes.h>
#include <sys/acpi.h>
#include <sys/acpi_prv.h>

#ifdef ACPI_BOOT
#include <sys/salib.h>
#include <sys/promif.h>
#include <sys/varargs.h>

/* including cmn_err.h causes conflicts with salib.h */
#define	CE_CONT  (0)
#define	CE_NOTE	 (1)
#define	CE_WARN	 (2)
#define	CE_PANIC (3)
#endif

#ifdef ACPI_KERNEL
#include <sys/cmn_err.h>
#endif

#ifdef ACPI_USER
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/cmn_err.h>
#endif

#include "acpi_exc.h"

#ifdef ACPI_USER
unsigned int acpi_debug_prop;
#endif

int acpi_interpreter_revision = 1;

static int exc_number;
static int exc_offset_pos;
static char *exc_tag_string;
static void (*exc_panic_handler)();

#define	EXC_BUFSIZ (1024)
static char exc_buf[EXC_BUFSIZ];

static char *exc_strings[] = {
/* -1 */ "unspecified error",
/*  0 */ "ok",
/*  1 */ "internal error",
/*  2 */ "resource/memory exhaustion",
/*  3 */ "premature end",
/*  4 */ "out of range",
/*  5 */ "over or underflow",
/*  6 */ "parse error",
/*  7 */ "reduce error",
/*  8 */ "other error",
/*  9 */ "bad object",
/* 10 */ "not enough args",
/* 11 */ "cannot eval object",
/* 12 */ "wrong type in expression",
/* 13 */ "fatal operator",
/* 14 */ "bad BCD",
/* 15 */ "RefOf failed",
/* 16 */ "region access failed",
/* 17 */ "mutex violation",
/* 18 */ "bad character",
/* 19 */ "bad checksum",
/* 20 */ "already defined",
/* 21 */ "undefined",
/* 22 */ "bad table",
};

void
exc_clear(void)
{
	exc_number = 0;
	exc_offset_pos = 0;
}

int
exc_no(void)
{
	return (exc_number);
}

/*LINTLIBRARY*/
char *
exc_string(int code)
{
	if (code < ACPI_EXC || code > ACPI_EMAX)
		code = ACPI_EXC;
	return (exc_strings[code + 1]);	/* -1 based array */
}

/*LINTLIBRARY*/
int
exc_pos(void)
{
	return (exc_offset_pos);
}

/* returns EXC to be useable in upper level return */
int
exc_code(int code)
{
	exc_number = (code < ACPI_EXC || code > ACPI_EMAX) ? ACPI_EXC : code;
	return (ACPI_EXC);
}

/* version of exc_code that returns NULL pointer */
void *
exc_null(int code)
{
	exc_number = (code < ACPI_EXC || code > ACPI_EMAX) ? ACPI_EXC : code;
	return (NULL);
}

/* returns EXC to be useable in upper level return */
int
exc_offset(int code, int offset)
{
	(void) exc_code(code);
	exc_offset_pos = offset;
	return (ACPI_EXC);
}

/* printing functions */
/* tailored version of vcmn_err */

static void
exc_output(int level, int prefix, char *msg, va_list ap)
{
	char *ptr = &exc_buf[0];
	int remain = EXC_BUFSIZ - 1;
	int len;

	switch (acpi_debug_prop & ACPI_DOUT_MASK) {
	case ACPI_DOUT_DFLT:	/* default */
		break;
	case ACPI_DOUT_CONS:	/* ^ */
		*ptr++ = '^';
		remain--;
		break;
	case ACPI_DOUT_LOG:	/* ! */
		*ptr++ = '!';
		remain--;
		break;
	case ACPI_DOUT_USER:	/* ? */
		*ptr++ = '?';
		remain--;
		break;
	}
	*ptr = NULL;
	/* XXX do something for ACPI_DOUT_FILE */

	if (prefix && exc_tag_string) {
		if ((len = strlen(exc_tag_string)) > remain - 1)
			len = remain - 1;
		(void) strncpy(ptr, exc_tag_string, len);
		*ptr++ = ' ';
		*ptr = NULL;
		len++;
		ptr += len;
		remain -= len;
	}
#ifdef ACPI_BOOT
	(void) prom_vsprintf(ptr, msg, ap);
#else
	(void) vsnprintf(ptr, remain, msg, ap);
#endif
#ifdef ACPI_KERNEL
	cmn_err(level, &exc_buf[0]);
#else
	(void) printf(&exc_buf[0]);
	if (level != CE_CONT)
		(void) printf("\n");
#endif
}

void
exc_cont(char *msg, ...)
{
	va_list ap;

	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_DEBUG) {
		va_start(ap, msg);
		exc_output(CE_CONT, 0, msg, ap);
		va_end(ap);
	}
}

void
exc_debug(int facility, char *msg, ...)
{
	va_list ap;

	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_DEBUG &&
	    (facility & acpi_debug_prop)) {
		va_start(ap, msg);
		exc_output(CE_NOTE, 1, msg, ap);
		va_end(ap);
	}
}

/*LINTLIBRARY*/
int
exc_note(char *msg, ...)
{
	va_list ap;

	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_NOTE) {
		va_start(ap, msg);
		exc_output(CE_NOTE, 1, msg, ap);
		va_end(ap);
	}
	return (ACPI_EXC);
}

int
exc_warn(char *msg, ...)
{
	va_list ap;

	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_WARN) {
		va_start(ap, msg);
		exc_output(CE_WARN, 1, msg, ap);
		va_end(ap);
	}
	return (ACPI_EXC);
}

/*LINTLIBRARY*/
int
exc_panic(char *msg, ...)
{
	va_list ap;
/* XXX compiler complains this is always true, which is true for now */
#if 0
	if ((acpi_debug_prop & ACPI_DVERB_MASK) >= ACPI_DVERB_PANIC) {
#endif
		va_start(ap, msg);
		exc_output(CE_PANIC, 1, msg, ap);
		va_end(ap);
#if 0
	}
#endif
	/* might not get here */
	if (exc_panic_handler)
		(*exc_panic_handler)();	/* might not return */
	return (ACPI_EXC);
}

/* set functions */
void
exc_setlevel(int level)
{
	acpi_debug_prop &= ~ACPI_DVERB_MASK;
	acpi_debug_prop |= (level & ACPI_DVERB_MASK);
}

void
exc_setdebug(int mask)
{
	acpi_debug_prop &= ~ACPI_DFAC_MASK;
	acpi_debug_prop |= (mask & ACPI_DFAC_MASK);
}

void
exc_setout(int flags)
{
	acpi_debug_prop &= ~ACPI_DOUT_MASK;
	acpi_debug_prop |= (flags & ACPI_DOUT_MASK);
}

/* won't survive module reloads */
void
exc_settag(char *tag)
{
	exc_tag_string = tag;
}

/* won't survive module reloads */
void
exc_setpanic(void (*handler)())
{
	exc_panic_handler = handler;
}


/* eof */
