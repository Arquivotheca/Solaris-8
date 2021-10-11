/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

/*
 * This is the header where the internal to libc definition of the FILE
 * structure is defined. The exrernal defintion defines the FILE structure
 * as an array of longs. This prevents customers from writing code that
 * depends upon the implemnetation of stdio. The __fbufsize(3C) man page
 * documents a set of routines that customers can use so that they do not
 * need access to the FILE structure.
 *
 * When compiling libc this file MUST be included BEFORE <stdio.h>, and
 * any other headers that themselves directly or indirectly include
 * <stdio.h>. Failure to do so, will cause the compile of libc to fail,
 * since the structure members will not be visible to the stdio routines.
 */


#ifndef	_FILE64_H
#define	_FILE64_H

#pragma ident	"@(#)file64.h	1.12	99/06/10 SMI"

#include <thread.h>
#include <synch.h>
#include <stdio_tag.h>

typedef struct {
	mutex_t	_mutex;		/* protects all the fields in this struct */
	cond_t	_cond;
	unsigned short	_wait_cnt;
	unsigned short	_lock_cnt;
	thread_t	_owner;
} rmutex_t;

#ifdef	_LP64

#include <wchar_impl.h>

#ifndef	_MBSTATE_T
#define	_MBSTATE_T
typedef __mbstate_t	mbstate_t;
#endif

struct __FILE_TAG {
	unsigned char	*_ptr;	/* next character from/to here in buffer */
	unsigned char	*_base;	/* the buffer */
	unsigned char	*_end;	/* the end of the buffer */
	ssize_t		_cnt;	/* number of available characters in buffer */
	int		_file;	/* UNIX System file descriptor */
	unsigned int	_flag;	/* the state of the stream */
	rmutex_t	_lock;	/* lock for this structure */
	mbstate_t	*_state;	/* mbstate_t */
	char		__fill[32];	/* filler to bring size to 128 bytes */
};

#endif	/*	_LP64	*/

#endif	/* _FILE64_H */
