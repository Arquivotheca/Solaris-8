/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.  All Rights Reserved.
 */

#ifndef	_SYS_FILEP_H
#define	_SYS_FILEP_H

#pragma ident	"@(#)filep.h	1.5	96/01/11 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_inode.h>

/*
 *  These structs should be invisible to the caller of
 *  the user-level routines
 */
typedef struct dev_ident {	/* device identifier block */
	char		*di_desc;
	int		di_dcookie;
	char		di_taken;
	union {
		struct	fs	di_fs;
		char	dummy[SBSIZE];
	}		un_fs;
} devid_t;

typedef struct file_ident {	/* file identifier block */
	u_int		fi_filedes;
	char		*fi_path;
	u_int		fi_blocknum;
	u_int		fi_count;
	u_int		fi_offset;
	caddr_t		fi_memp;
	char		fi_taken;
	devid_t		*fi_devp;
	char		fi_buf[MAXBSIZE];
	struct	inode	*fi_inode;
	struct file_ident *fi_forw;
	struct file_ident *fi_back;
}  fileid_t;

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_FILEP_H */
