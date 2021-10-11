/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _EXCEPTION_H
#define	_EXCEPTION_H

#pragma ident	"@(#)exception.h	1.4	99/05/07 SMI"

#include <jni.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This really doesn't belong here, but rather than create another whole
 * header file just for this macro, we stuck it here.
 */
#define	ARRAY_LENGTH(arr, len)	for (len = 0; arr[len] != NULL; ++len);

extern void throw(JNIEnv *, const char *);
extern void throw_memory_exception(JNIEnv *);
extern void throw_exists_exception(JNIEnv *, const char *);
extern void throw_noent_exception(JNIEnv *, const char *);
extern void throw_not_running_exception(JNIEnv *);
extern void throw_host_exists_exception(JNIEnv *, const char *);
extern void throw_host_noent_exception(JNIEnv *, const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* !_EXCEPTION_H */
