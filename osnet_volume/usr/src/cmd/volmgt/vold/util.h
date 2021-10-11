/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __UTIL_H
#define	__UTIL_H

#pragma ident	"@(#)util.h	1.20	97/08/12 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * General queue structure
 */
struct q {
	struct q	*q_next;
#define	q_head	q_next
	struct q	*q_prev;
#define	q_tail	q_prev
};

void my_remque(struct q *, struct q *);
void my_insque(struct q *, struct q *);

#define	INSQUE(head, ptr) my_insque(&(head), &(ptr)->q)
#define	REMQUE(head, ptr) my_remque(&(head), &(ptr)->q)
#define	HEAD(type, head) ((type *)(head.q_head))
#define	NEXT(type, ptr)	((type *)(ptr->q.q_next))
#define	TAIL(type, head) ((type *)(head.q_tail))
#define	PREV(type, ptr)	((type *)(ptr->q.q_prev))

/*
 * This is the maximum block offset of a CD-ROM.  This is calculated
 * from 640 sectors/cylinder * 2048 cylinders.  It is used to cull out
 * bad (invalid) partitions in a Sun label.
 */
#define	PART_MAXCDROM	1310720
#define	PART_INF	0xffffffff

/* don't need this on intel -- no backwards compat. to worry about */
void		partition_conv(struct vtoc *, u_int, u_char *, u_char *);
void		partition_conv_2(struct vtoc *, u_int, u_long *, u_char *);
int		partition_low(struct vtoc *);

char 		*location_newdev(char *, char *);
dev_t		location_localdev(char *);

/*
 * property list management functions.
 */
char 		*prop_attr_del(char *, char *);
char 		*prop_attr_get(char *, char *);
char 		*prop_attr_put(char *, char *, char *);
char		*prop_attr_merge(char *, char *);

char		*props_get(struct vol *);
void		props_set(struct vol *, char *);
void		props_merge(struct vol *, struct vol *);

dev_t		minor_alloc(struct vol *);
void		minor_free(minor_t);
struct vol	*minor_getvol(minor_t);

char		*makename(char *, size_t);

char	 	*path_make(struct vvnode *);
u_int	 	path_type(struct vvnode *);
char 		*path_nis(char *);
char 		*path_unnis(char *);
char		**path_split(char *);
void		path_freeps(char **);
char		*path_mntrename(char *, char *, char *);

char 		*mnt_special_test(char *);
char 		*mnt_mp_test(char *);
void		mnt_mp_rename(char *, char *);
void		mnt_special_rename(char *, char *);
struct mnttab	*mnt_mnttab(char *);
void		mnt_free_mnttab(struct mnttab *);



u_int		hash_string(char *);

bool_t		dso_load(char *, char *, int);

bool_t		unsafe_check(struct vol *);

int		dev_to_part(struct vol *, int);

/*
 * functions to generate signatures from data.
 */
void		calc_md4(u_char *, size_t, u_longlong_t *);
u_long		calc_crc(u_char *, size_t);

char		*sh_to_regex(char *);
char 		**match_path(char *, int (*testpath)(char *));
char		*rawpath(char *);
int		makepath(char *, mode_t);

#ifdef	__cplusplus
}
#endif

#endif /* __UTIL_H */
