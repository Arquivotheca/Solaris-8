
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_CHECKMOUNT_H
#define	_CHECKMOUNT_H

#pragma ident	"@(#)checkmount.h	1.4	96/05/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *	Prototypes for ANSI C
 */
int	checkmount(daddr_t start, daddr_t end);
int	checkswap(daddr_t start, daddr_t end);
int	check_label_with_mount(void);
int	check_label_with_swap(void);
int	getpartition(char *pathname);
int	checkpartitions(int mounted);


#ifdef	__cplusplus
}
#endif

#endif	/* _CHECKMOUNT_H */
