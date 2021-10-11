
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_ADD_DEFINITION_H
#define	_ADD_DEFINITION_H

#pragma ident	"@(#)add_definition.h	1.3	93/03/26 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI C compilers
 */
int	add_definition(void);
void	add_disktype(FILE *fd, struct disk_info *disk_info);
void	add_partition(FILE *fd, struct disk_info *,
		struct partition_info *);
int	add_entry(int col, FILE *fd, char *format, ...);

#ifdef	__cplusplus
}
#endif

#endif	/* _ADD_DEFINITION_H */
