/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FS_S5_FSDIR_H
#define	_SYS_FS_S5_FSDIR_H

#pragma ident	"@(#)s5_fsdir.h	1.2	93/11/01 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	DIRSIZ
#define	DIRSIZ	14
#endif
struct	direct {
	o_ino_t	d_ino;		/* s5 inode type */
	char	d_name[DIRSIZ];
};

struct dirtemplate {
	o_ino_t dot_ino;
	char	dot_name[DIRSIZ];
	o_ino_t dotdot_ino;
	char	dotdot_name[DIRSIZ];
};

#define	SDSIZ	(sizeof (struct direct))

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FS_S5_FSDIR_H */
