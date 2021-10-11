/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef _SYS_AUDIODEBUG_H
#define	_SYS_AUDIODEBUG_H

#pragma ident	"@(#)audiodebug.h	1.5	93/02/04 SMI"

/*
 * Audio debugging macros
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(AUDIOTRACE) || defined(DBRITRACE)

#ifndef NAUDIOTRACE
#define	NAUDIOTRACE 1024
#endif

struct audiotrace {
	int count;
	int function;		/* address of function */
	int trace_action;	/* descriptive 4 characters */
	int object;		/* object operated on */
};

extern struct audiotrace audiotrace_buffer[];
extern struct audiotrace *audiotrace_ptr;
extern int audiotrace_count;

#define	ATRACEINIT() {				\
	if (audiotrace_ptr == NULL)		\
		audiotrace_ptr = audiotrace_buffer; \
	}

#define	LOCK_TRACE()	(uint_t) ddi_enter_critical()
#define	UNLOCK_TRACE(x)	ddi_exit_critical((uint_t) x)

#if defined(AUDIOTRACE)
#define	ATRACE(func, act, obj) {		\
	int __s = LOCK_TRACE();			\
	int *_p = &audiotrace_ptr->count;	\
	*_p++ = ++audiotrace_count;		\
	*_p++ = (int)(func);			\
	*_p++ = (int)(act);			\
	*_p++ = (int)(obj);			\
	if ((struct audiotrace *)(void *)_p >= &audiotrace_buffer[NAUDIOTRACE])\
		audiotrace_ptr = audiotrace_buffer; \
	else					\
		audiotrace_ptr = (struct audiotrace *)(void *)_p; \
	UNLOCK_TRACE(__s);			\
	}
#else
#define	ATRACE(a, b, c)
#endif

#if defined(DBRITRACE)
#define	DTRACE(func, act, obj) {		\
	int __s = LOCK_TRACE();			\
	int *_p = &audiotrace_ptr->count;	\
	*_p++ = ++audiotrace_count;		\
	*_p++ = (int)(func);			\
	*_p++ = (int)(act);			\
	*_p++ = (int)(obj);			\
	if ((struct audiotrace *)(void *)_p >= &audiotrace_buffer[NAUDIOTRACE])\
		audiotrace_ptr = audiotrace_buffer; \
	else					\
		audiotrace_ptr = (struct audiotrace *)(void *)_p; \
	UNLOCK_TRACE(__s);			\
	}
#else
#define	DTRACE(a, b, c)
#endif

#else	/* !AUDIOTRACE */

/* If no tracing, define no-ops */
#define	ATRACEINIT()
#define	ATRACE(a, b, c)
#define	DTRACE(a, b, c)

#endif	/* !AUDIOTRACE */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_AUDIODEBUG_H */
