/*
 * Copyright (c) 1997 by Sun Microsystems Inc.
 */

#ident	"@(#)lockmap.c	1.1	98/05/11 SMI"

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>

/*
 *  These routines provide mutual exclusion between ypserv and ypxfr.
 *  Mutual exclusion is needed so that ypxfr doesn't try to rename
 *  dbm files while ypserv is trying to open them.  After ypserv has
 *  opened a dbm file, it is safe to rename it because ypserv still
 *  has access to the file through its file descriptor.
 *
 *  To lock a map, we hash the name of the map to a single integer
 *  and lock one byte at that file offset.  Note that the file
 *  is actually always empty.  It is perfectly legal to seek past the
 *  end of the file and place a lock.
 *
 *  The value of 1000 for MAXHASH was chosen by hashing all of the map
 *  names on a typical server and checking for collisions.  This modulus
 *  gave a unique hash value for almost every map.
 */

#define	MAXHASH 1000
#define	LOCKFILE "/var/yp/.maplock"
static int lock_file = -1;

static
int
hash(char *s)
{
	int n = 0;

	while (*s)
		n += *s++;
	return (n % MAXHASH);
}

/*
 *  Hash the map name to determine which byte of the lock file to lock.
 *  Seek to that offset and lock one byte.
 */
int
lock_map(char *mapname)
{
	int hashval;

	if (lock_file == -1) {
		lock_file = open(LOCKFILE, O_RDWR|O_CREAT, 0600);
		if (lock_file == -1) {
			syslog(LOG_ERR, "can't create %s:  %m", LOCKFILE);
			return (0);
		}
	}

	hashval = hash(mapname);

	if (lseek(lock_file, hashval, SEEK_SET) == -1) {
		syslog(LOG_ERR, "can't lseek %s:  %m", LOCKFILE);
		return (0);
	}

	if (lockf(lock_file, F_LOCK, 1) == -1) {
		syslog(LOG_ERR, "can't lock %s:  %m", LOCKFILE);
		return (0);
	}

	return (1);
}

/*
 *  Release a file lock.  We do an lseek before unlocking because
 *  two locks in a row will have moved the current file position.
 */
int
unlock_map(char *mapname)
{
	int hashval;

	if (lock_file == -1) {
		syslog(LOG_ERR, "unlock unlocked file %s", LOCKFILE);
		return (0);
	}

	hashval = hash(mapname);

	if (lseek(lock_file, hashval, SEEK_SET) == -1) {
		syslog(LOG_ERR, "can't lseek %s:  %m", LOCKFILE);
		return (0);
	}

	if (lockf(lock_file, F_ULOCK, 1) == -1) {
		syslog(LOG_ERR, "can't unlock %s:  %m", LOCKFILE);
		return (0);
	}

	return (1);
}
