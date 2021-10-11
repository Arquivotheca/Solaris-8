
/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_LABEL_H
#define	_LABEL_H

#pragma ident	"@(#)label.h	1.5	93/03/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Prototypes for ANSI C compilers
 */
int	checklabel(struct dk_label *label);
int	checksum(struct dk_label *label, int mode);
int	trim_id(char *id);
int	write_label(void);
int	read_label(int fd, struct dk_label *label);
int	vtoc_to_label(struct dk_label *label, struct vtoc *vtoc,
		struct dk_geom *geom);
int	label_to_vtoc(struct vtoc *vtoc, struct dk_label *label);

#ifdef	__cplusplus
}
#endif

#endif	/* _LABEL_H */
