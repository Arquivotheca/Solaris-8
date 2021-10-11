/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_VOLMGT_FSI_PRIVATE_H
#define	_VOLMGT_FSI_PRIVATE_H

#pragma ident	"@(#)volmgt_fsi_private.h	1.4	97/09/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * this file defines the interface between the FSI routines and
 * the database-like routines that support them
 *
 * also included are declarations that the FSI routines use and which
 * may be useful to the database implementation module
 */


/*
 * defines for volmgt device advisory locking (aka Floppy Summit Interace)
 */
#define	VOL_MAXIDLEN	256	/* max length of id_tag string */

/*
 * the volmgt device reservation database consists of ASCII records
 * composed of 4 fields:
 *
 *	dev major||dev minor||pid||Identification string(id)
 *
 *	where:
 *		dev = the major/minor device pair that uniquely identifies
 *			the device that is reserved
 *
 *		pid = the process identifier of the reserving process
 *
 *		id  = a character string that the reserving process wants
 *			returned to other processes attempting to reserve this
 *			device
 */

typedef struct	vol_db_entry {
	major_t	dev_major;	/* device for reservation (major) */
	minor_t dev_minor;	/* device for reservation (minor) */
	pid_t	pid;		/* process id of the reserver */
	char	*id_tag;	/* identifier string of the reserver */
} vol_db_entry_t;


/*
 * dbid type
 */
typedef	int	vol_dbid_t;

extern vol_dbid_t	vol_db_open(void);
extern int		vol_db_close(vol_dbid_t);
extern int		vol_db_insert(vol_dbid_t, vol_db_entry_t *);
extern int		vol_db_remove(vol_dbid_t, dev_t);
extern vol_db_entry_t	*vol_db_find(vol_dbid_t, dev_t);
extern void		vol_db_free(vol_db_entry_t *);
extern int		vol_db_proc_find(pid_t);


#ifdef	__cplusplus
}
#endif

#endif	/* _VOLMGT_FSI_PRIVATE_H */
