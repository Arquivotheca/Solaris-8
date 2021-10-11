/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#ifndef __OBJ_H
#define	__OBJ_H

#pragma ident	"@(#)obj.h	1.16	94/08/29 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/vtoc.h>

/*
 * Object types
 */
#ifdef notdef
#define	OBJ_DIR		1	/* directory */
#define	OBJ_VOL		2	/* volume */
#define	OBJ_LINK	3	/* hard link */
#define	OBJ_SYMLINK	4	/* symbolic link */
#endif

/*
 * These are the object independent attributes, and control
 * structures.
 */
typedef struct obj {
	struct q	q;		/* for future use */
	struct dbops	*o_dbops;	/* database this object is in */
	char		*o_name;	/* name of the object */
	char		*o_dir;		/* directory it lives in */
	u_longlong_t	o_xid;		/* version we have a copy of */
	u_int		o_type;		/* type of object */
	u_longlong_t	o_id;		/* unique id for the object */
	uid_t		o_uid;		/* user id of the owner */
	gid_t		o_gid;		/* group id */
	mode_t		o_mode;		/* unix permissions */
	u_int		o_nlinks;	/* hard link count */
	struct timeval  o_atime;	/* access time */
	struct timeval  o_ctime;	/* creation time */
	struct timeval  o_mtime;	/* modified time */
	u_longlong_t	o_upmask;	/* bitmask of changed fields */
	char		*o_props;	/* property string for object */
	int		o_pad[10];	/* room to grow */
} obj_t;

#define	OBJ_UP_NAME	0x001
#define	OBJ_UP_DIR	0x002
#define	OBJ_UP_UID	0x004
#define	OBJ_UP_GID	0x008
#define	OBJ_UP_MODE	0x010
#define	OBJ_UP_ATIME	0x020
#define	OBJ_UP_CTIME	0x040
#define	OBJ_UP_MTIME	0x080
#define	OBJ_UP_LABEL	0x100
#define	OBJ_UP_LOC	0x200
#define	OBJ_UP_FLAGS	0x400
#define	OBJ_UP_NLINKS	0x800

/*
 * The volume object.
 */
typedef struct vol {
	obj_t		v_obj;		/* object stuff */
	char		*v_mtype;	/* volume type (cdrom, floppy, ..) */
	label 		v_label;	/* volume label */
	u_long		v_parts;	/* per-vol partitions (bitmap) */
	bool_t		v_confirmed;	/* it is really there */
	devmap_t	*v_devmap;	/* map of devices (from v_basedev) */
	u_char		v_ndev;		/* number of devmaps */
	dev_t		v_basedev;	/* base device of location */
	char		*v_location;	/* location string */
	struct clue {
		minor_t		c_volume;	/* volume event happened on */
		uid_t		c_uid;		/* uid causing trouble */
		dev_t		c_tty;		/* his controlling tty */
		struct ve_error	*c_error;	/* error info */
	} v_clue; /* Hint for the various user friendy action features */
	u_long		v_eject;	/* count of outstanding eject acts */
	bool_t		v_ejfail;	/* failed the ejection */
	bool_t		v_ej_inprog;	/* ejection in progress */
	bool_t		v_ej_force;	/* already gone! */
	bool_t		v_checkresp;	/* respond to check request */
	u_long		v_flags;	/* per-vol flags (bitmap) */
	long		v_pad[9];	/* room to grow */
} vol_t;

/*
 * fields in v_flags
 */
#define	V_TAPE		0x1	/* volume is a tape (not a disk) */
#define	V_NETWIDE	0x2	/* available all over the network */
#define	V_FREE1		0x4	/* unused flag */
#define	V_UNLAB		0x8	/* volume is unlabeled */
#define	V_RDONLY	0x10	/* read-only media */
#define	V_WORM		0x20	/* write once/read many */
#define	V_NEWLABEL	0x40	/* new label has been written */
#define	V_RMONEJECT	0x80	/* remove on eject */
#define	V_MEJECTABLE	0x100	/* can be easily manually ejected */
#define	V_MISSING	0x200	/* missing event has been seen */
#define V_UNMAPPED	0x400	/* unmapping worked, so can free the unit */

/* disk partition information */
#ifdef	INTEL_PORT_AOK
#define	V_MAXPART	V_NUMPAR	/* from vtoc.h */
#else
/* XXX: this is partly because of bug id# 1153845 (wld) */
#define	V_MAXPART	8	/* temp hack to limit partitions to 8 */
#endif

#ifdef	TAPES_SUPPORTED
/* tape behavior */
#define	V_REWIND	0x100	/* rewind on close */
#define	V_SVR4		0x200	/* svr4 mode (blech) */
#define	V_DENSMSK	0xc00	/* mask for density */
#endif

/* tape and floppy densities */
#define	V_DENS_L	0x000	/* low density */
#define	V_DENS_M	0x400	/* medium density */
#define	V_DENS_H	0x800	/* high density */
#define	V_DENS_U	0xc00	/* ultra density */

#define	V_ENXIO		0x10000	/* return enxio till last close */
#ifdef	notdef
#define	V_FREE3		0x20000	/* free from here on */
#endif


/*
 * The directory object.
 */
typedef struct dirat {
	obj_t		da_obj;
	int		da_pad[10];
} dirat_t;

/*
 * For the links, we keep the pointers as paths, and only resolve
 * them to a vvnode when we actually do the lookup on them.
 */

/*
 * The symbolic link object.
 */
typedef struct symat {
	obj_t		sla_obj;
	char		*sla_ptr;	/* who it points at */
	int		sla_pad[10];
} symat_t;

/*
 * The hard link object.
 */
typedef struct linkat {
	obj_t		la_obj;
	u_longlong_t	la_id;		/* id of the object we point to */
	int		la_pad[10];
} linkat_t;

/*
 * The partition object.
 */
typedef struct partat {
	obj_t		pa_obj;
	int		pa_pad[10];
} partat_t;

/*
 * change the attribute of a database object.
 */
void change_name(obj_t *obj, char *name);
void change_dir(obj_t *obj, char *dir);
void change_uid(obj_t *obj, uid_t uid);
void change_gid(obj_t *obj, gid_t gid);
void change_mode(obj_t *obj, mode_t mode);
void change_atime(obj_t *obj, struct timeval *tv);
void change_mtime(obj_t *obj, struct timeval *tv);
void change_ctime(obj_t *obj, struct timeval *tv);

/*
 * these only apply to volumes
 */
void change_location(obj_t *obj, char *path);
void change_flags(obj_t *obj);
void change_label(obj_t *obj, label *la);

obj_t	*obj_dup(obj_t *);
char 	*obj_basepath(obj_t *);

#ifdef	__cplusplus
}
#endif

#endif /* __OBJ_H */
