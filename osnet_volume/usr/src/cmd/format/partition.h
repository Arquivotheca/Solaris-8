
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_PARTITION_H
#define	_PARTITION_H

#pragma ident	"@(#)partition.h	1.4	93/03/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI C compilers
 */
void	change_partition(int num);
int	get_partition(void);
void	make_partition(void);
void	delete_partition(struct partition_info *parts);
void	set_vtoc_defaults(struct partition_info	*part);


extern	struct dk_map2	default_vtoc_map[NDKMAP];

#ifdef	__cplusplus
}
#endif

#endif	/* _PARTITION_H */
