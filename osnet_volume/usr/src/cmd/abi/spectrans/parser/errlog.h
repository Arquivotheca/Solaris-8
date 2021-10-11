/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_ERRLOG_H
#define	_ERRLOG_H

#pragma ident	"@(#)errlog.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  errlog -- error logging facility for application programs
 *
 */

extern void errlog(const int, const char *, ...);
extern void seterrline(const int, const char *, const char *, const char *);
extern void seterrseverity(const int);
extern void openerrlog(const char *, const int, const int);
extern void closeerrlog(void);

/*
 * The first (non-short) int of errlog really describes a packed
 * form of three extensible enumerations, similar to:
 * typedef struct severity {
 *	int	descriptor:  8;	OTHER=0, INPUT or PROGRAM.
 *	int     attributes:  8;	NONE=0, INDENTED, OUTDENTED, etc.
 *	int	severity:   16;	FATAL (_ERROR)=-1, (RECOVERABLE_) ERROR=0
 *				WARNING, TRACING, VERBOSE (_TRACING), etc.
 * } severity_t;
 */

#define	FATAL	0x00FF
#define	ERROR	0

#define	WARNING 1
#define	STATUS  2
#define	TRACING	3
#define	VERBOSE	4

#define	INPUT	(1 << 8)
#define	PROGRAM	(2 << 8)
#define	OTHER	0

/* Reserved for implementor. */
#define	INDENTED (1 << 16)
#define	OUTDENTED (2 << 16)
#define	BEGIN	(OTHER | TRACING | INDENTED)
#define	END	(OTHER | TRACING | OUTDENTED)

#ifndef assert
/* EXPERIMENTAL assert replacement, deliberately not source-compatable */
#define	assert(cond, string) \
	if (!(cond)) { \
		seterrline(__LINE__, __FILE__, NULL, NULL); \
		errlog(FATAL|PROGRAM, string);	\
	}
#else
#error "assert.h and errlog.h both define assert: choose only one"
#endif	/* assert */

#ifdef	__cplusplus
}
#endif

#endif	/* _ERRLOG_H */
