/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_FS_S5_FBLK_H
#define	_SYS_FS_S5_FBLK_H

#pragma ident	"@(#)s5_fblk.h	1.2	93/11/01 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	FBLKP	(struct	fblk *)
struct	fblk {
	int	df_nfree;
	daddr_t	df_free[NICFREE];
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FS_S5_FBLK_H */
