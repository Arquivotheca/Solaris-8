/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INITIALIZE_H
#define	_INITIALIZE_H

#pragma ident	"@(#)initialize.h	1.1	98/12/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <fcntl.h>
#include <locale.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <strings.h>
#include <wchar.h>

#include "streams.h"
#include "types.h"
#include "utility.h"

extern void initialize_pre(sort_t *);
extern void initialize_post(sort_t *);

extern void remove_output_guard(sort_t *);

extern const char *filename_stdout;

#ifdef	__cplusplus
}
#endif

#endif	/* _INITIALIZE_H */
