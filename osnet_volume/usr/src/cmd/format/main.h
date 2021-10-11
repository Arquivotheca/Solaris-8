
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_MAIN_H
#define	_MAIN_H

#pragma ident	"@(#)main.h	1.5	93/09/07 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI C compilers
 */
void	main(int argc, char *argv[]);
int	notify_unix(void);
void	init_globals(struct disk_info *disk);
void	get_disk_characteristics(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _MAIN_H */
