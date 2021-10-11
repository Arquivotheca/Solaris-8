
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_MENU_COMMAND_H
#define	_MENU_COMMAND_H

#pragma ident	"@(#)menu_command.h	1.6	93/11/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


/*
 *	Prototypes for ANSI
 */
int	c_disk(void);
int	c_type(void);
int	c_partition(void);
int	c_current(void);
int	c_format(void);
int	c_fdisk(void);
int	c_repair(void);
int	c_show(void);
int	c_label(void);
int	c_analyze(void);
int	c_defect(void);
int	c_backup(void);
int	c_volname(void);
int	c_verify(void);
int	c_inquiry(void);


extern slist_t	ptag_choices[];
extern slist_t	pflag_choices[];

#ifdef	__cplusplus
}
#endif

#endif	/* _MENU_COMMAND_H */
