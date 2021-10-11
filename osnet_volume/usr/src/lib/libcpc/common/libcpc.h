/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_LIBCPC_H
#define	_LIBCPC_H

#pragma ident	"@(#)libcpc.h	1.1	99/08/15 SMI"

#include <sys/types.h>
#include <sys/cpc_event.h>
#include <inttypes.h>
#include <libpctx.h>
#include <stdarg.h>
#include <signal.h>
#include <string.h>
#include <ucontext.h>

/*
 * This library allows hardware performance counters present in
 * certain processors to be used by applications to monitor their
 * own statistics, or the statistics of others.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Library versioning support (c.f. elf_version(3e)).
 *
 * You can enquire about the version number of the library
 * by passing CPC_VER_NONE.  CPC_VER_CURRENT is the current
 * (most capable) version.
 *
 * You can set the version used by the library by passing the
 * required version number.  If this is not possible, the version
 * returned will be CPC_VER_NONE.
 */
#define	CPC_VER_CURRENT		1
#define	CPC_VER_NONE		0

extern uint_t cpc_version(uint_t ver);

/*
 * Utilities to help name counter events
 */
extern int cpc_getcpuver(void);
extern const char *cpc_getcciname(int cpuver);
extern const char *cpc_getcpuref(int cpuver);
extern const char *cpc_getusage(int cpuver);
extern uint_t cpc_getnpic(int cpuver);
extern void cpc_walk_names(int cpuver, int regno, void *arg,
    void (*action)(void *arg, int regno, const char *name, uint8_t bits));

/*
 * A vprintf-like error handling routine can be passed to the
 * library for use by more sophisticated callers.
 * If specified as NULL, errors are written to stderr.
 */
typedef void (cpc_errfn_t)(const char *fn, const char *fmt, va_list ap);
extern void cpc_seterrfn(cpc_errfn_t *errfn);

/*
 * Events can be converted to and from a string representation
 */
extern int cpc_strtoevent(int cpuver, const char *spec, cpc_event_t *event);
extern char *cpc_eventtostr(cpc_event_t *event);

/*
 * Utilities - accumulate and difference events
 */
extern void cpc_event_accum(cpc_event_t *accum, cpc_event_t *event);
extern void cpc_event_diff(cpc_event_t *diff,
    cpc_event_t *left, cpc_event_t *right);

/*
 * Test to see if performance counters are accessible from
 * the current process?
 */
extern int cpc_access();

/*
 * Bind an event to the underlying hardware counters.
 * The counters are set running by the act of binding.
 * Specifying a null event unbinds the event from the
 * underlying hardware.
 *
 * Various flags (defined in <sys/cpc_event.h>) can be set to
 * modify the behaviour of this API.
 *
 * If the flag CPC_BIND_EMT_OVF is set, the process will be sent
 * a SIGEMT signal with a siginfo si_code field set to EMT_CPCOVF
 * when the counters overflow.
 *
 * Note that different processors have different counter ranges available
 * though all supported processors allow at least 31 bits to be counted.
 * If the processor cannot detect counter overflow, this call will fail
 * with EINVAL.
 *
 * If the flag CPC_BIND_LWP_INHERIT is set, all lwps created by the
 * current lwp will automatically inherit the counter context and thus
 * the event being counted by the current lwp.
 */
extern int cpc_bind_event(cpc_event_t *event, int flags);

/*
 * Sample the counters associated with the currently bound event.
 */
extern int cpc_take_sample(cpc_event_t *event);

/*
 * Handy routines for instrumenting code blocks
 */
extern int cpc_count_usr_events(int enable);
extern int cpc_count_sys_events(int enable);

/*
 * Release the storage associated with the current lwp's context
 */
extern int cpc_rele(void);

/*
 * Routines that use libpctx to manage the counters in other processes
 */
extern int cpc_pctx_bind_event(pctx_t *pctx,
    id_t lwpid, cpc_event_t *event, int flags);
extern int cpc_pctx_take_sample(pctx_t *pctx, id_t lwpid, cpc_event_t *event);
extern int cpc_pctx_rele(pctx_t *pctx, id_t lwpid);
extern int cpc_pctx_invalidate(pctx_t *pctx, id_t lwpid);

/*
 * Get access to system-wide CPU performance counters.
 */
extern int cpc_shared_open(void);
extern void cpc_shared_close(int fd);
extern int cpc_shared_bind_event(int fd, cpc_event_t *event, int flags);
extern int cpc_shared_take_sample(int fd, cpc_event_t *event);
extern int cpc_shared_rele(int fd);

#ifdef __cplusplus
}
#endif

#endif	/* _LIBCPC_H */
