/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * err.h -- public definitions for err module
 */

#ifndef	_ERR_H
#define	_ERR_H

#ident "@(#)err.h   1.10   99/11/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* flags for ondoneadd() */
#define	CB_EVEN_FATAL	0x01		/* call even on fatal exit */

void fatal(const char *fmt, ...);
void warning(const char *fmt, ...);
void done(int exitcode);
void ondoneadd(void (*routine)(void *arg, int exitcode), void *arg, int flags);
void write_err(char *file);

#ifdef	__cplusplus
}
#endif

#endif	/* _ERR_H */
