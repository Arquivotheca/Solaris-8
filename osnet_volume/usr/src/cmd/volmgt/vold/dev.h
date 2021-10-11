/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef __DEV_H
#define	__DEV_H

#pragma ident	"@(#)dev.h	1.34	95/11/13 SMI"

#include <sys/types.h>
#include <rpc/types.h>
#include <synch.h>
#include <sys/vol.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Header file for device maniplulation.
 */

struct devsw {
				/* begin using a device */
	bool_t	(*d_use)(char *, char *);
				/* deal with an error on a device */
	bool_t	(*d_error)(struct ve_error *);
	int	(*d_getfd)(dev_t);	/* return an fd to the dev_t */
	void	(*d_poll)(dev_t);	/* launch the poll again */
				/* build devmap */
	void	(*d_devmap)(struct vol *, int, int);
	void	(*d_close)(char *);	/* stop using device */
				/* special eject support */
	void	(*d_eject)(struct devs *);
				/* find a missing volume */
	dev_t	(*d_find)(dev_t, struct vol  *);
				/* check to see if new media has arrived */
	int	(*d_check)(struct devs *);
	char	*d_mtype;	/* type of media this device handles */
	char	*d_dtype;	/* type of device */
	u_long	d_flags;	/* flags for volumes here */
	uid_t	d_uid;		/* uid for new inserts */
	gid_t	d_gid;		/* gid for new inserts */
	mode_t	d_mode;		/* mode for new inserts */
	bool_t	(*d_test)(char *); /* see if a path is okay for this device */
	long	d_pad[9];	/* room to grow */
	struct q d_pathl;	/* for reconfig stuff */
};

typedef struct dp_vol_lock {
	mutex_t		dp_vol_vg_mutex;/* for access to cv */
	cond_t		dp_vol_vg_cv;	/* for signalling "vol gone" */
} dp_vol_lock_t;

/*
 * d_flags
 */
#define	D_POLL		0x01	/* device uses d_poll entry point */
#define	D_RDONLY	0x02	/* read-only drive (like cdrom) */
#define	D_RMONEJECT	0x04	/* default, remove volume when ejected */
#define	D_MEJECTABLE	0x08	/* easily manually ejectable */


struct devs {
	struct q	q;		/* hash queue */
	struct devsw	*dp_dsw;	/* devsw that is for this dev */
	dev_t		dp_dev;		/* device this represents */
	char		*dp_path;	/* path to this device */
	void		*dp_priv;	/* driver private info */
	struct vvnode	*dp_rvn;	/* pointer to the char vn */
	struct vvnode	*dp_bvn;	/* pointer to the block vn */
	struct vol	*dp_vol;	/* vol_t that's in this device */
	bool_t		dp_writeprot;	/* dev is write protected */
	char		*dp_symname;	/* symbolic name for this dev */
	struct vvnode	*dp_symvn;	/* pointer to alias vn */
	int		dp_ndgrp;	/* number of devices in group */
	struct devs	**dp_dgrp;	/* pointers to dp's in group */
	bool_t		dp_checkresp;	/* respond to checker */
	dp_vol_lock_t	*dp_lock;	/* for signalling between threads */
	int		dp_pad[6];	/* room to grow */
};


/*
 * Mapping of volume dev_t to device dev_t.
 */
typedef struct devmap {
	dev_t	dm_voldev;	/* from (vol device name) */
	char	*dm_path;	/* to (path of device media is in */
	dev_t	dm_realdev;	/* cache of the dev_t */
} devmap_t;


/* dev prototypes */

void		dev_eject(struct vol *, bool_t);
void		dev_insert(dev_t);
void		dev_error(struct ve_error *);
bool_t		dev_use(char *, char *, char *, char *,
			char *, char *, char *, bool_t, bool_t);
void		dev_devmap(struct vol *);
bool_t		dev_devmapfree(struct vol *);
struct vvnode	*dev_newvol(char *, struct label *, time_t);
int		dev_type(char *);
char 		*dev_ident(int);
struct devs 	*dev_getdp(dev_t);
struct devs	*dev_makedp(struct devsw *, char *);
void		dev_freedp(struct devs *);
struct vvnode	*dev_dirpath(char *);
char		*dev_makepath(dev_t);
void		dev_unhangvol(struct devs *);
bool_t		dev_rdonly(dev_t);
bool_t		dev_map(struct vol *, bool_t);
struct vol 	*dev_unlabeled(struct devs *, enum laread_res,
		    struct label *);
char		*dev_symname(dev_t);

#define	DEV_SYM		"dev_init"

#ifdef	__cplusplus
}
#endif

#endif /* __DEV_H */
