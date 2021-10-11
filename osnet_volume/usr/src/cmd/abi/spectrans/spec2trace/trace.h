/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _TRACE_H
#define	_TRACE_H

#pragma ident	"@(#)trace.h	1.2	99/05/14 SMI"

#include "symtab.h"

#ifdef	__cplusplus
extern "C" {
#endif

#define	TRACE_VERSION "1.1"

/* Return codes from back- to front-end. */
enum retcode_t { SUCCESS_RC = 0, ERROR_RC = -1, SKIP_RC = 1};

/* Kinds of code-generation to do. */
typedef enum {AUDIT, PRELOAD} CODE;

/* Global functions. */
extern void stats_add_warning(void);
extern void stats_add_error(void);
extern void generate_interceptor(ENTRY *);
extern void print_function_signature(char *, char *, char *);
extern void generate_linkage(ENTRY *function);

/* Global variables */
extern CODE Generate;

/* Defines. */
#define	YES	1
#define	NO	0
#define	ERR (-1)

#define	MAXLINE 1024

#ifdef	__cplusplus
}
#endif

#endif /* _TRACE_H */
