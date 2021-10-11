/*
 *
 *			res.h
 *
 *   Defines routines to operate on the resource file.
 */

#ifndef	_RES_H
#define	_RES_H

#pragma ident "@(#)res.h   1.9     96/01/16 SMI"

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	cfs_fsck_res_h
#define	cfs_fsck_res_h

typedef struct res res;

res *res_create(char *namep, int entries, int verbose);
void res_destroy(res *resp);
int res_done(res *resp);
void res_addfile(res *resp, long nbytes);
int res_addident(res *resp, int index, rl_entry_t *dp, long nbytes, int file);
int res_clearident(res *resp, int index, int nbytes, int file);

#endif /* cfs_fsck_res_h */

#endif /* _RES_H */
