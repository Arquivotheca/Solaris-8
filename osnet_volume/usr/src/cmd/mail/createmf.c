/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)createmf.c	1.6	93/08/19 SMI" 	/* SVr4.0 2.	*/
#include "mail.h"
/*
	If mail file does not exist create it 
*/
#ifdef OLD
void createmf(uid, file)
uid_t uid;
char *file;
{
	int fd;

	void (*istat)(), (*qstat)(), (*hstat)();

	if (access(file, A_EXIST) == CERROR) {
		istat = signal(SIGINT, SIG_IGN);
		qstat = signal(SIGQUIT, SIG_IGN);
		hstat = signal(SIGHUP, SIG_IGN);
		umask(0);
		if ((fd = creat(file, MFMODE)) == -1)
			sav_errno = errno;
		else
			close(fd);
		umask(7);
		(void) signal(SIGINT, istat);
		(void) signal(SIGQUIT, qstat);
		(void) signal(SIGHUP, hstat);
	}
}
#else

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

int accessmf(path)
char *path;
{

struct stat fsb,sb;
int mbfd;
tryagain:
	if (lstat(path, &sb)) { 
		/* file/symlink does not exist, so create one */
		mbfd = open(path,
		    O_APPEND|O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		chmod(path, 0660); 
		/* if someone create a symlink/file just ahead */
		/* of us, the create will failed with EEXIST   */
		/* This is what we want, because we do not     */
		/* want someone to re-direct our "create"      */
	        /* request to a another location.              */
		if (mbfd == -1) {
			if (errno == EEXIST)
				goto tryagain;
		} 

	/* file/symlink  exist, make sure it is not linked */
	} else if (sb.st_nlink != 1 || S_ISLNK(sb.st_mode)) {
		fprintf(stderr, 
"%s: security violation, '%s' should not be linked to other file\n", program, path);
		sav_errno = errno;
		return -1;
	} else {
		/* if we get here, there is a pre-existing file, */
		/* and it is not a symlink...			 */
		/* open it, and make sure it is the same file    */
		/* we lstat() before...                          */
		/* this is to guard against someone deleting the */
		/* old file and creat a new symlink in its place */
		/* We are not createing a new file here, but we  */	
		/* do not want append to the worng file either   */
		mbfd = open(path, O_APPEND|O_WRONLY, 0);
		if (mbfd != -1 &&
		    (fstat(mbfd, &fsb) || fsb.st_nlink != 1 ||
		    S_ISLNK(fsb.st_mode) || sb.st_dev != fsb.st_dev ||
		    sb.st_ino != fsb.st_ino)) {
			/*  file changed after open */
			fprintf(stderr, "%s: security violation, '%s' inode changed after open\n", program, path);
			(void)close(mbfd);
			return -1;
		}
	}

	if (mbfd == -1) {
		sav_errno = errno;
		return -1;
	}
	
	return mbfd;
}
#endif
