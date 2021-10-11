/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990, 1991 SMI	*/
/*	  All Rights Reserved						*/


#pragma ident	"@(#)fslib.h	1.5	99/09/23 SMI"

#ifndef	_fslib_h
#define	_fslib_h

#include	<sys/mnttab.h>

/*
 * This structure is used to build a list of
 * mnttab structures from /etc/mnttab.
 */
typedef struct mntlist {
	int		mntl_flags;
	uint_t		mntl_dev;
	struct extmnttab *mntl_mnt;
	struct mntlist	*mntl_next;
} mntlist_t;

/*
 * Bits for mntl_flags.
 */
#define	MNTL_UNMOUNT	0x01	/* unmount this entry */
#define	MNTL_DIRECT	0x02	/* direct mount entry */

/*
 * Routines available in fslib.c:
 */
void			fsfreemnttab(struct extmnttab *);
struct extmnttab 	*fsdupmnttab(struct extmnttab *);
void			fsfreemntlist(mntlist_t *);

mntlist_t	*fsmkmntlist(FILE *);
mntlist_t	*fsgetmntlist();
mntlist_t	*fsgetmlast(mntlist_t *, struct mnttab *);
void	cmp_requested_to_actual_options(char *, char *, char *, char *);

int	fsgetmlevel(char *);
int	fsstrinlist(const char *, const char **);

#undef MIN
#undef MAX
#define	MIN(a, b)	((a) < (b) ? (a) : (b))
#define	MAX(a, b)	((a) > (b) ? (a) : (b))

#endif	/* !_fslib_h */
