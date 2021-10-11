/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBPCTX_H
#define	_LIBPCTX_H

#pragma ident	"@(#)libpctx.h	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <fcntl.h>
#include <stdarg.h>

/*
 * The process context library allows callers to use the facilities
 * of /proc to control processes in a simplified way by managing
 * the process via an event loop.  The controlling process expresses
 * interest in various events which are handled as callbacks by the
 * library.
 */

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct __pctx pctx_t;

/*
 * A vprintf-like error handling routine can be passed in for use
 * by more sophisticated callers.  If specified as NULL, errors
 * are written to stderr.
 */
typedef void (pctx_errfn_t)(const char *fn, const char *fmt, va_list ap);

extern pctx_t *pctx_create(const char *filename, char *const *argv,
    void *arg, int verbose, pctx_errfn_t *errfn);
extern pctx_t *pctx_capture(pid_t pid,
    void *arg, int verbose, pctx_errfn_t *errfn);

typedef int pctx_sysc_execfn_t(pctx_t *, pid_t, id_t, char *, void *);
typedef void pctx_sysc_forkfn_t(pctx_t *, pid_t, id_t, pid_t, void *);
typedef void pctx_sysc_exitfn_t(pctx_t *, pid_t, id_t, int, void *);
typedef int pctx_sysc_lwp_createfn_t(pctx_t *, pid_t, id_t, void *);
typedef int pctx_init_lwpfn_t(pctx_t *, pid_t, id_t, void *);
typedef int pctx_fini_lwpfn_t(pctx_t *, pid_t, id_t, void *);
typedef int pctx_sysc_lwp_exitfn_t(pctx_t *, pid_t, id_t, void *);

typedef	enum {
	PCTX_NULL_EVENT = 0,
	PCTX_SYSC_EXEC_EVENT,
	PCTX_SYSC_FORK_EVENT,
	PCTX_SYSC_EXIT_EVENT,
	PCTX_SYSC_LWP_CREATE_EVENT,
	PCTX_INIT_LWP_EVENT,
	PCTX_FINI_LWP_EVENT,
	PCTX_SYSC_LWP_EXIT_EVENT
} pctx_event_t;

extern int pctx_set_events(pctx_t *pctx, ...);

extern int pctx_run(pctx_t *pctx, uint_t msec, uint_t nsamples,
    int (*tick)(pctx_t *, pid_t, id_t, void *));

extern void pctx_release(pctx_t *pctx);

/*
 * Implementation-private system call used by libcpc
 */
extern int __pctx_cpc(pctx_t *pctx,
    int cmd, id_t lwpid, void *data, int flags, size_t size);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBPCTX_H */
