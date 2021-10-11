/*
 * Copyright (c) 1994-1996, 1998 by  Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)pax.h	1.14	98/06/30 SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */

/*
 * OSF/1 1.2
 */

/*
 * @(#)$RCSfile: pax.h,v $ $Revision: 1.2.2.3 $ (OSF)
 * $Date: 1992/02/26 23:06:53 $
 */

/*
 * pax.h - definitions for entire program
 *
 * DESCRIPTION
 *
 *	This file contains most all of the definitions required by the PAX
 *	software.  This header is included in every source file.
 *
 * AUTHOR
 *
 *     Mark H. Colburn, NAPS International (mark@jhereg.mn.org)
 *
 * Sponsored by The USENIX Association for public distribution.
 *
 * Copyright (c) 1989 Mark H. Colburn.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Mark H. Colburn and sponsored by The USENIX Association.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef _PAX_H
#define	_PAX_H

/* Headers */

#include <unistd.h>
#include <errno.h>
#include <nl_types.h>
#include <libintl.h>
#include <locale.h>
#include <limits.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include "config.h"

/* Note include of "func.h" at the end; it requires some of the */
/* intervening definitions to be truly happy. */

#ifndef FALSE
#define	FALSE (0 == 1)
#endif /* FALSE */

#ifndef TRUE
#define	TRUE (0 == 0)
#endif /* TRUE */

/* Defines */

#define	STDIN	0		/* Standard input  file descriptor */
#define	STDOUT	1		/* Standard output file descriptor */

#define	TMAGIC		"ustar"		/* ustar and a null */
#define	TMAGLEN		6
#define	TVERSION	"00"		/* 00 and no null */
#define	TVERSLEN	2

/* Values used in typeflag field */
#define	REGTYPE		'0'		/* Regular File */
#define	AREGTYPE	'\0'		/* Regular File */
#define	LNKTYPE		'1'		/* Link */
#define	SYMTYPE		'2'		/* Reserved */
#define	CHRTYPE		'3'		/* Character Special File */
#define	BLKTYPE		'4'		/* Block Special File */
#define	DIRTYPE		'5'		/* Directory */
#define	FIFOTYPE	'6'		/* FIFO */
#define	CONTTYPE	'7'		/* Reserved */
#define	XHDRTYPE	'X'		/* Extended header */

/*
 *  Blocking defaults: blocking factor and block size.
 */

#define	DEFBLK_TAR	20	/* default blocking factor for tar */
#define	DEFBLK_CPIO	10	/* default blocking factor for cpio */
#define	BLOCKSIZE	512	/* all output is padded to 512 bytes */

#define	BLOCK	5120		/* Default archive block size */
#define	H_COUNT	10		/* Number of items in ASCII header */
#define	H_PRINT	"%06o%06o%06o%06o%06o%06o%06o%011lo%06o%011llo"
#define	H_SCAN	"%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6lo%11llo"
#define	H_STRLEN 70		/* ASCII header string length */
#define	M_ASCII "070707"	/* ASCII magic number */
#define	M_BINARY 070707		/* Binary magic number */
#define	M_STRLEN 6		/* ASCII magic number length */
#define	PATHELEM 256		/* Pathname element count limit */
#define	S_IFSHF	12		/* File type shift (shb in stat.h) */
#define	S_IPERM	01777		/* File permission bits (shb in stat.h) */
#define	S_IPOPN	0777		/* Open access bits (shb in stat.h) */

#define	TAR_OFFSET_MAX	077777777777	/* largest we can archive without */
					/* extensions.  (-E for tar or */
					/* -x xustar for pax */
#define	OCTAL7CHAR	07777777	/* Limit for ustar gid, uid, dev */
					/* unless extended headers are used */
/*
 * Access the offsets of the various fields within the tar header
 * (defined in main.c).
 */
extern const int TO_NAME;
extern const int TO_MODE;
extern const int TO_UID;
extern const int TO_GID;
extern const int TO_SIZE;
extern const int TO_MTIME;
extern const int TO_CHKSUM;
extern const int TO_TYPEFLG;
extern const int TO_LINKNAME;
extern const int TO_MAGIC;
extern const int TO_VERSION;
extern const int TO_UNAME;
extern const int TO_GNAME;
extern const int TO_DEVMAJOR;
extern const int TO_DEVMINOR;
extern const int TO_PREFIX;

/*
 *  Access the field lengths for the fields of the tar header
 */

#define	TL_NAME		100
#define	TL_MODE		8
#define	TL_UID		8
#define	TL_GID		8
#define	TL_SIZE		12
#define	TL_MTIME	12
#define	TL_CHKSUM	8
#define	TL_TYPEFLG	1
#define	TL_LINKNAME	100
#define	TL_MAGIC	6
#define	TL_VERSION	2
#define	TL_UNAME	32
#define	TL_GNAME	32
#define	TL_DEVMAJOR	8
#define	TL_DEVMINOR	8
#define	TL_PREFIX	155
#define	TL_TOTAL_TAR	500

/*
 *  Because some static global arrays inside names.c use these #defines,
 *  we will keep them around until a better method can be devised for
 *  creating them.
 */

#define	TUNMLEN		32
#define	TGNMLEN		32

/*
 * Trailer pathnames. All must be of the same length.
 */
#define	TRAILER	"TRAILER!!!"	/* Archive trailer (cpio compatible) */
#define	TRAILZ	11		/* Trailer pathname length (including null) */

#define	TAR		1
#define	CPIO		2
#define	PAX		3

#define	AR_READ 	0
#define	AR_WRITE 	1
#define	AR_EXTRACT	2
#define	AR_APPEND 	4

/* defines for get_disposition */
#define	ADD		1
#define	EXTRACT		2
#define	PASS		3


/* The checksum field is filled with this while the checksum is computed. */
#define	CHKBLANKS	"        "	/* 8 blanks, no null */

/*
 * Exit codes from the "tar" program
 */
#define	EX_SUCCESS	0	/* success! */
#define	EX_ARGSBAD	1	/* invalid args */
#define	EX_BADFILE	2	/* invalid filename */
#define	EX_BADARCH	3	/* bad archive */
#define	EX_SYSTEM	4	/* system gave unexpected error */

#define	ROUNDUP(a, b) 	(((a) % (b)) == 0 ? (a) : ((a) + ((b) - ((a) % (b)))))

/*
 * Exit codes from read_header
 */

#define	HDR_FIRSTREC	-1
#define	HDR_OK		0
#define	HDR_EOF		1
#define	HDR_ZEROREC	2
#define	HDR_ERROR	3
#define	HDR_NOXHDR	4

/*
 * Minimum value.
 */

#ifndef MIN
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif


/*
 * Maximum value.
 */

#ifndef MAX
#define	MAX(a, b)	(((a) > (b)) ? (a) : (b))
#endif

/*
 * Remove a file or directory.
 */
#define	REMOVE(name, asb) \
	(((asb)->sb_mode & S_IFMT) == S_IFDIR ? rmdir(name) : unlink(name))

/*
 * Cast and reduce to unsigned short.
 */
#define	USH(n)		(((ushort) (n)) & 0177777)

/* Type Definitions */

/*
 * Binary archive header (obsolete).
 */
typedef struct {
	short	b_dev;		/* Device code */
	ushort	b_ino;		/* Inode number */
	ushort	b_mode;		/* Type and permissions */
	ushort	b_uid;		/* Owner */
	ushort	b_gid;		/* Group */
	short	b_nlink;	/* Number of links */
	short	b_rdev;		/* Real device */
	ushort	b_mtime[2];	/* Modification time (hi/lo) */
	ushort	b_name;		/* Length of pathname (with null) */
	ushort	b_size[2];	/* Length of data */
}	Binary;

/*
 * File status with symbolic links. Kludged to hold symbolic link pathname
 * within structure.
 */
typedef struct {
	struct stat	sb_stat;
	char		sb_link[PATH_MAX + 1];
	char		*linkname;		/* for tar links */
} Stat;

#define	STAT(name, asb)		stat(name, &(asb)->sb_stat)
#define	FSTAT(fd, asb)		fstat(fd, &(asb)->sb_stat)

#define	sb_dev		sb_stat.st_dev
#define	sb_ino		sb_stat.st_ino
#define	sb_mode		sb_stat.st_mode
#define	sb_nlink	sb_stat.st_nlink
#define	sb_uid		sb_stat.st_uid
#define	sb_gid		sb_stat.st_gid
#define	sb_rdev		sb_stat.st_rdev
#define	sb_size		sb_stat.st_size
#define	sb_atime	sb_stat.st_atime
#define	sb_mtime	sb_stat.st_mtime
#define	sb_ctime	sb_stat.st_ctime

#ifdef	S_IFLNK
#define	LSTAT(name, asb)	lstat(name, &(asb)->sb_stat)
#define	sb_blksize		sb_stat.st_blksize
#define	sb_blocks		sb_stat.st_blocks
#else			/* S_IFLNK */
/*
 * File status without symbolic links.
 */
#define	LSTAT(name, asb)	stat(name, &(asb)->sb_stat)
#endif			/* S_IFLNK */

/*
 * Hard link sources. One or more are chained from each link structure.
 */
typedef struct name {
	struct name	*p_forw;	/* Forward chain (terminated) */
	struct name	*p_back;	/* Backward chain (circular) */
	char		 *p_name;	/* Pathname to link from */
} Path;

/*
 * File linking information. One entry exists for each unique file with
 * outstanding hard links.
 */
typedef	struct link {
	struct link	*l_forw;	/* Forward chain (terminated) */
	struct link	*l_back;	/* Backward chain (terminated) */
	dev_t		l_dev;		/* Device */
	ino_t		l_ino;		/* Inode */
	ushort		l_nlink;	/* Unresolved link count */
	OFFSET		l_size;		/* Length */
	char		*l_name;	/* pathname to link from */
	Path		*l_path;	/* Pathname which link to l_name */
} Link;

/*
 * Structure for ed-style replacement strings (-s option).
*/
typedef struct replstr {
	regex_t		comp;		/* compiled regular expression */
	char		*replace;	/* replacement string */
	char		print;		/* >0 if we are to print replacement */
	char		global;		/* >0 if we are to replace globally */
	struct replstr	*next;		/* pointer to next record */
} Replstr;

/*
 * Structure for list of directories
 */
typedef struct dirlist {
	char		*name;	/* name of the directory */
	uid_t		uid;	/* user id */
	gid_t		gid;	/* group id */
	mode_t		perm;	/* directory mode */
	struct timeval	atime;	/* directory access time */
	struct timeval	mtime;	/* directory modify time */
	struct dirlist	*next;	/* pointer to next record */
} Dirlist;

/*
 * Structure for the hash table
 */
typedef struct hashentry {
	char			*name;	/* Filename of entry */
	struct timeval		mtime;	/* modify time of file */
	struct hashentry	*next;	/* pointer to next entry */
} Hashentry;

#define	HTABLESIZE	(16*1024)	/* Hash table size */

struct xtar_hdr {
	uid_t		x_uid,		/* Uid of file */
			x_gid;		/* Gid of file */
	major_t		x_devmajor;	/* Device major	node */
	minor_t		x_devminor;	/* Device minor	node */
	off_t		x_filesz;	/* Length of file */
	char		*x_uname,	/* Pointer to uname */
			*x_gname,	/* Pointer to gid */
			*x_linkpath,	/* Path	for link */
			*x_path;	/* Path	of file	*/
	timestruc_t	x_mtime,	/* Seconds and nanoseconds */
			x_atime,
			x_ctime;

};

/*
 * This has to be included here to insure that all of the type
 * declarations are declared for the prototypes.
 */

#ifndef NO_EXTERN
/* Globally Available Identifiers */

extern char    *ar_file;
extern char    *bufend;
extern char    *bufstart;
extern char    *bufidx;
extern nl_catd catd;
extern char	*lastheader;
extern char	*myname;
extern int	archivefd;
extern int	blocking;
extern uint_t	blocksize;
extern gid_t	gid;
extern int	head_standard;
extern int	ar_interface;
extern int	ar_format;
extern int	mask;
extern int	ttyf;
extern uid_t	uid;
extern int	exit_status;
extern OFFSET	total;
extern short	areof;
extern short	f_access_time;
extern short	f_append;
extern short	f_blocking;
extern short	f_charmap;
extern short	f_create;
extern short	f_device;
extern short	f_dir_create;
extern short	f_disposition;
extern short	f_exit_on_error;
extern short	f_extract;
extern short	f_extract_access_time;
extern short	f_follow_links;
extern short	f_interactive;
extern short	f_link;
extern short	f_linksleft;
extern short	f_list;
extern short	f_mtime;
extern short	f_mode;
extern short	f_newer;
extern short	f_no_depth;
extern short	f_no_overwrite;
extern short	f_owner;
extern short	f_pass;
extern short	f_pax;
extern short	f_posix;
extern short	f_reverse_match;
extern short	f_single_match;
extern short	f_times;
extern short	f_unconditional;
extern short	f_verbose;
extern time_t	now;
extern uint_t	arvolume;
extern int	names_from_stdin;
extern Replstr	*rplhead;
extern Replstr	*rpltail;
extern char	**argv;
extern int	argc;
extern FILE	*msgfile;
extern Dirlist	*dirhead;
extern Dirlist	*dirtail;
extern short	bad_last_match;

/* The following are used for the extended headers */
extern struct xtar_hdr	Xtarhdr;
extern size_t		xrec_size;	/* initial size */
extern char		*xrec_ptr;
extern off_t		xrec_offset;
extern int		charset_type;
extern	u_longlong_t	xhdr_flgs;	/* Bits set determine which items */
					/* need to be in extended header. */
#define	_X_DEVMAJOR	0x1
#define	_X_DEVMINOR	0x2
#define	_X_GID		0x4
#define	_X_GNAME	0x8
#define	_X_LINKPATH	0x10
#define	_X_PATH		0x20
#define	_X_SIZE		0x40
#define	_X_UID		0x80
#define	_X_UNAME	0x100
#define	_X_ATIME	0x200
#define	_X_CTIME	0x400
#define	_X_MTIME	0x800
#define	_X_LAST		0x40000000

extern u_longlong_t	xhdr_count;
extern char		xhdr_dirname[];
extern char		pidchars[];
extern char		local_path[];
extern char		local_linkpath[];
extern char		local_gname[];
extern char		local_uname[];

#endif /* NO_EXTERN */

extern char	*optarg;
extern int	optind;
extern int	sys_nerr;
extern char	*sys_errlist[];
extern int	errno;

#define	CHARSET_UNKNOWN	0
#define	CHARSET_7_BIT	1
#define	CHARSET_8_BIT	2
#define	CHARSET_UTF_8	3
#define	CHARSET_ERROR	-1

#define	PID_MAX_DIGITS		(10 * sizeof (pid_t) / 4)
#define	TIME_MAX_DIGITS		(10 * sizeof (time_t) / 4)
#define	LONG_MAX_DIGITS		(10 * sizeof (long) / 4)
#define	ULONGLONG_MAX_DIGITS	(10 * sizeof (u_longlong_t) / 4)

/*
 * UTF_8 encoding requires more space than the current codeset equivalent.
 * Currently a factor of 2-3 would suffice, but it is possible for a factor
 * of 6 to be needed in the future, so for saftey, we use that here.
 */
#define	UTF_8_FACTOR	6

#include "func.h"
#endif /* _PAX_H */
