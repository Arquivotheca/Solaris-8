/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _ACPI_EXC_H
#define	_ACPI_EXC_H

#pragma ident	"@(#)acpi_exc.h	1.1	99/05/21 SMI"

#ifdef __cplusplus
extern "C" {
#endif


/*
 * interface for exception reporting and debugging
 */

/*
 * There are different classes of exception and debug reporting.
 *
 * Class A. The lowest level functions just return OK (0)/EXC (-1) or
 * a pointer/NULL (0).
 *
 * Class B. Some functions can also set an exception code via exc_code()
 * and offset with exc_offset().
 *
 * Class C. Functions can print out an error or diagnostic (similar to
 * cmn_error and syslog): exc_cont, exc_debug, exc_note, exc_warn,
 * exc_panic
 *
 * exc_cont and exc_debug do not print anything unless the
 * corresponding debug mask bit has been enabled.
 *
 * exc_panic also calls the fatal handler.
 *
 * Results can be tailored with the exc_set functions.
 */

/* debug output destinations and facilities in acpi_prv.h */

extern int acpi_interpreter_revision;

extern void exc_clear(void);
extern int exc_no(void);
extern char *exc_string(int code);
extern int exc_pos(void);

/* all functions with exception returns */
extern int exc_code(int code);
extern void *exc_null(int code);
extern int exc_offset(int code, int offset);
/* exc_bst in bst.h */

extern void exc_cont(char *msg, ...);
extern void exc_debug(int facility, char *msg, ...);
extern int exc_note(char *msg, ...);
extern int exc_warn(char *msg, ...);
extern int exc_panic(char *msg, ...);

extern void exc_setlevel(int level);
extern void exc_setdebug(int mask);
extern void exc_setout(int flags);
extern void exc_settag(char *tag);
extern void exc_setpanic(void (*handler)());


#ifdef __cplusplus
}
#endif

#endif /* _ACPI_EXC_H */
