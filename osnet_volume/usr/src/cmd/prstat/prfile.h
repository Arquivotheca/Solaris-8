/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PRFILE_H
#define	_PRFILE_H

#pragma ident	"@(#)prfile.h	1.2	99/09/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAX_PROCFS_PATH	40
#define	NUM_RESERVED_FD	10

typedef struct fd {
	int	fd_fd;
	int	fd_flags;
	char	fd_name[MAX_PROCFS_PATH];
} fd_t;

typedef struct fds {
	pid_t	fds_pid;
	int	fds_msacct;
	fd_t	*fds_psinfo;
	fd_t	*fds_usage;
	fd_t	*fds_lpsinfo;
	fd_t	*fds_lusage;
	struct fds *fds_next;
} fds_t;

extern void fd_init(int);
extern void fd_exit();
extern fd_t *fd_open(char *, int, fd_t *);
extern int fd_getfd(fd_t *);
extern void fd_close(fd_t *);
extern void fd_closeall();
extern void fd_update();
extern fds_t *fds_get(pid_t);
extern void fds_rm(pid_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _PRFILE_H */
