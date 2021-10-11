
/* @(#)thread_db.h 1.6 94/11/02 SMI */
/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#ifndef thread_db_h
#define	thread_db_h


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Interface to "thread_db.o"
 *
 */


#include <thread.h>


/*
 * For version checking
 */
#define	THREAD_DB_VERS 2

int 	thread_db_vers();


/*
 * Error stuff
 * A simple 0/-1 scheme isn`'t enoght because some routines need to return
 * more than one kind of status.
 */

typedef enum  {
	TDB_ERR,	/* generic error */
	TDB_OK,		/* generic "call succeeded" */
	TDB_NOTHREAD,	/* no libthread symbols were found by thread_db_sync */
} thread_db_err_e;

extern char thread_db_err[];


/*
 * Initialization stuff
 * Including interface to help thread_db find out locations of crucial
 * global symbols
 */
thread_db_err_e	thread_db_init();
thread_db_err_e	thread_db_sync();



/*
 * Derive a thread from a virtual cpu
 */

thread_db_err_e	thread_db_from_vcpu(struct VCpu_t *, thread_t *);


/*
 * Validate the given thread id
 */
int	thread_db_valid(thread_t);


/*
 * Iteration over threads.
 * 'thread_db_iter' will iterate over all threads and call the callback
 * function (of type thread_iter_f) passing 'client_data' through.
 * The count of threads will be returned. (-1 if any errors)
 * 'cb' can be NULL, in which case only a count is returned.
 */

typedef void thread_iter_f(thread_t, void *client_data);

int	thread_db_iter(thread_iter_f *cb, void *client_data);



/*
 * Thread status (SHOULD be adjusted)
 * All pointers are in user space
 */

typedef enum {
    thread_SLEEP,
    thread_RUN,
    thread_ONPROC,
    thread_ZOMB,
    thread_STOPPED,
    thread_UNKNOWN
} thread_state_e;

typedef struct thread_status {
    thread_state_e
		state;
    int		lwpid;		/* 0 if none */
    unsigned	flags;		/* passed at creation time */
    unsigned	sleep_addr;	/* what's being slept on */
} thread_status_t;

thread_db_err_e	thread_db_status(thread_t, thread_status_t *);


/*
 * Miscellaneous information
 */

thread_db_err_e	thread_db_lwpid(thread_t, int *lidp);
thread_db_err_e	thread_db_pc(thread_t, struct Proc_t *, unsigned *pcp);


/*
 * Regsiter access.
 */
thread_db_err_e	thread_db_get_regs(thread_t, struct RegGSet_t *,
    struct Proc_t *);
thread_db_err_e thread_db_get_fregs(thread_t, struct RegFSet_t *,
    struct Proc_t *);
thread_db_err_e	thread_db_set_regs(thread_t, struct RegGSet_t *,
    struct Proc_t *);
thread_db_err_e thread_db_set_fregs(thread_t, struct RegFSet_t *,
    struct Proc_t *);

/*
 * Leak Detection
 */
thread_db_err_e thread_db_stksegment(thread_t tid, stack_t *stk);


/*
 * Event mgmt
 */
thread_db_err_e	thread_db_notify_on_death(thread_t);


#ifdef __cplusplus
}
#endif

#endif	/* thread_db_h */
