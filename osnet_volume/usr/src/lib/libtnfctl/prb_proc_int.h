/*
 * Copyright (c) 1994-1997, by Sun Microsytems, Inc.
 * All rights reserved.
 */

#ifndef _PRB_PROC_INT_H
#define	_PRB_PROC_INT_H

#pragma ident	"@(#)prb_proc_int.h	1.15	98/01/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interfaces private to proc layer
 */

#include <sys/types.h>
#include <sys/syscall.h>

#include <tnf/probe.h>
#include <note.h>

#include "prb_proc.h"

/*
 * size of breakpoint instruction
 */
#if defined(sparc)
typedef unsigned int bptsave_t;
#elif defined(i386)
typedef unsigned char bptsave_t;
#endif

/*
 * memory shared between parent and child when exec'ing a child.
 * child spins on "spin" member waiting for parent to set it free
 */
typedef struct shmem_msg {
	boolean_t	spin;
} shmem_msg_t;
NOTE(SCHEME_PROTECTS_DATA("parent writes; child reads", shmem_msg))

/*
 * per /proc handle state
 */
struct prb_proc_ctl {
	int 		procfd;
	int		pid;
	uintptr_t	bptaddr;
	bptsave_t	saveinstr;	/* instruction that bpt replaced */
	boolean_t	bpt_inserted;	/* is bpt inserted ? */
	uintptr_t	dbgaddr;
};
NOTE(SCHEME_PROTECTS_DATA("one thread per handle", prb_proc_ctl))

/*
 * Declarations
 */
prb_status_t	prb_status_map(int);
prb_status_t	find_executable(const char *name, char *ret_path);

/* shared memory lock interfaces */
prb_status_t	prb_shmem_init(volatile shmem_msg_t **);
prb_status_t	prb_shmem_wait(volatile shmem_msg_t *);
prb_status_t	prb_shmem_clear(volatile shmem_msg_t *);
prb_status_t	prb_shmem_free(volatile shmem_msg_t *smp);

/* runs and stops the process to clear it out of system call */
prb_status_t	prb_proc_prstop(prb_proc_ctl_t *proc_p);

/* break point interfaces */
prb_status_t	prb_proc_tracebpt(prb_proc_ctl_t *proc_p, boolean_t bpt);
prb_status_t	prb_proc_istepbpt(prb_proc_ctl_t *proc_p);
prb_status_t	prb_proc_clrbptflt(prb_proc_ctl_t *proc_p);

/* read a string from target process */
prb_status_t	prb_proc_readstr(prb_proc_ctl_t *proc_p, uintptr_t addr,
			const char **outstr_pp);

#ifdef __cplusplus
}
#endif

#endif	/* _PRB_PROC_INT_H */
