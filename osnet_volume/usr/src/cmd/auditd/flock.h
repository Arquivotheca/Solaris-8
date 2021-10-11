/* @(#)flock.h 93/02/16 SMI */

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Header file for flock() which is not in SYS V libraries.
 */

/* These definitions no longer appear in <sys/file.h> */

#define	  LOCK_SH   1    /* shared lock */
#define	  LOCK_EX   2    /* exclusive lock */
#define	  LOCK_NB   4    /* don't block when locking */
#define	  LOCK_UN   8    /* unlock */

extern int	flock();
