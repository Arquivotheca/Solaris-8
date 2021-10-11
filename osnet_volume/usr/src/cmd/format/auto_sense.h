
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_AUTO_SENSE_H
#define	_AUTO_SENSE_H

#pragma ident	"@(#)auto_sense.h	1.2	96/06/13 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#ifdef	__STDC__
/*
 *	Prototypes for ANSI C compilers
 */
struct disk_type	*auto_sense(
				int		fd,
				int		can_prompt,
				struct dk_label	*label);

int			build_default_partition(struct dk_label *label);

#else

struct disk_type	*auto_sense();
int			build_default_partition();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _AUTO_SENSE_H */
