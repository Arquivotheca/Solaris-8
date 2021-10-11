/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SNOOP_NFS_H
#define	_SNOOP_NFS_H

#pragma ident	"@(#)snoop_nfs.h	1.1	99/08/20 SMI"

/*
 * Definitions that are shared among the NFS-related interpreters.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern char *sum_nfsfh(void);
extern void detail_nfsfh(void);
extern int sum_nfsstat(char *);
extern int detail_nfsstat(void);
extern void detail_fattr(void);
extern void skip_fattr(void);
extern int sum_filehandle(int);

extern char *sum_nfsfh3(void);
extern void detail_nfsfh3(void);
extern int sum_nfsstat3(char *);
extern void detail_post_op_attr(char *);
extern int detail_nfsstat3(void);

#ifdef __cplusplus
}
#endif

#endif /* _SNOOP_NFS_H */
