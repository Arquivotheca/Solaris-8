/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UTILITY_H
#define	_UTILITY_H

#pragma ident	"@(#)utility.h	1.4	99/11/24 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/varargs.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libintl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "types.h"

#define	CMDNAME	"sort"

#ifndef TRUE
#define	TRUE	1
#endif /* TRUE */

#ifndef FALSE
#define	FALSE	0
#endif /* FALSE */

#define	SGN(x)		(((x) == 0 ? 0 : ((x) > 0 ? 1 : -1)))
#define	MIN(x, y)	(((x) < (y)) ? (x) : (y))
#define	MAX(x, y)	(((x) > (y)) ? (x) : (y))

#define	SE_BAD_FIELD			1
#define	SE_BAD_SPECIFIER		2
#define	SE_BAD_STREAM			3
#define	SE_CANT_MMAP_FILE		4
#define	SE_CANT_OPEN_FILE		5
#define	SE_CANT_SET_SIGNAL		6
#define	SE_CAUGHT_SIGNAL		7
#define	SE_CHECK_ERROR			8
#define	SE_CHECK_FAILED			9
#define	SE_CHECK_SUCCEED		10
#define	SE_ILLEGAL_CHARACTER		11
#define	SE_INSUFFICIENT_DESCRIPTORS	12
#define	SE_INSUFFICIENT_MEMORY		13
#define	SE_MMAP_FAILED			14
#define	SE_MUNMAP_FAILED		15
#define	SE_READ_FAILED			16
#define	SE_REALLOCATE_BUFFER		17
#define	SE_STAT_FAILED			18
#define	SE_TOO_MANY_TEMPFILES		19
#define	SE_UNLINK_FAILED		20
#define	SE_USAGE			21
#define	SE_WRITE_FAILED			22

#define	KILOBYTE			1024
#define	MEGABYTE			(1024 * KILOBYTE)

#define	AV_MEM_MULTIPLIER		3
#define	AV_MEM_DIVISOR			4

#define	OUTPUT_MODE	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | \
    S_IWOTH)

extern void swap(void **, void **);
extern int bump_file_template(char *);
extern size_t strtomem(char *);
extern size_t available_memory(size_t);
extern void set_memory_ratio(sort_t *, int *, int *);
extern void set_cleanup_chain(stream_t *);
extern void set_output_file(char *);
extern void set_output_guard(stream_t *);
extern void warning(const char *, ...);
extern void terminate(int, ...);
extern void *safe_realloc(void *, size_t);
extern void safe_free(void *);

extern void *xzmap(void *, size_t, int, int, off_t);
extern void hold_file_descriptor(void);
extern void release_file_descriptor(void);

extern void copy_line_rec(const line_rec_t *, line_rec_t *);
extern void trip_eof(FILE *f);

extern ssize_t cxwrite(int, char *, size_t);
extern ssize_t wxwrite(int, wchar_t *, size_t);

extern int xstreql(const char *, const char *);
extern int xstrneql(const char *, const char *, const size_t);
extern char *xstrnchr(const char *, const int, const size_t);
extern void xstrninv(char *, ssize_t, ssize_t);

extern int xwcsneql(const wchar_t *, const wchar_t *, const size_t);
extern wchar_t *xwsnchr(const wchar_t *, const wint_t, const size_t);
extern void xwcsninv(wchar_t *, ssize_t, ssize_t);

extern wchar_t *xmemwchar(wchar_t *, wchar_t, ssize_t);

extern void xcp(char *, char *, off_t);

#ifdef DEBUG
#define	ASSERT(x) assert(x)
#else
#define	ASSERT(x)
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILITY_H */
