/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ulockf.c	1.14	97/07/18 SMI"	/* SVr4.0 1.1	*/

#include "uucp.h"
#include <rpc/trace.h> 

#include <unistd.h>
/* #include <sys/types.h> */
/* #include <sys/stat.h> */

#ifdef	V7
#define O_RDONLY	0
#endif

#undef _STAT
#undef _FSTAT

#define _STAT stat64
#define _FSTAT fstat64

static void	stlock();
static int	onelock();

/*
 * make a lock file with given 'name'
 * If one already exists, send a signal 0 to the process--if
 * it fails, then unlink it and make a new one.
 *
 * input:
 *	name - name of the lock file to make
 *
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */

GLOBAL int
mklock(name)
register char *name;
{
	static	char pid[SIZEOFPID+2] = { '\0' }; /* +2 for '\n' and NULL */
	static char *tempfile;

#ifdef V8
	char *cp;
#endif
	trace1(TR_mklock, 0);
	if (pid[0] == '\0') {
		tempfile = (char *)malloc(MAXNAMESIZE);
		if (tempfile == NULL)
			return (FAIL);
		(void) sprintf(pid, "%*ld\n", SIZEOFPID, (long) getpid());
		(void) sprintf(tempfile, "%s/LTMP.%ld", X_LOCKDIR, (long) getpid());
	}

#ifdef V8	/* this wouldn't be a problem if we used lock directories */
		/* some day the truncation of system names will bite us */
	cp = rindex(name, '/');
	if (cp++ != CNULL)
		if (strlen(cp) > MAXBASENAME)
		*(cp+MAXBASENAME) = NULLCHAR;
#endif /* V8 */
	if (onelock(pid, tempfile, name) == -1) {
		(void) unlink(tempfile);
		if (cklock(name)) {
			trace1(TR_mklock, 1);
			return (FAIL);
		}				
		else {
			if (onelock(pid, tempfile, name)) {
			(void) unlink(tempfile);
			DEBUG(4,"ulockf failed in onelock()\n%s", "");
			trace1(TR_mklock, 1);
			return (FAIL);
			}
		}
	}
	stlock(name);
	trace1(TR_mklock, 1);
	return (0);
}

/*
 * check to see if the lock file exists and is still active
 * - use kill(pid,0)
 *
 * return:
 *	0	-> success (lock file removed - no longer active
 *	FAIL	-> lock file still active
 */
GLOBAL int
cklock(name)
register char *name;
{
	register int ret;
	pid_t lpid = -1;
	char alpid[SIZEOFPID+2];	/* +2 for '\n' and NULL */
	int fd;

	trace1(TR_cklock, 0);
	fd = open(name, O_RDONLY);
	DEBUG(4, "ulockf name %s\n", name);
	if (fd == -1) {
		if (errno == ENOENT) {	/* file does not exist -- OK */
		trace1(TR_cklock, 1);
		return (0);
		}
		DEBUG(4,"Lock File--can't read (errno %d) --remove it!\n", errno);
		goto unlk;
	}
	ret = read(fd, (char *) alpid, SIZEOFPID+1); /* +1 for '\n' */
	(void) close(fd);
	if (ret != (SIZEOFPID+1)) {

		DEBUG(4, "Lock File--bad format--remove it!\n%s", "");
		goto unlk;
	}
	lpid = (pid_t) strtol(alpid, (char **) NULL, 10);
	if ((ret=kill(lpid, 0)) == 0 || errno == EPERM) {
		DEBUG(4, "Lock File--process still active--not removed\n%s", "");
		trace1(TR_cklock, 1);
		return (FAIL);
	}
	else { /* process no longer active */
		/*EMPTY*/
		DEBUG(4, "kill pid (%ld), ", (long) lpid);
		DEBUG(4, "returned %d", ret);
		DEBUG(4, "--ok to remove lock file (%s)\n", name);
	}
unlk:
	
	if (unlink(name) != 0) {
		DEBUG(4,"ulockf failed in unlink()\n%s", "");
			trace1(TR_cklock, 1);
		return (FAIL);
	}
	trace1(TR_cklock, 1);
	return (0);
}

#define MAXLOCKS 10	/* maximum number of lock files */
static char *Lockfile[MAXLOCKS];
GLOBAL int Nlocks = 0;

/*
 * put name in list of lock files
 * return:
 *	none
 */
static void
stlock(name)
char *name;
{
	register int i;
	char *p;

	trace1(TR_stlock, 0);
	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			break;
	}
	ASSERT(i < MAXLOCKS, "TOO MANY LOCKS", "", i);
	if (i >= Nlocks)
		i = Nlocks++;
	p = (char*) calloc((unsigned) strlen(name) + 1, sizeof (char));
	ASSERT(p != NULL, "CAN NOT ALLOCATE FOR", name, 0);
	(void) strcpy(p, name);
	Lockfile[i] = p;
	trace1(TR_stlock, 1);
	return;
}

/*
 * remove the named lock. If named lock is NULL,
 *	then remove all locks currently in list.
 * return:
 *	none
 */
GLOBAL void
rmlock(name)
register char *name;
{
	register int i;
#ifdef V8
	char *cp;
#endif /* V8 */

	trace1(TR_rmlock, 0);
#ifdef V8
	cp = rindex(name, '/');
	if (cp++ != CNULL)
		if (strlen(cp) > MAXBASENAME)
		*(cp+MAXBASENAME) = NULLCHAR;
#endif /* V8 */

	for (i = 0; i < Nlocks; i++) {
		if (Lockfile[i] == NULL)
			continue;
		if (name == NULL || EQUALS(name, Lockfile[i])) {
			(void) unlink(Lockfile[i]);
			free(Lockfile[i]);
			Lockfile[i] = NULL;
		}
	}
	trace1(TR_rmlock, 1);
	return;
}

/*
 * remove a lock file
 *
 * Parameters:
 *	pre -	Path and first part of file name of the lock file to be
 *		removed.
 *	s -	The suffix part of the lock file.  The name of the lock file
 *		will be derrived by concatenating pre, a period, and s.
 *
 * return:
 *	none
 */
GLOBAL void
delock(pre, s)
char * pre;
char *s;
{
	char ln[MAXNAMESIZE];

	trace1(TR_delock, 0);
	(void) sprintf(ln, "%s.%s", pre, s);
	BASENAME(ln, '/')[MAXBASENAME] = '\0';
	rmlock(ln);
	trace1(TR_delock, 1);
	return;
}

/*
 * create lock file
 *
 * Parameters:
 *	pre -	Path and first part of file name of the lock file to be
 *		created.
 *	name -	The suffix part of the lock file.  The name of the lock file
 *		will be derrived by concatenating pre, a period, and name.
 *
 * return:
 *	0	-> success
 *	FAIL	-> failure
 */
GLOBAL int
mlock(pre, name)
char * pre;
char *name;
{
	char lname[MAXNAMESIZE];
	int  dummy;

	trace1(TR_mlock, 0);
	/*
	 * if name has a '/' in it, then it's a device name and it's
	 * not in /dev (i.e., it's a remotely-mounted device or it's
	 * in a subdirectory of /dev).  in either case, creating our normal 
	 * lockfile (/var/spool/locks/LCK..<dev>) is going to bomb if
	 * <dev> is "/remote/dev/term/14" or "/dev/net/foo/clone", so never 
	 * mind.  since we're using advisory filelocks on the devices 
	 * themselves, it'll be safe.
	 *
	 * of course, programs and people who are used to looking at the
	 * lockfiles to find out what's going on are going to be a trifle
	 * misled.  we really need to re-consider the lockfile naming structure
	 * to accomodate devices in directories other than /dev ... maybe in
	 * the next release.
	 */
	if (strchr(name, '/') != NULL) {
		trace1(TR_mlock, 1);
		return (0);
	}
	(void) sprintf(lname, "%s.%s", pre, BASENAME(name, '/'));
	BASENAME(lname, '/')[MAXBASENAME] = '\0';
	dummy = mklock(lname);
	trace1(TR_mlock, 1);
	return (dummy);
}

/*
 * makes a lock on behalf of pid.
 * input:
 *	pid - process id
 *	tempfile - name of a temporary in the same file system
 *	name - lock file name (full path name)
 * return:
 *	-1 - failed
 *	0  - lock made successfully
 */
static int
onelock(pid,tempfile,name)
char *pid;
char *tempfile, *name;
{	
	register int fd;
	char	cb[100];

	trace1(TR_onelock, 0);
	fd=creat(tempfile, (mode_t) 0444);
	if (fd < 0){
		(void) sprintf(cb, "%s %s %d",tempfile, name, errno);
		logent("ULOCKC", cb);
		if ((errno == EMFILE) || (errno == ENFILE))
			(void) unlink(tempfile);
		trace1(TR_onelock, 1);
		return (-1);
	}
	/* +1 for '\n' */
	if (write(fd, pid, SIZEOFPID+1) != (SIZEOFPID+1)) {
		(void) sprintf(cb, "%s %s %d",tempfile, name, errno);
		logent("ULOCKW", cb);
		(void) unlink(tempfile);
		return (-1);
	}
	(void) chmod(tempfile, (mode_t) 0444);
	(void) chown(tempfile, UUCPUID, UUCPGID);
	(void) close(fd);
	if (link(tempfile,name)<0){
		DEBUG(4, "%s: ", strerror(errno));
		DEBUG(4, "link(%s, ", tempfile);
		DEBUG(4, "%s)\n", name);
		if (unlink(tempfile)< 0){
			(void) sprintf(cb, "ULK err %s %d", tempfile,  errno);
			logent("ULOCKLNK", cb);
		}
		trace1(TR_onelock, 1);
		return (-1);
	}
	if (unlink(tempfile)<0){
		(void) sprintf(cb, "%s %d",tempfile,errno);
		logent("ULOCKF", cb);
	}
	trace1(TR_onelock, 1);
	return (0);
}

/*
 * fd_mklock(fd) - lock the device indicated by fd is possible
 *
 * return -
 *	SUCCESS - this process now has the fd locked
 *	FAIL - this process was not able to lock the fd
 */

GLOBAL int
fd_mklock(fd)
int fd;
{
	int tries = 0;
	struct stat64 _st_buf;
	char lockname[BUFSIZ];

	trace2(TR_fd_mklock, 0, fd);
	if (_FSTAT(fd, &_st_buf) != 0) {
		trace1(TR_fd_mklock, 1);
		return (FAIL);
	}

	(void) sprintf(lockname, "%s.%3.3lu.%3.3lu.%3.3lu", L_LOCK,
		(unsigned long) major(_st_buf.st_dev),
		(unsigned long) major(_st_buf.st_rdev),
		(unsigned long) minor(_st_buf.st_rdev));

	if (mklock(lockname) == FAIL) {
		trace1(TR_fd_mklock, 1);
		return (FAIL);
	}
 
	while (lockf(fd, F_TLOCK, 0L) != 0) {	
		DEBUG(7, "fd_mklock: lockf returns %d\n", errno);
		if ((++tries >= MAX_LOCKTRY) || (errno != EAGAIN)) {
			rmlock(lockname);
			logent("fd_mklock","lockf failed");
			trace1(TR_fd_mklock, 1);
			return (FAIL);
		}
		sleep(2);
	}
	DEBUG(7, "fd_mklock: ok\n%s", "");
	trace1(TR_fd_mklock, 1);
	return (SUCCESS);
}

/*
 * fn_cklock(name) - determine if the device indicated by name is locked
 *
 * return -
 *	SUCCESS - the name is not locked
 *	FAIL - the name is locked by another process
 */

GLOBAL int
fn_cklock(name)
char *name;
{
	int dummy;
	struct stat64 _st_buf;
	char lockname[BUFSIZ];

	trace1(TR_fn_cklock, 0);
	/* we temporarily use lockname to hold full path name */
	(void) sprintf(lockname, "%s%s", (*name == '/' ? "" : "/dev/"), name);

	if (_STAT(lockname, &_st_buf) != 0) {
		trace1(TR_fn_cklock, 1);
		return (FAIL);
	}

	(void) sprintf(lockname, "%s.%3.3lu.%3.3lu.%3.3lu", L_LOCK,
		(unsigned long) major(_st_buf.st_dev),
		(unsigned long) major(_st_buf.st_rdev),
		(unsigned long) minor(_st_buf.st_rdev));

	dummy = cklock(lockname);
	trace1(TR_fn_cklock, 1);
	return (dummy);
}

/*
 * fd_cklock(fd) - determine if the device indicated by fd is locked
 *
 * return -
 *	SUCCESS - the fd is not locked
 *	FAIL - the fd is locked by another process
 */

GLOBAL int
fd_cklock(fd)
int fd;
{
	struct stat64 _st_buf;
	char lockname[BUFSIZ];

	trace2(TR_fd_cklock, 0, fd);
	if (_FSTAT(fd, &_st_buf) != 0) {
		trace1(TR_fd_cklock, 1);
		return (FAIL);
	}

	(void) sprintf(lockname, "%s.%3.3lu.%3.3lu.%3.3lu", L_LOCK,
		(unsigned long) major(_st_buf.st_dev),
		(unsigned long) major(_st_buf.st_rdev),
		(unsigned long) minor(_st_buf.st_rdev));

	if (cklock(lockname) == FAIL) {
		trace1(TR_fd_cklock, 1);
		return (FAIL);
	} else {
		trace1(TR_fd_cklock, 1);
		return (lockf(fd, F_TEST, 0L));
	}
}

/*
 * remove the locks associated with the device file descriptor
 *
 * return -
 *	SUCCESS - both BNU lock file and advisory locks removed
 *	FAIL - 
 */

GLOBAL void
fd_rmlock(fd)
int fd;
{
	struct stat64 _st_buf;
	char lockname[BUFSIZ];

	trace2(TR_fd_rmlock, 0, fd);
	if (_FSTAT(fd, &_st_buf) == 0) {
		(void) sprintf(lockname, "%s.%3.3lu.%3.3lu.%3.3lu", L_LOCK,
			(unsigned long) major(_st_buf.st_dev),
			(unsigned long) major(_st_buf.st_rdev),
			(unsigned long) minor(_st_buf.st_rdev));
		rmlock(lockname);
	}
	(void) lockf(fd, F_ULOCK, 0L);
	trace1(TR_fd_rmlock, 1);
	return;
}
