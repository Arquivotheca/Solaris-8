/*
 *
 *			fscache.h
 *
 * Include file for the fscache class.
 */

#ident   "@(#)cfsd_fscache.h 1.2     99/05/13 SMI"
/* Copyright (c) 1994 by Sun Microsystems, Inc. */

#ifndef CFSD_FSCACHE
#define	CFSD_FSCACHE

typedef struct cfsd_fscache_object {
	char	i_name[MAXNAMELEN];		/* fscache name */
	char	i_cachepath[MAXPATHLEN];	/* cache pathname */
	int	i_fscacheid;			/* fscache identifier */

	char	i_mntpt[MAXPATHLEN];		/* mount point */
	char	i_backfs[MAXPATHLEN * 2];	/* back file system */
	char	i_backpath[MAXPATHLEN];		/* back file system path */
	char	i_backfstype[MAXNAMELEN];	/* back file system type */
	char	i_cfsopt[MAXPATHLEN * 4];	/* cachefs mount options */
	char	i_bfsopt[MAXPATHLEN * 4];	/* backfs mount options */

	mutex_t		i_lock;			/* synchronizing lock */
	int		i_refcnt;		/* refs to object */
	volatile int	i_disconnectable:1;	/* 1 if okay to disconnect */
	volatile int	i_mounted:1;		/* 1 if fs is mounted */
	volatile int	i_threaded:1;		/* 1 if thread running */
	volatile int	i_connected:1;		/* 1 if connected */
	volatile int	i_reconcile:1;		/* 1 if reconciling */
	volatile int	i_changes:1;		/* 1 if changes to push back */
	volatile int	i_simdis:1;		/* 1 means sim disconnect */
	volatile int	i_tryunmount:1;		/* 1 if should try unmount */
	volatile int	i_backunmount:1;	/* 1 if need to umount backfs */
	time_t		i_time_state;		/* time of last dis/connect */
	time_t		i_time_mnt;		/* time of last u/mount */
	int		i_modify;		/* changed when modified */

	int		i_ofd;			/* message file descriptor */

	thread_t	i_threadid;		/* id of thread, if running */
	cond_t		i_cvwait;		/* cond var to wait on */

	off_t		i_again_offset;		/* offset to head modify op */
	int		i_again_seq;		/* seq num of head modify op */
	struct cfsd_fscache_object *i_next;	/* next fscache object */
} cfsd_fscache_object_t;

cfsd_fscache_object_t *cfsd_fscache_create(const char *name,
    const char *cachepath, int fscacheid);
void cfsd_fscache_destroy(cfsd_fscache_object_t *fscache_object_p);

void fscache_lock(cfsd_fscache_object_t *fscache_object_p);
void fscache_unlock(cfsd_fscache_object_t *fscache_object_p);

void fscache_setup(cfsd_fscache_object_t *fscache_object_p);
void fscache_process(cfsd_fscache_object_t *fscache_object_p);
int fscache_simdisconnect(cfsd_fscache_object_t *fscache_object_p,
    int disconnect);
int fscache_unmount(cfsd_fscache_object_t *fscache_object_p, int);
void fscache_server_alive(cfsd_fscache_object_t *fscache_object_p,
    cfsd_kmod_object_t *kmod_object_p);
int fscache_pingserver(cfsd_fscache_object_t *fscache_object_p);
int fscache_roll(cfsd_fscache_object_t *fscache_object_p,
    cfsd_kmod_object_t *kmod_object_p);
int fscache_rollone(cfsd_fscache_object_t *fscache_object_p,
    cfsd_kmod_object_t *kmod_object_p,
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    u_long seq);
int fscache_addagain(cfsd_fscache_object_t *fscache_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    u_long nseq);
void fscache_fsproblem(cfsd_fscache_object_t *fscache_object_p,
    cfsd_kmod_object_t *kmod_object_p);
void fscache_changes(cfsd_fscache_object_t *fscache_object_p, int tt);



#endif /* CFSD_FSCACHE */
