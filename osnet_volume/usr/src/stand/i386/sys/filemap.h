/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_I386_SYS_FILEMAP_H
#define	_I386_SYS_FILEMAP_H

#pragma ident	"@(#)filemap.h	1.2	97/04/02 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *  Solaris.map --
 *  A mechanism that allows us to give the illusion that files are in the
 *  root file system when actually they are on a different file system.
 */
#define	MAPFILE	"\\SOLARIS.MAP"
#define	COMPFS_AUGMENT	0x1000	/* set by c flag in SOLARIS.MAP */
#define	COMPFS_TEXT	0x2000	/* set by t flag in SOLARIS.MAP */
#define	COMPFS_PATH	0x4000	/* set by p flag in SOLARIS.MAP */
#define	COMPFS_DECOMP	0x8000	/* set by z flag in SOLARIS.MAP */

struct map_entry {
	char	*target;
	char	*source;
	int	flags;
	struct map_entry *link;
};

extern void cpfs_add_entry(char *, char *, int);
extern void cpfs_show_entries(void);
extern int cpfs_isamapdir(char *);

#ifdef	__cplusplus
}
#endif

#endif /* _I386_SYS_FILEMAP_H */
