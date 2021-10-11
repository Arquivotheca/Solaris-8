/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *
 *
 *
 *		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	Copyright (c) 1986-1991,1994-1999 by Sun Microsystems, Inc.
 *	Copyright (c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	All rights reserved.
 */

#ifndef	_NFS_NFS_H
#define	_NFS_NFS_H

#pragma ident	"@(#)nfs.h	1.111	99/09/24 SMI"
/*	nfs.h 2.38 88/08/19 SMI 	*/

#include <sys/isa_defs.h>
#include <sys/vfs.h>
#include <sys/stream.h>
#include <rpc/types.h>
#include <sys/types32.h>
#ifdef _KERNEL
#include <rpc/xdr.h>
#endif
#ifdef _KERNEL
#include <sys/fcntl.h>
#include <sys/kstat.h>
#include <nfs/mount.h>
#endif
#include <vm/page.h>
#include <rpc/rpc_sztypes.h>
#include <sys/sysmacros.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * remote file service numbers
 */
#define	NFS_PROGRAM	((rpcprog_t)100003)
#define	NFS_VERSMIN	((rpcvers_t)2)
#define	NFS_VERSMAX	((rpcvers_t)3)
#define	NFS_VERSION	((rpcvers_t)2)
#define	NFS_PORT	2049

/* Maximum size of data portion of a remote request */
#define	NFS_MAXDATA	8192
#define	NFS_MAXNAMLEN	255
#define	NFS_MAXPATHLEN	1024

/*
 * Rpc retransmission parameters
 */
#define	NFS_TIMEO	11	/* initial timeout for clts in 10th of a sec */
#define	NFS_RETRIES	5	/* times to retry request */
#define	NFS_COTS_TIMEO	600	/* initial timeout for cots in 10th of a sec */

/*
 * The value of UID_NOBODY/GIF_NOBODY presented to the world via NFS.
 * UID_NOBODY/GID_NOBODY is translated to NFS_UID_NOBODY/NFS_GID_NOBODY
 * when being sent out over the network and NFS_UID_NOBODY/NFS_GID_NOBODY
 * is translated to UID_NOBODY/GID_NOBODY when received.
 */
#define	NFS_UID_NOBODY	-2
#define	NFS_GID_NOBODY	-2

/*
 * maximum transfer size for different interfaces
 */
#define	ECTSIZE	2048
#define	IETSIZE	8192

/*
 * WebNFS error status
 */
enum wnfsstat {
	WNFSERR_CLNT_FLAVOR = 20001	/* invalid client sec flavor */
};

/*
 * Error status
 * Should include all possible net errors.
 * For now we just cast errno into an enum nfsstat.
 */
enum nfsstat {
	NFS_OK = 0,			/* no error */
	NFSERR_PERM = 1,		/* Not owner */
	NFSERR_NOENT = 2,		/* No such file or directory */
	NFSERR_IO = 5,			/* I/O error */
	NFSERR_NXIO = 6,		/* No such device or address */
	NFSERR_ACCES = 13,		/* Permission denied */
	NFSERR_EXIST = 17,		/* File exists */
	NFSERR_XDEV = 18,		/* Cross-device link */
	NFSERR_NODEV = 19,		/* No such device */
	NFSERR_NOTDIR = 20,		/* Not a directory */
	NFSERR_ISDIR = 21,		/* Is a directory */
	NFSERR_INVAL = 22,		/* Invalid argument */
	NFSERR_FBIG = 27,		/* File too large */
	NFSERR_NOSPC = 28,		/* No space left on device */
	NFSERR_ROFS = 30,		/* Read-only file system */
	NFSERR_OPNOTSUPP = 45,		/* Operation not supported */
	NFSERR_NAMETOOLONG = 63,	/* File name too long */
	NFSERR_NOTEMPTY = 66,		/* Directory not empty */
	NFSERR_DQUOT = 69,		/* Disc quota exceeded */
	NFSERR_STALE = 70,		/* Stale NFS file handle */
	NFSERR_REMOTE = 71,		/* Object is remote */
	NFSERR_WFLUSH = 99		/* write cache flushed */
};

typedef enum nfsstat nfsstat;

/*
 * File types
 */
enum nfsftype {
	NFNON,
	NFREG,		/* regular file */
	NFDIR,		/* directory */
	NFBLK,		/* block special */
	NFCHR,		/* character special */
	NFLNK,		/* symbolic link */
	NFSOC		/* socket */
};

/*
 * Macros for converting device numbers to and from the format
 * SunOS 4.x used. SVR4 uses 14 bit majors and 18 bits minors,
 * SunOS 4.x used 8 bit majors and 8 bit minors. It isn't sufficient
 * to use the cmpdev() and expdev() macros because they only compress
 * 7 bit (and smaller) majors. We must compress 8 bit majors too.
 * If the major or minor exceeds 8 bits, then we send it out in
 * full 32 bit format and hope that the peer can deal with it.
 */

#define	SO4_BITSMAJOR	8	/* # of SunOS 4.x major device bits */
#define	SO4_BITSMINOR	8	/* # of SunOS 4.x minor device bits */
#define	SO4_MAXMAJ	0xff	/* SunOS 4.x max major value */
#define	SO4_MAXMIN	0xff	/* SunOS 4.x max minor value */

/*
 * Convert to over-the-wire device number format
 */
#define	nfsv2_cmpdev(x) \
	((uint32_t) \
	((getmajor(x) > SO4_MAXMAJ || getminor(x) > SO4_MAXMIN) ? NODEV : \
	((getmajor(x) << SO4_BITSMINOR) | (getminor(x) & SO4_MAXMIN))))

/*
 * Convert from over-the-wire format to SVR4 device number format
 */
#define	nfsv2_expdev(x) \
	makedevice((((x) >> SO4_BITSMINOR) & SO4_MAXMAJ), (x) & SO4_MAXMIN)

/*
 * Special kludge for fifos (named pipes)  [to adhere to NFS Protocol Spec]
 *
 * VFIFO is not in the protocol spec (VNON will be replaced by VFIFO)
 * so the over-the-wire representation is VCHR with a '-1' device number.
 *
 * NOTE: This kludge becomes unnecessary with the Protocol Revision,
 *	 but it may be necessary to support it (backwards compatibility).
 */
#define	NFS_FIFO_TYPE	NFCHR
#define	NFS_FIFO_MODE	S_IFCHR
#define	NFS_FIFO_DEV	((uint32_t)-1)

/* identify fifo in nfs attributes */
#define	NA_ISFIFO(NA)	(((NA)->na_type == NFS_FIFO_TYPE) && \
			    ((NA)->na_rdev == NFS_FIFO_DEV))

/* set fifo in nfs attributes */
#define	NA_SETFIFO(NA)	{ \
		(NA)->na_type = NFS_FIFO_TYPE; \
		(NA)->na_rdev = NFS_FIFO_DEV; \
		(NA)->na_mode = ((NA)->na_mode & ~S_IFMT) | NFS_FIFO_MODE; \
		}

/*
 * Check for time overflow using a kernel tunable to determine whether
 * we should accept or reject an unsigned 32-bit time value. time value in NFS
 * v2/3 protocol is unsigned 32 bit, but ILP32 kernel only allows 31 bits.
 *
 * nfs_allow_preepoch_time is TRUE if pre-epoch (negative) times are allowed
 * and is FALSE (the default) otherwise.
 * If FALSE, the maximum time value is INT32_MAX for 32-bit kernels and
 * UINT32_MAX for 64-bit kernels (to allow times larger than 2038)
 * and the minimum is zero. Note that in that case, a 32-bit application
 * running on a 64-bit kernel will not be able to access files with
 * the larger time values.
 * If nfs_allow_preepoch_time is TRUE, the maximum time value is INT32_MAX
 * for both kernel configurations and the minimum is INT32_MIN.
 */
#ifdef _KERNEL

extern bool_t		nfs_allow_preepoch_time;

#ifdef _LP64

/*
 * If no negative otw values are allowed, may use the full 32-bits of the
 * time to represent time later than 2038, by presenting the value as an
 * unsigned (but this can only be used by 64-bit apps due to cstat32
 * retrictions). If negative values are allowed, cannot represent times
 * after 2038. Either way, all 32 bits have a valid representation.
 */
/* Check if an nfstime3 (uint32_t) can be stored in the system time */
#define	NFS3_TIME_OK(tt)	TRUE

/* Check if an nfs2_timeval (uint32_t) can be stored in the system time. */
#define	NFS2_TIME_OK(tt)	TRUE

/*
 * Macros to use with NFS*_TIME_OK and its derivatives (FATTR) - because
 * the NFS*_TIME_OK are always TRUE for 64-bit, the parameter "cond" is
 * always TRUE and therefore stmt is always executed for IF_NFS_TIME_OK and
 * IF_ELSE_NFS_TIME_OK, but never for IF_NFS_NOT_TIME_OK.
 */
#define	IF_NFS_TIME_OK(cond, stmt)	stmt
#define	IF_NOT_NFS_TIME_OK(cond, stmt)
#define	IF_ELSE_NFS_TIME_OK(cond, stmt, else_stmt)	stmt

/*
 * check if a time_t (int64_t) is ok when preepoch times are allowed -
 * every 32-bit value is accepted, but can't overflow 64->32.
 */
#define	NFS_PREEPOCH_TIME_T_OK(tt)					\
	(((tt) >= (time_t)INT32_MIN) && ((tt) <= (time_t)INT32_MAX))

/*
 * check if a time_t (int64_t) is ok when preepoch times are not allowed -
 * every positive 32-bit value is accepted, but can't overflow 64->32.
 */
#define	NFS_LARGETIME_TIME_T_OK(tt)					\
	(((tt) >= 0) && ((tt) <= (time_t)(ulong_t)UINT32_MAX))

#else /* not _LP64 */

/*
 * Cannot represent times after 2038 in a 32-bit kernel, but we may wish to
 * restrict the use of negative values (which violate the protocol).
 * So if negative times allowed, all uint32 time values are valid. Otherwise
 * only those which are less than INT32_MAX (msb=0).
 */

/* Check if an nfstime3 (uint32_t) can be stored in the system time */
#define	NFS3_TIME_OK(tt)						\
	(nfs_allow_preepoch_time || ((tt) <= INT32_MAX))

/* Check if an nfs2_timeval (uint32_t) can be stored in the system time. */
#define	NFS2_TIME_OK(tt)						\
	(nfs_allow_preepoch_time || ((tt) <= INT32_MAX))

/*
 * Macros to use with NFS*_TIME_OK and its derivatives (FATTR) - because
 * the NFS*_TIME_OK is not always TRUE for 32-bit the parameter "cond" is
 * tested as condition for executing the stmt.
 */
#define	IF_NFS_TIME_OK(cond, stmt)	if (cond) stmt
#define	IF_NOT_NFS_TIME_OK(cond, stmt)	if (!(cond)) stmt
#define	IF_ELSE_NFS_TIME_OK(cond, stmt, else_stmt)		\
	if (cond) stmt else else_stmt

/*
 * check if a time_t (int32_t) is ok when preepoch times are allowed -
 * every 32-bit value is accepted
 */
#define	NFS_PREEPOCH_TIME_T_OK(tt)	TRUE

/*
 * check if a time_t (int32_t) is ok when preepoch times are not allowed -
 * only positive values are accepted upto INT32_MAX.
 */
#define	NFS_LARGETIME_TIME_T_OK(tt)	((tt) >= 0)

#endif /* _LP64 */

/*
 * Test if time_t (signed long) can be sent over the wire - only if:
 * 1. The time value can fit in a uint32_t; and
 * 2. Either the time value is positive or allow preepoch times.
 */
#define	NFS_TIME_T_OK(tt)						\
	(nfs_allow_preepoch_time ?					\
		NFS_PREEPOCH_TIME_T_OK(tt) : NFS_LARGETIME_TIME_T_OK(tt))

/* Test if all attr times are valid */
#define	NFS_VAP_TIME_OK(vap)						\
	(nfs_allow_preepoch_time ?					\
		(NFS_PREEPOCH_TIME_T_OK((vap)->va_atime.tv_sec) &&	\
		NFS_PREEPOCH_TIME_T_OK((vap)->va_mtime.tv_sec) &&	\
		NFS_PREEPOCH_TIME_T_OK((vap)->va_ctime.tv_sec)) :	\
		(NFS_LARGETIME_TIME_T_OK((vap)->va_atime.tv_sec) &&	\
		NFS_LARGETIME_TIME_T_OK((vap)->va_mtime.tv_sec) &&	\
		NFS_LARGETIME_TIME_T_OK((vap)->va_ctime.tv_sec)))

/*
 * To extend the sign or not extend the sign, that is the question.
 * Note: The correct way is to code a macro:
 * #define	NFS_TIME_T_CONVERT(tt)					\
 *	(nfs_allow_preepoch_time ? (int32_t)(tt) : (uint32_t)(tt))
 * But the 64-bit compiler does not extend the sign in that case (why?)
 * so we'll do it the ugly way...
 */
#define	NFS_TIME_T_CONVERT(systt, tt)		\
	if (nfs_allow_preepoch_time) {		\
		systt = (int32_t)(tt);		\
	} else {				\
		systt = (uint32_t)(tt);		\
	}

/* macro to check for overflowed time attribute fields - version 3 */
#define	NFS3_FATTR_TIME_OK(attrs)			\
	(NFS3_TIME_OK((attrs)->atime.seconds) &&	\
	NFS3_TIME_OK((attrs)->mtime.seconds) &&		\
	NFS3_TIME_OK((attrs)->ctime.seconds))

/* macro to check for overflowed time attribute fields - version 2 */
#define	NFS2_FATTR_TIME_OK(attrs)			\
	(NFS2_TIME_OK((attrs)->na_atime.tv_sec) &&	\
	NFS2_TIME_OK((attrs)->na_mtime.tv_sec) &&	\
	NFS2_TIME_OK((attrs)->na_ctime.tv_sec))

/* Check that a size3 value is not overflowed */
#define	NFS3_SIZE_OK(size)	((size) <= MAXOFFSET_T)

#endif /* _KERNEL */

/*
 * Size of an fhandle in bytes
 */
#define	NFS_FHSIZE	32

struct nfs_fid {
	ushort_t nf_len;
	ushort_t nf_pad;
	char	nf_data[NFS_FHSIZE];
};

/*
 * File access handle
 * This structure is the Sun server representation of a file.
 * It is handed out by a server for the client to use in further
 * file transactions.
 */

/*
 * This struct is only used to find the size of the data field in the
 * fhandle structure below.
 */
struct fhsize {
	fsid_t	f1;
	ushort_t f2;
	char	f3[4];
	ushort_t f4;
	char	f5[4];
};
#define	NFS_FHMAXDATA	((NFS_FHSIZE - sizeof (struct fhsize) + 8) / 2)

/*
 * The current nfs file handle size for version 3 is currently 32 which is
 * the same in version 2. It is the object returned from makefh
 * (fhandle_t) which is size of struct svcfh.
 * Thus, if the size of struct svcfh changes or if Version 3 uses some other
 * filehandle format, this constant may need to change.
 */

#define	NFS3_CURFHSIZE	32

/*
 * This is the actual definition of a filehandle.  There is some dependence
 * on this layout in NFS-related code, particularly in the user-level lock
 * manager, so be careful about changing it.
 */

struct svcfh {
	fsid_t	fh_fsid;			/* filesystem id */
	ushort_t fh_len;			/* file number length */
	char	fh_data[NFS_FHMAXDATA];		/* and data */
	ushort_t fh_xlen;			/* export file number length */
	char	fh_xdata[NFS_FHMAXDATA];	/* and data */
};

typedef struct svcfh fhandle_t;

/*
 * Arguments to remote write and writecache
 */
/*
 * The `over the wire' representation of the first four arguments.
 */
struct otw_nfswriteargs {
	fhandle_t	otw_wa_fhandle;
	uint32_t	otw_wa_begoff;
	uint32_t	otw_wa_offset;
	uint32_t	otw_wa_totcount;
};

struct nfswriteargs {
	struct otw_nfswriteargs *wa_args;	/* ptr to the otw arguments */
	struct otw_nfswriteargs wa_args_buf;	/* space for otw arguments */
	uint32_t	wa_count;
	char		*wa_data;	/* data to write (up to NFS_MAXDATA) */
	mblk_t		*wa_mblk;	/* pointer to mblks containing data */
};
#define	wa_fhandle	wa_args->otw_wa_fhandle
#define	wa_begoff	wa_args->otw_wa_begoff
#define	wa_offset	wa_args->otw_wa_offset
#define	wa_totcount	wa_args->otw_wa_totcount

/*
 * NFS timeval struct using unsigned int as specified in V2 protocol.
 * tv_sec and tv_usec used to match existing code.
 */
struct nfs2_timeval {
	uint32_t tv_sec;
	uint32_t tv_usec;
};
typedef struct nfs2_timeval nfs2_timeval;

/*
 * File attributes
 */
struct nfsfattr {
	enum nfsftype	na_type;	/* file type */
	uint32_t	na_mode;	/* protection mode bits */
	uint32_t	na_nlink;	/* # hard links */
	uint32_t	na_uid;		/* owner user id */
	uint32_t	na_gid;		/* owner group id */
	uint32_t	na_size;	/* file size in bytes */
	uint32_t	na_blocksize;	/* prefered block size */
	uint32_t	na_rdev;	/* special device # */
	uint32_t	na_blocks;	/* Kb of disk used by file */
	uint32_t	na_fsid;	/* device # */
	uint32_t	na_nodeid;	/* inode # */
	struct nfs2_timeval na_atime;	/* time of last access */
	struct nfs2_timeval na_mtime;	/* time of last modification */
	struct nfs2_timeval na_ctime;	/* time of last change */
};

#define	n2v_type(x)	(NA_ISFIFO(x) ? VFIFO : nf_to_vt[(x)->na_type])
#define	n2v_rdev(x)	(NA_ISFIFO(x) ? 0 : (x)->na_rdev)

/*
 * Arguments to remote read
 */
struct nfsreadargs {
	fhandle_t	ra_fhandle;	/* handle for file */
	uint32_t	ra_offset;	/* byte offset in file */
	uint32_t	ra_count;	/* immediate read count */
	uint32_t	ra_totcount;	/* total read cnt (from this offset) */
};

/*
 * Status OK portion of remote read reply
 */
struct nfsrrok {
	struct nfsfattr	rrok_attr;	/* attributes, need for pagin */
	uint32_t	rrok_count;	/* bytes of data */
	char		*rrok_data;	/* data (up to NFS_MAXDATA bytes) */
	uint_t		rrok_bufsize;	/* size of kmem_alloc'd buffer */
	mblk_t		*rrok_mp;	/* mblk_t contains data for reply */
};

/*
 * Reply from remote read
 */
struct nfsrdresult {
	nfsstat	rr_status;			/* status of read */
	union {
		struct nfsrrok	rr_ok_u;	/* attributes, need for pagin */
	} rr_u;
};
#define	rr_ok		rr_u.rr_ok_u
#define	rr_attr		rr_u.rr_ok_u.rrok_attr
#define	rr_count	rr_u.rr_ok_u.rrok_count
#define	rr_bufsize	rr_u.rr_ok_u.rrok_bufsize
#define	rr_data		rr_u.rr_ok_u.rrok_data
#define	rr_mp		rr_u.rr_ok_u.rrok_mp

/*
 * File attributes which can be set
 */
struct nfssattr {
	uint32_t	sa_mode;	/* protection mode bits */
	uint32_t	sa_uid;		/* owner user id */
	uint32_t	sa_gid;		/* owner group id */
	uint32_t	sa_size;	/* file size in bytes */
	struct nfs2_timeval sa_atime;	/* time of last access */
	struct nfs2_timeval sa_mtime;	/* time of last modification */
};


/*
 * Reply status with file attributes
 */
struct nfsattrstat {
	nfsstat	ns_status;			/* reply status */
	union {
		struct nfsfattr ns_attr_u;	/* NFS_OK: file attributes */
	} ns_u;
};
#define	ns_attr	ns_u.ns_attr_u


/*
 * NFS_OK part of read sym link reply union
 */
struct nfssrok {
	uint32_t srok_count;	/* size of string */
	char	*srok_data;	/* string (up to NFS_MAXPATHLEN bytes) */
};

/*
 * Result of reading symbolic link
 */
struct nfsrdlnres {
	nfsstat	rl_status;			/* status of symlink read */
	union {
		struct nfssrok	rl_srok_u;	/* name of linked to */
	} rl_u;
};
#define	rl_srok		rl_u.rl_srok_u
#define	rl_count	rl_u.rl_srok_u.srok_count
#define	rl_data		rl_u.rl_srok_u.srok_data


/*
 * Arguments to readdir
 */
struct nfsrddirargs {
	fhandle_t rda_fh;	/* directory handle */
	uint32_t rda_offset;	/* offset in directory (opaque) */
	uint32_t rda_count;	/* number of directory bytes to read */
};

/*
 * NFS_OK part of readdir result
 */
struct nfsrdok {
	uint32_t rdok_offset;		/* next offset (opaque) */
	uint32_t rdok_size;		/* size in bytes of entries */
	bool_t	rdok_eof;		/* true if last entry is in result */
	struct dirent64 *rdok_entries;	/* variable number of entries */
};

/*
 * Readdir result
 */
struct nfsrddirres {
	nfsstat	rd_status;
	uint_t		rd_bufsize;	/* client request size (not xdr'ed) */
	union {
		struct nfsrdok rd_rdok_u;
	} rd_u;
};
#define	rd_rdok		rd_u.rd_rdok_u
#define	rd_offset	rd_u.rd_rdok_u.rdok_offset
#define	rd_size		rd_u.rd_rdok_u.rdok_size
#define	rd_eof		rd_u.rd_rdok_u.rdok_eof
#define	rd_entries	rd_u.rd_rdok_u.rdok_entries


/*
 * Arguments for directory operations
 */
struct nfsdiropargs {
	fhandle_t	*da_fhandle;	/* pointer to directory file handle */
	char		*da_name;	/* name (up to NFS_MAXNAMLEN bytes) */
	fhandle_t	da_fhandle_buf;	/* directory file handle */
	int		da_flags;	/* flags, see below */
};
#define	DA_FREENAME	1

/*
 * NFS_OK part of directory operation result
 */
struct  nfsdrok {
	fhandle_t	drok_fhandle;	/* result file handle */
	struct nfsfattr	drok_attr;	/* result file attributes */
};

/*
 * Results from directory operation
 */
struct  nfsdiropres {
	nfsstat	dr_status;			/* result status */
	union {
		struct  nfsdrok	dr_drok_u;	/* NFS_OK result */
	} dr_u;
};
#define	dr_drok		dr_u.dr_drok_u
#define	dr_fhandle	dr_u.dr_drok_u.drok_fhandle
#define	dr_attr		dr_u.dr_drok_u.drok_attr

/*
 * arguments to setattr
 */
struct nfssaargs {
	fhandle_t	saa_fh;		/* fhandle of file to be set */
	struct nfssattr	saa_sa;		/* new attributes */
};

/*
 * arguments to create and mkdir
 */
struct nfscreatargs {
	struct nfsdiropargs	ca_da;	/* file name to create and parent dir */
	struct nfssattr		*ca_sa;	/* initial attributes */
	struct nfssattr		ca_sa_buf;	/* space to store attributes */
};

/*
 * arguments to link
 */
struct nfslinkargs {
	fhandle_t		*la_from;	/* old file */
	fhandle_t		la_from_buf;	/* old file */
	struct nfsdiropargs	la_to;		/* new file and parent dir */
};

/*
 * arguments to rename
 */
struct nfsrnmargs {
	struct nfsdiropargs rna_from;	/* old file and parent dir */
	struct nfsdiropargs rna_to;	/* new file and parent dir */
};

/*
 * arguments to symlink
 */
struct nfsslargs {
	struct nfsdiropargs	sla_from;	/* old file and parent dir */
	char			*sla_tnm;	/* new name */
	int			sla_tnm_flags;	/* flags for name */
	struct nfssattr		*sla_sa;	/* attributes */
	struct nfssattr		sla_sa_buf;	/* attributes buffer */
};
#define	SLA_FREETNM	1

/*
 * NFS_OK part of statfs operation
 */
struct nfsstatfsok {
	uint32_t fsok_tsize;	/* preferred transfer size in bytes */
	uint32_t fsok_bsize;	/* fundamental file system block size */
	uint32_t fsok_blocks;	/* total blocks in file system */
	uint32_t fsok_bfree;	/* free blocks in fs */
	uint32_t fsok_bavail;	/* free blocks avail to non-superuser */
};

/*
 * Results of statfs operation
 */
struct nfsstatfs {
	nfsstat	fs_status;			/* result status */
	union {
		struct	nfsstatfsok fs_fsok_u;	/* NFS_OK result */
	} fs_u;
};
#define	fs_fsok		fs_u.fs_fsok_u
#define	fs_tsize	fs_u.fs_fsok_u.fsok_tsize
#define	fs_bsize	fs_u.fs_fsok_u.fsok_bsize
#define	fs_blocks	fs_u.fs_fsok_u.fsok_blocks
#define	fs_bfree	fs_u.fs_fsok_u.fsok_bfree
#define	fs_bavail	fs_u.fs_fsok_u.fsok_bavail

#ifdef _KERNEL
/*
 * XDR routines for handling structures defined above
 */
extern bool_t	xdr_attrstat(XDR *, struct nfsattrstat *);
extern bool_t	xdr_fastattrstat(XDR *, struct nfsattrstat *);
extern bool_t	xdr_creatargs(XDR *, struct nfscreatargs *);
extern bool_t	xdr_fastcreatargs(XDR *, struct nfscreatargs **);
extern bool_t	xdr_diropargs(XDR *, struct nfsdiropargs *);
extern bool_t	xdr_fastdiropargs(XDR *, struct nfsdiropargs **);
extern bool_t	xdr_diropres(XDR *, struct nfsdiropres *);
extern bool_t	xdr_fastdiropres(XDR *, struct nfsdiropres *);
extern bool_t	xdr_drok(XDR *, struct nfsdrok *);
#ifdef _LITTLE_ENDIAN
extern bool_t	xdr_fastdrok(XDR *, struct nfsdrok *);
#endif
extern bool_t	xdr_fattr(XDR *, struct nfsfattr *);
#ifdef _LITTLE_ENDIAN
extern bool_t	xdr_fastfattr(XDR *, struct nfsfattr *);
#endif
extern bool_t	xdr_fhandle(XDR *, fhandle_t *);
extern bool_t	xdr_fastfhandle(XDR *, fhandle_t **);
extern bool_t	xdr_linkargs(XDR *, struct nfslinkargs *);
extern bool_t	xdr_fastlinkargs(XDR *, struct nfslinkargs **);
extern bool_t	xdr_rddirargs(XDR *, struct nfsrddirargs *);
extern bool_t	xdr_fastrddirargs(XDR *, struct nfsrddirargs **);
extern bool_t	xdr_putrddirres(XDR *, struct nfsrddirres *);
extern bool_t	xdr_getrddirres(XDR *, struct nfsrddirres *);
extern bool_t	xdr_rdlnres(XDR *, struct nfsrdlnres *);
extern bool_t	xdr_rdresult(XDR *, struct nfsrdresult *);
extern bool_t	xdr_readargs(XDR *, struct nfsreadargs *);
extern bool_t	xdr_fastreadargs(XDR *, struct nfsreadargs **);
extern bool_t	xdr_rnmargs(XDR *, struct nfsrnmargs *);
extern bool_t	xdr_fastrnmargs(XDR *, struct nfsrnmargs **);
extern bool_t	xdr_rrok(XDR *, struct nfsrrok *);
extern bool_t	xdr_saargs(XDR *, struct nfssaargs *);
extern bool_t	xdr_fastsaargs(XDR *, struct nfssaargs **);
extern bool_t	xdr_sattr(XDR *, struct nfssattr *);
extern bool_t	xdr_slargs(XDR *, struct nfsslargs *);
extern bool_t	xdr_fastslargs(XDR *, struct nfsslargs **);
extern bool_t	xdr_srok(XDR *, struct nfssrok *);
extern bool_t	xdr_nfs2_timeval(XDR *, struct nfs2_timeval *);
extern bool_t	xdr_writeargs(XDR *, struct nfswriteargs *);
extern bool_t	xdr_fastwriteargs(XDR *, struct nfswriteargs **);
extern bool_t	xdr_fsok(XDR *, struct nfsstatfsok *);
#ifdef _LITTLE_ENDIAN
extern bool_t	xdr_fastfsok(XDR *, struct nfsstatfsok *);
#endif
extern bool_t	xdr_statfs(XDR *, struct nfsstatfs *);
extern bool_t	xdr_faststatfs(XDR *, struct nfsstatfs *);
#ifdef _LITTLE_ENDIAN
extern bool_t	xdr_fastenum(XDR *, enum_t *);
#endif
#endif

/*
 * Remote file service routines
 */
#define	RFS_NULL	0
#define	RFS_GETATTR	1
#define	RFS_SETATTR	2
#define	RFS_ROOT	3
#define	RFS_LOOKUP	4
#define	RFS_READLINK	5
#define	RFS_READ	6
#define	RFS_WRITECACHE	7
#define	RFS_WRITE	8
#define	RFS_CREATE	9
#define	RFS_REMOVE	10
#define	RFS_RENAME	11
#define	RFS_LINK	12
#define	RFS_SYMLINK	13
#define	RFS_MKDIR	14
#define	RFS_RMDIR	15
#define	RFS_READDIR	16
#define	RFS_STATFS	17
#define	RFS_NPROC	18

#ifdef _KERNEL
/*
 * The NFS Version 2 service procedures
 */
struct exportinfo;	/* defined in nfs/export.h */
struct servinfo;	/* defined in nfs/nfs_clnt.h */
struct mntinfo;		/* defined in nfs/nfs_clnt.h */

extern void rfs_getattr(fhandle_t *, struct nfsattrstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_getattr_getfh(fhandle_t *);
extern void rfs_setattr(struct nfssaargs *, struct nfsattrstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_setattr_getfh(struct nfssaargs *);
extern void rfs_lookup(struct nfsdiropargs *, struct nfsdiropres *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_lookup_getfh(struct nfsdiropargs *);
extern void rfs_readlink(fhandle_t *, struct nfsrdlnres *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_readlink_getfh(fhandle_t *);
extern void rfs_rlfree(struct nfsrdlnres *);
extern void rfs_read(struct nfsreadargs *, struct nfsrdresult *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_read_getfh(struct nfsreadargs *);
extern void rfs_rdfree(struct nfsrdresult *);
extern void rfs_write_sync(struct nfswriteargs *, struct nfsattrstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern void rfs_write(struct nfswriteargs *, struct nfsattrstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_write_getfh(struct nfswriteargs *);
extern void rfs_create(struct nfscreatargs *, struct nfsdiropres *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_create_getfh(struct nfscreatargs *);
extern void rfs_remove(struct nfsdiropargs *, enum nfsstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_remove_getfh(struct nfsdiropargs *);
extern void rfs_rename(struct nfsrnmargs *, enum nfsstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_rename_getfh(struct nfsrnmargs *);
extern void rfs_link(struct nfslinkargs *, enum nfsstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_link_getfh(struct nfslinkargs *);
extern void rfs_symlink(struct nfsslargs *, enum nfsstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_symlink_getfh(struct nfsslargs *);
extern void rfs_mkdir(struct nfscreatargs *, struct nfsdiropres *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_mkdir_getfh(struct nfscreatargs *);
extern void rfs_rmdir(struct nfsdiropargs *, enum nfsstat *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_rmdir_getfh(struct nfsdiropargs *);
extern void rfs_readdir(struct nfsrddirargs *, struct nfsrddirres *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_readdir_getfh(struct nfsrddirargs *);
extern void rfs_rddirfree(struct nfsrddirres *);
extern void rfs_statfs(fhandle_t *, struct nfsstatfs *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs_statfs_getfh(fhandle_t *);
extern void rfs_srvrinit(void);
extern void rfs_srvrfini(void);
#endif

#ifdef _KERNEL

/*
 * flags to define path types during Multi Component Lookups
 * using the public filehandle
 */
#define	URLPATH		0x01	/* Universal Resource Locator path */
#define	NATIVEPATH	0x02	/* Native path, i.e., via mount protocol */
#define	SECURITY_QUERY	0x04	/* Security query */

enum nfs_svccounts {NFS_CALLS, NFS_BADCALLS}; /* index for svstat_ptr */

/*	function defs for NFS kernel */
extern int	nfs_validate_caches(vnode_t *, cred_t *);
extern void	nfs_purge_caches(vnode_t *, cred_t *);
extern void	nfs_purge_rddir_cache(vnode_t *);
extern void	nfs_cache_check(vnode_t *, timestruc_t, timestruc_t,
				len_t, int *, cred_t *);
extern void	nfs_attrcache(vnode_t *, struct nfsfattr *, int);
extern void	nfs_attrcache_va(vnode_t *, struct vattr *, int);
extern int	nfs_getattr_otw(vnode_t *, struct vattr *, cred_t *);
extern int	nfsgetattr(vnode_t *, struct vattr *, cred_t *);
extern int	nattr_to_vattr(vnode_t *, struct nfsfattr *, struct vattr *);
extern void	nfs_async_stop(struct vfs *);
extern int	nfs_async_stop_sig(struct vfs *);
extern int	nfs_clntinit(void);
extern void	nfs_clntfini(void);
extern int	nfstsize(void);
extern int	nfs_srvinit(void);
extern void	nfs_srvfini(void);
extern int	vattr_to_sattr(struct vattr *, struct nfssattr *);
extern void	setdiropargs(struct nfsdiropargs *, char *, vnode_t *);
extern int	setdirgid(vnode_t *, gid_t *, cred_t *);
extern int	setdirmode(vnode_t *, mode_t *, cred_t *);
extern int	newnum(void);
extern char	*newname(void);
extern int	nfs_atoi(char *);
extern int	nfs_subrinit(void);
extern void	nfs_subrfini(void);
extern enum nfsstat puterrno(int);
extern int	geterrno(enum nfsstat);
extern int	nfsinit(struct vfssw *, int);
extern void	nfsfini(void);
extern int	nfs_vfsinit(void);
extern void	nfs_vfsfini(void);
extern int	nfs_dump(vnode_t *, caddr_t, int, int);
extern void	nfs_perror(int error, char *fmt, ...);
extern void	nfs_cmn_err(int error, int level, char *fmt, ...);
extern int	nfs_addcllock(vnode_t *vp, struct flock64 *bfp);
extern void	nfs_rmcllock(vnode_t *vp, struct flock64 *bfp);
extern void	nfs_lockrelease(vnode_t *vp, int flag,
		    offset_t offset, cred_t *credp);
extern int	vattr_to_nattr(struct vattr *, struct nfsfattr *);
extern int	mount_root(char *, char *, int, struct nfs_args *);
extern void	nfs_lockcompletion(vnode_t *vp, int cmd);
extern void	nfs_add_locking_id(vnode_t *, pid_t, int, char *, int);
extern void	nfs3copyfh(caddr_t, vnode_t *);
extern void	nfscopyfh(caddr_t, vnode_t *);
extern int	nfs3lookup(vnode_t *, char *, vnode_t **, struct pathname *,
				int, vnode_t *, cred_t *, int);
extern int	nfslookup(vnode_t *, char *, vnode_t **, struct pathname *,
				int, vnode_t *, cred_t *, int);
extern void	sv_free(struct servinfo *);
extern int	nfsauth_access(struct exportinfo *exi, struct svc_req *req);
extern void	nfsauth_init();
extern void	nfsauth_fini();
extern int	nfs_setopts(vnode_t *vp, model_t model, struct nfs_args *args);

extern kmutex_t		nfs_kstat_lock;
extern kstat_named_t	*svstat_ptr;
extern uint_t		svstat_ndata;
extern krwlock_t	rroklock;
extern vtype_t		nf_to_vt[];
extern kstat_named_t	*rfsproccnt_v2_ptr;
extern uint_t		rfsproccnt_v2_ndata;
extern kmutex_t		nfs_rtable_lock;
extern kmutex_t		nfs_minor_lock;
extern int		nfs_major;
extern int		nfs_minor;
extern kstat_named_t	*clstat_ptr;
extern uint_t		clstat_ndata;
extern kstat_t		*nfs_client_kstat;
extern kstat_t		*nfs_server_kstat;
extern struct vfsops	nfs_vfsops;
extern struct vnodeops	nfs_vnodeops;
extern vnode_t		nfs_notfound;
extern int		nfsfstyp;
#endif	/* _KERNEL */

/*
 * Version 3 declarations and definitions.
 */

#define	NFS3_FHSIZE 64
#define	NFS3_COOKIEVERFSIZE 8
#define	NFS3_CREATEVERFSIZE 8
#define	NFS3_WRITEVERFSIZE 8

typedef char *filename3;

typedef char *nfspath3;

#define	nfs3nametoolong	((char *)-1)

typedef uint64 fileid3;

typedef uint64 cookie3;

typedef uint32 uid3;

typedef uint32 gid3;

typedef uint64 size3;

typedef uint64 offset3;

typedef uint32 mode3;

typedef uint32 count3;

typedef char cookieverf3[NFS3_COOKIEVERFSIZE];

typedef char createverf3[NFS3_CREATEVERFSIZE];

typedef char writeverf3[NFS3_WRITEVERFSIZE];

struct nfs_fh3 {
	uint_t fh3_length;
	union nfs_fh3_u {
		struct nfs_fh3_i {
			fhandle_t fh3_i;
		} nfs_fh3_i;
		char data[NFS3_FHSIZE];
	} fh3_u;
};
#define	fh3_fsid	fh3_u.nfs_fh3_i.fh3_i.fh_fsid
#define	fh3_len		fh3_u.nfs_fh3_i.fh3_i.fh_len /* fid length */
#define	fh3_data	fh3_u.nfs_fh3_i.fh3_i.fh_data /* fid bytes */
#define	fh3_xlen	fh3_u.nfs_fh3_i.fh3_i.fh_xlen
#define	fh3_xdata	fh3_u.nfs_fh3_i.fh3_i.fh_xdata
typedef struct nfs_fh3 nfs_fh3;

/*
 * Two elements were added to the
 * diropargs3 structure for performance (xdr-inlining).
 * They are not included as part of the args
 * that are encoded or decoded:
 * dirp - ptr to the nfs_fh3
 * flag indicating when to free the name that was
 * allocated during decode.
 */
struct diropargs3 {
	nfs_fh3 *dirp;
	nfs_fh3 dir;
	filename3 name;
	int flags;
};
typedef struct diropargs3 diropargs3;

struct nfstime3 {
	uint32 seconds;
	uint32 nseconds;
};
typedef struct nfstime3 nfstime3;

struct specdata3 {
	uint32 specdata1;
	uint32 specdata2;
};
typedef struct specdata3 specdata3;

enum nfsstat3 {
	NFS3_OK = 0,
	NFS3ERR_PERM = 1,
	NFS3ERR_NOENT = 2,
	NFS3ERR_IO = 5,
	NFS3ERR_NXIO = 6,
	NFS3ERR_ACCES = 13,
	NFS3ERR_EXIST = 17,
	NFS3ERR_XDEV = 18,
	NFS3ERR_NODEV = 19,
	NFS3ERR_NOTDIR = 20,
	NFS3ERR_ISDIR = 21,
	NFS3ERR_INVAL = 22,
	NFS3ERR_FBIG = 27,
	NFS3ERR_NOSPC = 28,
	NFS3ERR_ROFS = 30,
	NFS3ERR_MLINK = 31,
	NFS3ERR_NAMETOOLONG = 63,
	NFS3ERR_NOTEMPTY = 66,
	NFS3ERR_DQUOT = 69,
	NFS3ERR_STALE = 70,
	NFS3ERR_REMOTE = 71,
	NFS3ERR_BADHANDLE = 10001,
	NFS3ERR_NOT_SYNC = 10002,
	NFS3ERR_BAD_COOKIE = 10003,
	NFS3ERR_NOTSUPP = 10004,
	NFS3ERR_TOOSMALL = 10005,
	NFS3ERR_SERVERFAULT = 10006,
	NFS3ERR_BADTYPE = 10007,
	NFS3ERR_JUKEBOX = 10008
};
typedef enum nfsstat3 nfsstat3;

enum ftype3 {
	NF3REG = 1,
	NF3DIR = 2,
	NF3BLK = 3,
	NF3CHR = 4,
	NF3LNK = 5,
	NF3SOCK = 6,
	NF3FIFO = 7
};
typedef enum ftype3 ftype3;

struct fattr3 {
	ftype3 type;
	mode3 mode;
	uint32 nlink;
	uid3 uid;
	gid3 gid;
	size3 size;
	size3 used;
	specdata3 rdev;
	uint64 fsid;
	fileid3 fileid;
	nfstime3 atime;
	nfstime3 mtime;
	nfstime3 ctime;
};
typedef struct fattr3 fattr3;

struct post_op_attr {
	bool_t attributes;
	fattr3 attr;
};
typedef struct post_op_attr post_op_attr;

struct wcc_attr {
	size3 size;
	nfstime3 mtime;
	nfstime3 ctime;
};
typedef struct wcc_attr wcc_attr;

struct pre_op_attr {
	bool_t attributes;
	wcc_attr attr;
};
typedef struct pre_op_attr pre_op_attr;

struct wcc_data {
	pre_op_attr before;
	post_op_attr after;
};
typedef struct wcc_data wcc_data;

struct post_op_fh3 {
	bool_t handle_follows;
	nfs_fh3 handle;
};
typedef struct post_op_fh3 post_op_fh3;

enum time_how {
	DONT_CHANGE = 0,
	SET_TO_SERVER_TIME = 1,
	SET_TO_CLIENT_TIME = 2
};
typedef enum time_how time_how;

struct set_mode3 {
	bool_t set_it;
	mode3 mode;
};
typedef struct set_mode3 set_mode3;

struct set_uid3 {
	bool_t set_it;
	uid3 uid;
};
typedef struct set_uid3 set_uid3;

struct set_gid3 {
	bool_t set_it;
	gid3 gid;
};
typedef struct set_gid3 set_gid3;

struct set_size3 {
	bool_t set_it;
	size3 size;
};
typedef struct set_size3 set_size3;

struct set_atime {
	time_how set_it;
	nfstime3 atime;
};
typedef struct set_atime set_atime;

struct set_mtime {
	time_how set_it;
	nfstime3 mtime;
};
typedef struct set_mtime set_mtime;

struct sattr3 {
	set_mode3 mode;
	set_uid3 uid;
	set_gid3 gid;
	set_size3 size;
	set_atime atime;
	set_mtime mtime;
};
typedef struct sattr3 sattr3;

/*
 * A couple of defines to make resok and resfail look like the
 * correct things in a response type independent manner.
 */
#define	resok	res_u.ok
#define	resfail	res_u.fail

struct GETATTR3args {
	nfs_fh3 object;
};
typedef struct GETATTR3args GETATTR3args;

struct GETATTR3resok {
	fattr3 obj_attributes;
};
typedef struct GETATTR3resok GETATTR3resok;

struct GETATTR3res {
	nfsstat3 status;
	union {
		GETATTR3resok ok;
	} res_u;
};
typedef struct GETATTR3res GETATTR3res;

struct sattrguard3 {
	bool_t check;
	nfstime3 obj_ctime;
};
typedef struct sattrguard3 sattrguard3;

struct SETATTR3args {
	nfs_fh3 object;
	sattr3 new_attributes;
	sattrguard3 guard;
};
typedef struct SETATTR3args SETATTR3args;

struct SETATTR3resok {
	wcc_data obj_wcc;
};
typedef struct SETATTR3resok SETATTR3resok;

struct SETATTR3resfail {
	wcc_data obj_wcc;
};
typedef struct SETATTR3resfail SETATTR3resfail;

struct SETATTR3res {
	nfsstat3 status;
	union {
		SETATTR3resok ok;
		SETATTR3resfail fail;
	} res_u;
};
typedef struct SETATTR3res SETATTR3res;

struct LOOKUP3args {
	diropargs3 what;
};
typedef struct LOOKUP3args LOOKUP3args;

struct LOOKUP3resok {
	nfs_fh3 object;
	post_op_attr obj_attributes;
	post_op_attr dir_attributes;
};
typedef struct LOOKUP3resok LOOKUP3resok;

struct LOOKUP3resfail {
	post_op_attr dir_attributes;
};
typedef struct LOOKUP3resfail LOOKUP3resfail;

struct LOOKUP3res {
	nfsstat3 status;
	union {
		LOOKUP3resok ok;
		LOOKUP3resfail fail;
	} res_u;
};
typedef struct LOOKUP3res LOOKUP3res;

struct ACCESS3args {
	nfs_fh3 object;
	uint32 access;
};
typedef struct ACCESS3args ACCESS3args;
#define	ACCESS3_READ 0x1
#define	ACCESS3_LOOKUP 0x2
#define	ACCESS3_MODIFY 0x4
#define	ACCESS3_EXTEND 0x8
#define	ACCESS3_DELETE 0x10
#define	ACCESS3_EXECUTE 0x20

struct ACCESS3resok {
	post_op_attr obj_attributes;
	uint32 access;
};
typedef struct ACCESS3resok ACCESS3resok;

struct ACCESS3resfail {
	post_op_attr obj_attributes;
};
typedef struct ACCESS3resfail ACCESS3resfail;

struct ACCESS3res {
	nfsstat3 status;
	union {
		ACCESS3resok ok;
		ACCESS3resfail fail;
	} res_u;
};
typedef struct ACCESS3res ACCESS3res;

struct READLINK3args {
	nfs_fh3 symlink;
};
typedef struct READLINK3args READLINK3args;

struct READLINK3resok {
	post_op_attr symlink_attributes;
	nfspath3 data;
};
typedef struct READLINK3resok READLINK3resok;

struct READLINK3resfail {
	post_op_attr symlink_attributes;
};
typedef struct READLINK3resfail READLINK3resfail;

struct READLINK3res {
	nfsstat3 status;
	union {
		READLINK3resok ok;
		READLINK3resfail fail;
	} res_u;
};
typedef struct READLINK3res READLINK3res;

struct READ3args {
	nfs_fh3 file;
	offset3 offset;
	count3 count;
};
typedef struct READ3args READ3args;

struct READ3resok {
	post_op_attr file_attributes;
	count3 count;
	bool_t eof;
	struct {
		uint_t data_len;
		char *data_val;
		mblk_t *mp;
	} data;
	uint_t size;
};
typedef struct READ3resok READ3resok;

struct READ3resfail {
	post_op_attr file_attributes;
};
typedef struct READ3resfail READ3resfail;

struct READ3res {
	nfsstat3 status;
	union {
		READ3resok ok;
		READ3resfail fail;
	} res_u;
};
typedef struct READ3res READ3res;

enum stable_how {
	UNSTABLE = 0,
	DATA_SYNC = 1,
	FILE_SYNC = 2
};
typedef enum stable_how stable_how;

struct WRITE3args {
	nfs_fh3 file;
	offset3 offset;
	count3 count;
	stable_how stable;
	struct {
		uint_t data_len;
		char *data_val;
	} data;
	mblk_t *mblk;
};
typedef struct WRITE3args WRITE3args;

struct WRITE3resok {
	wcc_data file_wcc;
	count3 count;
	stable_how committed;
	writeverf3 verf;
};
typedef struct WRITE3resok WRITE3resok;

struct WRITE3resfail {
	wcc_data file_wcc;
};
typedef struct WRITE3resfail WRITE3resfail;

struct WRITE3res {
	nfsstat3 status;
	union {
		WRITE3resok ok;
		WRITE3resfail fail;
	} res_u;
};
typedef struct WRITE3res WRITE3res;

enum createmode3 {
	UNCHECKED = 0,
	GUARDED = 1,
	EXCLUSIVE = 2
};
typedef enum createmode3 createmode3;

struct createhow3 {
	createmode3 mode;
	union {
		sattr3 obj_attributes;
		createverf3 verf;
	} createhow3_u;
};
typedef struct createhow3 createhow3;

struct CREATE3args {
	diropargs3 where;
	createhow3 how;
};
typedef struct CREATE3args CREATE3args;

struct CREATE3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct CREATE3resok CREATE3resok;

struct CREATE3resfail {
	wcc_data dir_wcc;
};
typedef struct CREATE3resfail CREATE3resfail;

struct CREATE3res {
	nfsstat3 status;
	union {
		CREATE3resok ok;
		CREATE3resfail fail;
	} res_u;
};
typedef struct CREATE3res CREATE3res;

struct MKDIR3args {
	diropargs3 where;
	sattr3 attributes;
};
typedef struct MKDIR3args MKDIR3args;

struct MKDIR3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct MKDIR3resok MKDIR3resok;

struct MKDIR3resfail {
	wcc_data dir_wcc;
};
typedef struct MKDIR3resfail MKDIR3resfail;

struct MKDIR3res {
	nfsstat3 status;
	union {
		MKDIR3resok ok;
		MKDIR3resfail fail;
	} res_u;
};
typedef struct MKDIR3res MKDIR3res;

struct symlinkdata3 {
	sattr3 symlink_attributes;
	nfspath3 symlink_data;
};
typedef struct symlinkdata3 symlinkdata3;

struct SYMLINK3args {
	diropargs3 where;
	symlinkdata3 symlink;
};
typedef struct SYMLINK3args SYMLINK3args;

struct SYMLINK3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct SYMLINK3resok SYMLINK3resok;

struct SYMLINK3resfail {
	wcc_data dir_wcc;
};
typedef struct SYMLINK3resfail SYMLINK3resfail;

struct SYMLINK3res {
	nfsstat3 status;
	union {
		SYMLINK3resok ok;
		SYMLINK3resfail fail;
	} res_u;
};
typedef struct SYMLINK3res SYMLINK3res;

struct devicedata3 {
	sattr3 dev_attributes;
	specdata3 spec;
};
typedef struct devicedata3 devicedata3;

struct mknoddata3 {
	ftype3 type;
	union {
		devicedata3 device;
		sattr3 pipe_attributes;
	} mknoddata3_u;
};
typedef struct mknoddata3 mknoddata3;

struct MKNOD3args {
	diropargs3 where;
	mknoddata3 what;
};
typedef struct MKNOD3args MKNOD3args;

struct MKNOD3resok {
	post_op_fh3 obj;
	post_op_attr obj_attributes;
	wcc_data dir_wcc;
};
typedef struct MKNOD3resok MKNOD3resok;

struct MKNOD3resfail {
	wcc_data dir_wcc;
};
typedef struct MKNOD3resfail MKNOD3resfail;

struct MKNOD3res {
	nfsstat3 status;
	union {
		MKNOD3resok ok;
		MKNOD3resfail fail;
	} res_u;
};
typedef struct MKNOD3res MKNOD3res;

struct REMOVE3args {
	diropargs3 object;
};
typedef struct REMOVE3args REMOVE3args;

struct REMOVE3resok {
	wcc_data dir_wcc;
};
typedef struct REMOVE3resok REMOVE3resok;

struct REMOVE3resfail {
	wcc_data dir_wcc;
};
typedef struct REMOVE3resfail REMOVE3resfail;

struct REMOVE3res {
	nfsstat3 status;
	union {
		REMOVE3resok ok;
		REMOVE3resfail fail;
	} res_u;
};
typedef struct REMOVE3res REMOVE3res;

struct RMDIR3args {
	diropargs3 object;
};
typedef struct RMDIR3args RMDIR3args;

struct RMDIR3resok {
	wcc_data dir_wcc;
};
typedef struct RMDIR3resok RMDIR3resok;

struct RMDIR3resfail {
	wcc_data dir_wcc;
};
typedef struct RMDIR3resfail RMDIR3resfail;

struct RMDIR3res {
	nfsstat3 status;
	union {
		RMDIR3resok ok;
		RMDIR3resfail fail;
	} res_u;
};
typedef struct RMDIR3res RMDIR3res;

struct RENAME3args {
	diropargs3 from;
	diropargs3 to;
};
typedef struct RENAME3args RENAME3args;

struct RENAME3resok {
	wcc_data fromdir_wcc;
	wcc_data todir_wcc;
};
typedef struct RENAME3resok RENAME3resok;

struct RENAME3resfail {
	wcc_data fromdir_wcc;
	wcc_data todir_wcc;
};
typedef struct RENAME3resfail RENAME3resfail;

struct RENAME3res {
	nfsstat3 status;
	union {
		RENAME3resok ok;
		RENAME3resfail fail;
	} res_u;
};
typedef struct RENAME3res RENAME3res;

struct LINK3args {
	nfs_fh3 file;
	diropargs3 link;
};
typedef struct LINK3args LINK3args;

struct LINK3resok {
	post_op_attr file_attributes;
	wcc_data linkdir_wcc;
};
typedef struct LINK3resok LINK3resok;

struct LINK3resfail {
	post_op_attr file_attributes;
	wcc_data linkdir_wcc;
};
typedef struct LINK3resfail LINK3resfail;

struct LINK3res {
	nfsstat3 status;
	union {
		LINK3resok ok;
		LINK3resfail fail;
	} res_u;
};
typedef struct LINK3res LINK3res;

struct READDIR3args {
	nfs_fh3 dir;
	cookie3 cookie;
	cookieverf3 cookieverf;
	count3 count;
};
typedef struct READDIR3args READDIR3args;

struct entry3 {
	fileid3 fileid;
	filename3 name;
	cookie3 cookie;
	struct entry3 *nextentry;
};
typedef struct entry3 entry3;

struct dirlist3 {
	entry3 *entries;
	bool_t eof;
};
typedef struct dirlist3 dirlist3;

struct READDIR3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlist3 reply;
	uint_t size;
	uint_t count;
	cookie3 cookie;
};
typedef struct READDIR3resok READDIR3resok;

struct READDIR3resfail {
	post_op_attr dir_attributes;
};
typedef struct READDIR3resfail READDIR3resfail;

struct READDIR3res {
	nfsstat3 status;
	union {
		READDIR3resok ok;
		READDIR3resfail fail;
	} res_u;
};
typedef struct READDIR3res READDIR3res;

struct READDIRPLUS3args {
	nfs_fh3 dir;
	cookie3 cookie;
	cookieverf3 cookieverf;
	count3 dircount;
	count3 maxcount;
};
typedef struct READDIRPLUS3args READDIRPLUS3args;

struct entryplus3 {
	fileid3 fileid;
	filename3 name;
	cookie3 cookie;
	post_op_attr name_attributes;
	post_op_fh3 name_handle;
	struct entryplus3 *nextentry;
};
typedef struct entryplus3 entryplus3;

struct dirlistplus3 {
	entryplus3 *entries;
	bool_t eof;
};
typedef struct dirlistplus3 dirlistplus3;

struct READDIRPLUS3resok {
	post_op_attr dir_attributes;
	cookieverf3 cookieverf;
	dirlistplus3 reply;
	uint_t size;
	uint_t count;
	uint_t maxcount;
	post_op_attr *attributes;
	post_op_fh3 *handles;
};
typedef struct READDIRPLUS3resok READDIRPLUS3resok;

struct READDIRPLUS3resfail {
	post_op_attr dir_attributes;
};
typedef struct READDIRPLUS3resfail READDIRPLUS3resfail;

struct READDIRPLUS3res {
	nfsstat3 status;
	union {
		READDIRPLUS3resok ok;
		READDIRPLUS3resfail fail;
	} res_u;
};
typedef struct READDIRPLUS3res READDIRPLUS3res;

struct FSSTAT3args {
	nfs_fh3 fsroot;
};
typedef struct FSSTAT3args FSSTAT3args;

struct FSSTAT3resok {
	post_op_attr obj_attributes;
	size3 tbytes;
	size3 fbytes;
	size3 abytes;
	size3 tfiles;
	size3 ffiles;
	size3 afiles;
	uint32 invarsec;
};
typedef struct FSSTAT3resok FSSTAT3resok;

struct FSSTAT3resfail {
	post_op_attr obj_attributes;
};
typedef struct FSSTAT3resfail FSSTAT3resfail;

struct FSSTAT3res {
	nfsstat3 status;
	union {
		FSSTAT3resok ok;
		FSSTAT3resfail fail;
	} res_u;
};
typedef struct FSSTAT3res FSSTAT3res;

struct FSINFO3args {
	nfs_fh3 fsroot;
};
typedef struct FSINFO3args FSINFO3args;

struct FSINFO3resok {
	post_op_attr obj_attributes;
	uint32 rtmax;
	uint32 rtpref;
	uint32 rtmult;
	uint32 wtmax;
	uint32 wtpref;
	uint32 wtmult;
	uint32 dtpref;
	size3 maxfilesize;
	nfstime3 time_delta;
	uint32 properties;
};
typedef struct FSINFO3resok FSINFO3resok;

struct FSINFO3resfail {
	post_op_attr obj_attributes;
};
typedef struct FSINFO3resfail FSINFO3resfail;
#define	FSF3_LINK 0x1
#define	FSF3_SYMLINK 0x2
#define	FSF3_HOMOGENEOUS 0x8
#define	FSF3_CANSETTIME 0x10

struct FSINFO3res {
	nfsstat3 status;
	union {
		FSINFO3resok ok;
		FSINFO3resfail fail;
	} res_u;
};
typedef struct FSINFO3res FSINFO3res;

struct PATHCONF3args {
	nfs_fh3 object;
};
typedef struct PATHCONF3args PATHCONF3args;

struct nfs3_pathconf_info {
	uint32 link_max;
	uint32 name_max;
	bool_t no_trunc;
	bool_t chown_restricted;
	bool_t case_insensitive;
	bool_t case_preserving;
};
typedef struct nfs3_pathconf_info nfs3_pathconf_info;

struct PATHCONF3resok {
	post_op_attr obj_attributes;
	nfs3_pathconf_info info;
};
typedef struct PATHCONF3resok PATHCONF3resok;

struct PATHCONF3resfail {
	post_op_attr obj_attributes;
};
typedef struct PATHCONF3resfail PATHCONF3resfail;

struct PATHCONF3res {
	nfsstat3 status;
	union {
		PATHCONF3resok ok;
		PATHCONF3resfail fail;
	} res_u;
};
typedef struct PATHCONF3res PATHCONF3res;

struct COMMIT3args {
	nfs_fh3 file;
	offset3 offset;
	count3 count;
};
typedef struct COMMIT3args COMMIT3args;

struct COMMIT3resok {
	wcc_data file_wcc;
	writeverf3 verf;
};
typedef struct COMMIT3resok COMMIT3resok;

struct COMMIT3resfail {
	wcc_data file_wcc;
};
typedef struct COMMIT3resfail COMMIT3resfail;

struct COMMIT3res {
	nfsstat3 status;
	union {
		COMMIT3resok ok;
		COMMIT3resfail fail;
	} res_u;
};
typedef struct COMMIT3res COMMIT3res;

#define	NFS3_PROGRAM ((rpcprog_t)100003)
#define	NFS_V3 ((rpcvers_t)3)
#define	NFSPROC3_NULL ((rpcproc_t)0)
extern  void * nfsproc3_null_3();
#define	NFSPROC3_GETATTR ((rpcproc_t)1)
extern  GETATTR3res * nfsproc3_getattr_3();
#define	NFSPROC3_SETATTR ((rpcproc_t)2)
extern  SETATTR3res * nfsproc3_setattr_3();
#define	NFSPROC3_LOOKUP ((rpcproc_t)3)
extern  LOOKUP3res * nfsproc3_lookup_3();
#define	NFSPROC3_ACCESS ((rpcproc_t)4)
extern  ACCESS3res * nfsproc3_access_3();
#define	NFSPROC3_READLINK ((rpcproc_t)5)
extern  READLINK3res * nfsproc3_readlink_3();
#define	NFSPROC3_READ ((rpcproc_t)6)
extern  READ3res * nfsproc3_read_3();
#define	NFSPROC3_WRITE ((rpcproc_t)7)
extern  WRITE3res * nfsproc3_write_3();
#define	NFSPROC3_CREATE ((rpcproc_t)8)
extern  CREATE3res * nfsproc3_create_3();
#define	NFSPROC3_MKDIR ((rpcproc_t)9)
extern  MKDIR3res * nfsproc3_mkdir_3();
#define	NFSPROC3_SYMLINK ((rpcproc_t)10)
extern  SYMLINK3res * nfsproc3_symlink_3();
#define	NFSPROC3_MKNOD ((rpcproc_t)11)
extern  MKNOD3res * nfsproc3_mknod_3();
#define	NFSPROC3_REMOVE ((rpcproc_t)12)
extern  REMOVE3res * nfsproc3_remove_3();
#define	NFSPROC3_RMDIR ((rpcproc_t)13)
extern  RMDIR3res * nfsproc3_rmdir_3();
#define	NFSPROC3_RENAME ((rpcproc_t)14)
extern  RENAME3res * nfsproc3_rename_3();
#define	NFSPROC3_LINK ((rpcproc_t)15)
extern  LINK3res * nfsproc3_link_3();
#define	NFSPROC3_READDIR ((rpcproc_t)16)
extern  READDIR3res * nfsproc3_readdir_3();
#define	NFSPROC3_READDIRPLUS ((rpcproc_t)17)
extern  READDIRPLUS3res * nfsproc3_readdirplus_3();
#define	NFSPROC3_FSSTAT ((rpcproc_t)18)
extern  FSSTAT3res * nfsproc3_fsstat_3();
#define	NFSPROC3_FSINFO ((rpcproc_t)19)
extern  FSINFO3res * nfsproc3_fsinfo_3();
#define	NFSPROC3_PATHCONF ((rpcproc_t)20)
extern  PATHCONF3res * nfsproc3_pathconf_3();
#define	NFSPROC3_COMMIT ((rpcproc_t)21)
extern  COMMIT3res * nfsproc3_commit_3();

#ifdef _KERNEL
/* the NFS Version 3 XDR functions */

extern bool_t xdr_nfs_fh3(XDR *, nfs_fh3 *);
extern bool_t xdr_diropargs3(XDR *, diropargs3 *);
extern bool_t xdr_fattr3(XDR *, fattr3 *);
extern bool_t xdr_post_op_attr(XDR *, post_op_attr *);
extern bool_t xdr_wcc_attr(XDR *, wcc_attr *);
extern bool_t xdr_pre_op_attr(XDR *, pre_op_attr *);
extern bool_t xdr_wcc_data(XDR *, wcc_data *);
extern bool_t xdr_post_op_fh3(XDR *, post_op_fh3 *);
extern bool_t xdr_sattr3(XDR *, sattr3 *);
extern bool_t xdr_GETATTR3resok(XDR *, GETATTR3resok *);
extern bool_t xdr_GETATTR3res(XDR *, GETATTR3res *);
extern bool_t xdr_sattrguard3(XDR *, sattrguard3 *);
extern bool_t xdr_SETATTR3args(XDR *, SETATTR3args *);
extern bool_t xdr_SETATTR3res(XDR *, SETATTR3res *);
extern bool_t xdr_LOOKUP3res(XDR *, LOOKUP3res *);
extern bool_t xdr_ACCESS3args(XDR *, ACCESS3args *);
extern bool_t xdr_ACCESS3res(XDR *, ACCESS3res *);
extern bool_t xdr_READLINK3args(XDR *, READLINK3args *);
extern bool_t xdr_READLINK3res(XDR *, READLINK3res *);
extern bool_t xdr_READ3args(XDR *, READ3args *);
extern bool_t xdr_READ3resok(XDR *, READ3resok *);
extern bool_t xdr_READ3res(XDR *, READ3res *);
extern bool_t xdr_WRITE3args(XDR *, WRITE3args *);
extern bool_t xdr_WRITE3res(XDR *, WRITE3res *);
extern bool_t xdr_CREATE3args(XDR *, CREATE3args *);
extern bool_t xdr_CREATE3res(XDR *, CREATE3res *);
extern bool_t xdr_MKDIR3args(XDR *, MKDIR3args *);
extern bool_t xdr_MKDIR3res(XDR *, MKDIR3res *);
extern bool_t xdr_SYMLINK3args(XDR *, SYMLINK3args *);
extern bool_t xdr_SYMLINK3res(XDR *, SYMLINK3res *);
extern bool_t xdr_MKNOD3args(XDR *, MKNOD3args *);
extern bool_t xdr_MKNOD3res(XDR *, MKNOD3res *);
extern bool_t xdr_REMOVE3res(XDR *, REMOVE3res *);
extern bool_t xdr_RMDIR3resfail(XDR *, RMDIR3resfail *);
extern bool_t xdr_RMDIR3res(XDR *, RMDIR3res *);
extern bool_t xdr_RENAME3args(XDR *, RENAME3args *);
extern bool_t xdr_RENAME3res(XDR *, RENAME3res *);
extern bool_t xdr_LINK3args(XDR *, LINK3args *);
extern bool_t xdr_LINK3res(XDR *, LINK3res *);
extern bool_t xdr_READDIR3args(XDR *, READDIR3args *);
extern bool_t xdr_putdirlist(XDR *, READDIR3resok *);
extern bool_t xdr_getdirlist(XDR *, READDIR3resok *);
extern bool_t xdr_READDIR3res(XDR *, READDIR3res *);
extern bool_t xdr_READDIRPLUS3args(XDR *, READDIRPLUS3args *);
extern bool_t xdr_putdirpluslist(XDR *, READDIRPLUS3resok *);
extern bool_t xdr_getdirpluslist(XDR *, READDIRPLUS3resok *);
extern bool_t xdr_READDIRPLUS3res(XDR *, READDIRPLUS3res *);
extern bool_t xdr_FSSTAT3res(XDR *, FSSTAT3res *);
extern bool_t xdr_FSINFO3res(XDR *, FSINFO3res *);
extern bool_t xdr_PATHCONF3res(XDR *, PATHCONF3res *);
extern bool_t xdr_COMMIT3args(XDR *, COMMIT3args *);
extern bool_t xdr_COMMIT3res(XDR *, COMMIT3res *);
extern bool_t xdr_fastnfs_fh3(XDR *, nfs_fh3 **);
extern bool_t xdr_fastdiropargs3(XDR *, diropargs3 **);
#endif

#ifdef _KERNEL
/*
 * The NFS Version 3 service procedures.
 */
struct exportinfo;	/* defined in nfs/export.h */
struct servinfo;	/* defined in nfs/nfs_clnt.h */
struct mntinfo;		/* defined in nfs/nfs_clnt.h */
struct sec_ol;		/* defined in nfs/export.h */

extern void rfs3_getattr(GETATTR3args *, GETATTR3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_getattr_getfh(GETATTR3args *);
extern void rfs3_setattr(SETATTR3args *, SETATTR3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_setattr_getfh(SETATTR3args *);
extern void rfs3_lookup(LOOKUP3args *, LOOKUP3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_lookup_getfh(LOOKUP3args *);
extern void rfs3_access(ACCESS3args *, ACCESS3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_access_getfh(ACCESS3args *);
extern void rfs3_readlink(READLINK3args *, READLINK3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_readlink_getfh(READLINK3args *);
extern void rfs3_readlink_free(READLINK3res *);
extern void rfs3_read(READ3args *, READ3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_read_getfh(READ3args *);
extern void rfs3_read_free(READ3res *);
extern void rfs3_write(WRITE3args *, WRITE3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_write_getfh(WRITE3args *);
extern void rfs3_create(CREATE3args *, CREATE3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_create_getfh(CREATE3args *);
extern void rfs3_mkdir(MKDIR3args *, MKDIR3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_mkdir_getfh(MKDIR3args *);
extern void rfs3_symlink(SYMLINK3args *, SYMLINK3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_symlink_getfh(SYMLINK3args *);
extern void rfs3_mknod(MKNOD3args *, MKNOD3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_mknod_getfh(MKNOD3args *);
extern void rfs3_remove(REMOVE3args *, REMOVE3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_remove_getfh(REMOVE3args *);
extern void rfs3_rmdir(RMDIR3args *, RMDIR3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_rmdir_getfh(RMDIR3args *);
extern void rfs3_rename(RENAME3args *, RENAME3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_rename_getfh(RENAME3args *);
extern void rfs3_link(LINK3args *, LINK3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_link_getfh(LINK3args *);
extern void rfs3_readdir(READDIR3args *, READDIR3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_readdir_getfh(READDIR3args *);
extern void rfs3_readdir_free(READDIR3res *);
extern void rfs3_readdirplus(READDIRPLUS3args *, READDIRPLUS3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_readdirplus_getfh(READDIRPLUS3args *);
extern void rfs3_readdirplus_free(READDIRPLUS3res *);
extern void rfs3_fsstat(FSSTAT3args *, FSSTAT3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_fsstat_getfh(FSSTAT3args *);
extern void rfs3_fsinfo(FSINFO3args *, FSINFO3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_fsinfo_getfh(FSINFO3args *);
extern void rfs3_pathconf(PATHCONF3args *, PATHCONF3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_pathconf_getfh(PATHCONF3args *);
extern void rfs3_commit(COMMIT3args *, COMMIT3res *,
			struct exportinfo *, struct svc_req *, cred_t *);
extern fhandle_t *rfs3_commit_getfh(COMMIT3args *);
extern void rfs3_srvrinit(void);
extern void rfs3_srvrfini(void);
#endif

#ifdef _KERNEL
extern int	nfs3_validate_caches(vnode_t *, cred_t *);
extern void	nfs3_cache_check(vnode_t *, wcc_attr *, int *, cred_t *);
extern void	nfs3_cache_check_fattr3(vnode_t *, fattr3 *, int *, cred_t *);
extern void	nfs3_cache_post_op_attr(vnode_t *, post_op_attr *, cred_t *);
extern void	nfs3_cache_wcc_data(vnode_t *, wcc_data *, cred_t *);
extern void	nfs3_check_wcc_data(vnode_t *, wcc_data *);
extern void	nfs3_attrcache(vnode_t *, fattr3 *, int);
extern int	nfs3_getattr_otw(vnode_t *, struct vattr *, cred_t *);
extern int	nfs3getattr(vnode_t *, struct vattr *, cred_t *);
extern int	fattr3_to_vattr(vnode_t *, fattr3 *, struct vattr *);
extern int	nfs3tsize(void);
extern int	vattr_to_sattr3(struct vattr *, sattr3 *);
extern void	setdiropargs3(diropargs3 *, char *, vnode_t *);
extern enum nfsstat3 puterrno3(int);
extern int	geterrno3(enum nfsstat3);
extern int	nfs3init(struct vfssw *, int);
extern void	nfs3fini(void);
extern int	nfs3_vfsinit(void);
extern void	nfs3_vfsfini(void);
extern void	vattr_to_post_op_attr(struct vattr *, post_op_attr *);
extern void	mblk_to_iov(mblk_t *, struct iovec *);
extern int	rfs_publicfh_mclookup(char *, vnode_t *, cred_t *,
			vnode_t **, struct exportinfo **, struct sec_ol *);
extern int	rfs_pathname(char *, vnode_t **, vnode_t **,
			vnode_t *, cred_t *, int);
extern vtype_t		nf3_to_vt[];
extern kstat_named_t	*rfsproccnt_v3_ptr;
extern uint_t		rfsproccnt_v3_ndata;
extern struct vfsops	nfs3_vfsops;
extern struct vnodeops	nfs3_vnodeops;
extern vnode_t		nfs3_notfound;
#ifdef DEBUG
extern int		rfs3_do_pre_op_attr;
extern int		rfs3_do_post_op_attr;
#endif
/*
 * Some servers do not properly update the attributes of the
 * directory when changes are made.  To allow interoperability
 * with these broken servers, the nfs_disable_rddir_cache
 * parameter can be used to disable readdir response caching.
 */
extern int		nfs_disable_rddir_cache;

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _NFS_NFS_H */
