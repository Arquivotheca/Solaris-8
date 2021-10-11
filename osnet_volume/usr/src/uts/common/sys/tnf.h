/*
 *	Copyright (c) 1994, by Sun Microsytems, Inc.
 *	All rights reserved.
 */

#ifndef _SYS_TNF_H
#define	_SYS_TNF_H

#pragma ident	"@(#)tnf.h	1.16	97/08/06 SMI"

#ifndef NPROBE

#include <sys/types.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/cpuvar.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *
 */

typedef struct {
	ulong_t probenum;
	int enabled;
	int traced;
	int attrsize;
} tnf_probevals_t;

/*
 *
 */

typedef struct {
	enum {
		TIFIOCBUF_NONE,
		TIFIOCBUF_UNINIT,
		TIFIOCBUF_OK,
		TIFIOCBUF_BROKEN
		} buffer_state;
	int buffer_size;
	int trace_stopped;
	int pidfilter_mode;
	int pidfilter_size;
} tifiocstate_t;

typedef struct {
	char *dst_addr;
	int block_num;
} tifiocgblock_t;

typedef struct {
	long *dst_addr;
	int start;
	int slots;
} tifiocgfw_t;

/*
 * ioctl codes
 */

#define	TIFIOCGMAXPROBE		(('t' << 8) | 1) /* get max probe number */
#define	TIFIOCGPROBEVALS	(('t' << 8) | 2) /* get probe info */
#define	TIFIOCGPROBESTRING	(('t' << 8) | 3) /* get probe string */
#define	TIFIOCSPROBEVALS	(('t' << 8) | 4) /* set probe info */
#define	TIFIOCGSTATE		(('t' << 8) | 5) /* get tracing system state */
#define	TIFIOCALLOCBUF		(('t' << 8) | 6) /* allocate trace buffer */
#define	TIFIOCDEALLOCBUF	(('t' << 8) | 7) /* dealloc trace buffer */
#define	TIFIOCSTRACING		(('t' << 8) | 8) /* set ktrace mode */
#define	TIFIOCSPIDFILTER	(('t' << 8) | 9) /* set pidfilter mode */
#define	TIFIOCGPIDSTATE		(('t' << 8) | 10) /* check pid filter member */
#define	TIFIOCSPIDON		(('t' << 8) | 11) /* add pid to filter */
#define	TIFIOCSPIDOFF		(('t' << 8) | 12) /* drop pid from filter */
#define	TIFIOCPIDFILTERGET	(('t' << 8) | 13) /* return pid filter set */
#define	TIFIOCGHEADER		(('t' << 8) | 14) /* copy out tnf header blk */
#define	TIFIOCGBLOCK		(('t' << 8) | 15) /* copy out tnf block */
#define	TIFIOCGFWZONE		(('t' << 8) | 16) /* copy out forwarding ptrs */

#ifdef _KERNEL

extern volatile int tnf_tracing_active;

extern void tnf_thread_create(kthread_t *);
extern void tnf_thread_queue(kthread_t *, cpu_t *, pri_t);
extern void tnf_thread_switch(kthread_t *);
extern void tnf_thread_exit(void);
extern void tnf_thread_free(kthread_t *);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* NPROBE */

#endif /* _SYS_TNF_H */
