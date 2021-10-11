/*
 *      Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#ifndef _TNF_PROBE_H
#define	_TNF_PROBE_H

#pragma ident	"@(#)probe.h	1.12	94/12/06 SMI"

#include <tnf/writer.h>
#include <sys/tnf_probe.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Interface for enabling/disabling tracing for the process.
 */
void tnf_process_disable(void);
void tnf_process_enable(void);

/*
 * Interface for enabling/disabling tracing for the calling thread.
 */
void tnf_thread_disable(void);
void tnf_thread_enable(void);

#ifdef	__cplusplus
}
#endif

#endif /* _TNF_PROBE_H */
