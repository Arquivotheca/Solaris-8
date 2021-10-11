/*
 * Copyright (c) 1990-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_BYTEORDER_H
#define	_BYTEORDER_H

#pragma ident	"@(#)byteorder.h	1.12	99/01/22 SMI"

#include <stdio.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_acl.h>
#include <protocols/dumprestore.h>
#include <assert.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct byteorder_ctx {
	int initialized;
	int Bcvt;
};

#ifdef __STDC__
extern struct byteorder_ctx *byteorder_create(void);
extern void byteorder_destroy(struct byteorder_ctx *);
extern void byteorder_banner(struct byteorder_ctx *, FILE *);
extern void swabst(char *, uchar_t *);
extern uint32_t swabl(uint32_t);
extern int normspcl(struct byteorder_ctx *, struct s_spcl *, int *, int, int);
extern void normdirect(struct byteorder_ctx *, struct direct *);
extern void normacls(struct byteorder_ctx *, ufs_acl_t *, int);
#else /* __STDC__ */
extern struct byteorder_ctx *byteorder_create();
extern void byteorder_destroy();
extern void byteorder_banner();
extern void swabst();
extern uint32_t swabl();
extern int normspcl();
extern void normdirect();
extern void normacls();
#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif /* _BYTEORDER_H */
