/*
 *
 *			cfsd_logelem.h
 *
 * Include file for the logelem class.
 *
 */
#ident   "@(#)cfsd_logelem.h 1.3     97/12/06 SMI"
/* Copyright (c) 1994-1997 by Sun Microsystems, Inc. */

#ifndef CFSD_LOGELEM
#define	CFSD_LOGELEM

/* typedefs for logelem type */
#define	NO_OBJECT_TYPE		0
#define	SETATTR_OBJECT_TYPE	1
#define	SETSECATTR_OBJECT_TYPE	2
#define	CREATE_OBJECT_TYPE	3
#define	REMOVE_OBJECT_TYPE	4
#define	RMDIR_OBJECT_TYPE	5
#define	MKDIR_OBJECT_TYPE	6
#define	LINK_OBJECT_TYPE	7
#define	SYMLINK_OBJECT_TYPE	8
#define	RENAME_OBJECT_TYPE	9
#define	MODIFIED_OBJECT_TYPE	10
#define	MAPFID_OBJECT_TYPE	11

#define	CFSDMesgMax		4096
/* BEGIN CSTYLED */
/* defines for refrencing objects */
#define	SETATTR_OBJECT(ptr)		ptr->i_operation.i_setattr_object
#define	SETATTR_OBJECT_PTR(ptr)		&(ptr->i_operation.i_setattr_object)
#define	SETSECATTR_OBJECT(ptr)		ptr->i_operation.i_setsecattr_object
#define	SETSECATTR_OBJECT_PTR(ptr)	&(ptr->i_operation.i_setsecattr_object)
#define	CREATE_OBJECT(ptr)		ptr->i_operation.i_create_object
#define	CREATE_OBJECT_PTR(ptr)		&(ptr->i_operation.i_create_object)
#define	REMOVE_OBJECT(ptr)		ptr->i_operation.i_remove_object
#define	REMOVE_OBJECT_PTR(ptr)		&(ptr->i_operation.i_remove_object)
#define	RMDIR_OBJECT(ptr)		ptr->i_operation.i_rmdir_object
#define	RMDIR_OBJECT_PTR(ptr)		&(ptr->i_operation.i_rmdir_object)
#define	MKDIR_OBJECT(ptr)		ptr->i_operation.i_mkdir_object
#define	MKDIR_OBJECT_PTR(ptr)		&(ptr->i_operation.i_mkdir_object)
#define	LINK_OBJECT(ptr)		ptr->i_operation.i_link_object
#define	LINK_OBJECT_PTR(ptr)		&(ptr->i_operation.i_link_object)
#define	SYMLINK_OBJECT(ptr)		ptr->i_operation.i_symlink_object
#define	SYMLINK_OBJECT_PTR(ptr)		&(ptr->i_operation.i_symlink_object)
#define	RENAME_OBJECT(ptr)		ptr->i_operation.i_rename_object
#define	RENAME_OBJECT_PTR(ptr)		&(ptr->i_operation.i_rename_object)
#define	MODIFIED_OBJECT(ptr)		ptr->i_operation.i_modified_object
#define	MODIFIED_OBJECT_PTR(ptr)	&(ptr->i_operation.i_modified_object)
#define	MAPFID_OBJECT(ptr)		ptr->i_operation.i_mapfid_object
#define	MAPFID_OBJECT_PTR(ptr)		&(ptr->i_operation.i_mapfid_object)
/* END CSTYLED */

/* setattr */
typedef struct cfsd_logelem_setattr_object {
	struct cfs_dlog_setattr	*i_up;
} cfsd_logelem_setattr_object_t;

/* setsecattr */
typedef struct cfsd_logelem_setsecattr_oject {
	struct cfs_dlog_setsecattr	*i_up;
	const aclent_t			*i_acl;
} cfsd_logelem_setsecattr_object_t;

/* create */
typedef struct cfsd_logelem_create_object {
	struct cfs_dlog_create	*i_up;
	const char		*i_namep;	/* name of file to create */
} cfsd_logelem_create_object_t;

/* remove */
typedef struct cfsd_logelem_remove_object {
	struct cfs_dlog_remove	*i_up;
	const char		*i_namep;	/* name of file to remove */
} cfsd_logelem_remove_object_t;

/* rmdir */
typedef struct cfsd_logelem_rmdir_object {
	struct cfs_dlog_rmdir	*i_up;
	const char		*i_namep;	/* name of dir to rmdir */
} cfsd_logelem_rmdir_object_t;

/* mkdir */
typedef struct cfsd_logelem_mkdir_object {
	struct cfs_dlog_mkdir	*i_up;
	const char		*i_namep;	/* name of dir to mkdir */
} cfsd_logelem_mkdir_object_t;

/* link */
typedef struct cfsd_logelem_link_object {
	struct cfs_dlog_link	*i_up;
	const char		*i_namep;	/* name of link */
} cfsd_logelem_link_object_t;

/* symlink */
typedef struct cfsd_logelem_symlink_object {
	struct cfs_dlog_symlink	*i_up;
	const char		*i_namep;	/* name of symlink */
	const char		*i_contentsp;	/* contents of symlink */
} cfsd_logelem_symlink_object_t;

/* rename */
typedef struct cfsd_logelem_rename_object {
	struct cfs_dlog_rename	*i_up;
	const char		*i_orignamep;	/* original name */
	const char		*i_newnamep;	/* new name */
} cfsd_logelem_rename_object_t;

/* modify */
typedef struct cfsd_logelem_modified_object {
	struct cfs_dlog_modify	*i_up;
} cfsd_logelem_modified_object_t;

/* mapfid */
typedef struct cfsd_logelem_mapfid_object {
	struct cfs_dlog_mapfid	*i_up;
} cfsd_logelem_mapfid_object_t;

/* Abstract base class used by the other logelem classes. */
typedef struct cfsd_logelem_object {

	cfsd_maptbl_object_t	*i_maptbl_object_p;
	cfsd_logfile_object_t	*i_logfile_object_p;
	cfsd_kmod_object_t	*i_kmod_object_p;
	char			 i_messagep[CFSDMesgMax];	/* message */
	char			 i_fidbuf[1024];		/* debugging */
	cfs_dlog_entry_t	*i_entp;
	off_t			 i_offset;
	int			 i_type;
	union {
		cfsd_logelem_setattr_object_t	i_setattr_object;
		cfsd_logelem_setsecattr_object_t	i_setsecattr_object;
		cfsd_logelem_create_object_t	i_create_object;
		cfsd_logelem_remove_object_t	i_remove_object;
		cfsd_logelem_rmdir_object_t	i_rmdir_object;
		cfsd_logelem_mkdir_object_t	i_mkdir_object;
		cfsd_logelem_link_object_t	i_link_object;
		cfsd_logelem_symlink_object_t	i_symlink_object;
		cfsd_logelem_rename_object_t	i_rename_object;
		cfsd_logelem_modified_object_t	i_modified_object;
		cfsd_logelem_mapfid_object_t	i_mapfid_object;
	}i_operation;
}cfsd_logelem_object_t;

cfsd_logelem_object_t *cfsd_logelem_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);
void cfsd_logelem_destroy(cfsd_logelem_object_t *logelem_object_p);
void logelem_print_cred(cred_t *credp);
void logelem_print_attr(vattr_t *vp);
void logelem_format_fid(cfsd_logelem_object_t *logelem_object_p, fid_t *fidp);
int logelem_lostfound(cfsd_logelem_object_t *logelem_object_p,
    cfs_cid_t *cidp,
    cfs_cid_t *pcidp,
    const char *namep,
    cred_t *cred);
void logelem_problem(cfsd_logelem_object_t *logelem_object_p,
	char *strp);
void logelem_resolution(cfsd_logelem_object_t *logelem_object_p,
	char *strp);
void logelem_message_append(char *strp1, char *strp2);
void logelem_message(cfsd_logelem_object_t *logelem_object_p,
	char *prefix, char *strp);
void logelem_log_opfailed(cfsd_logelem_object_t *logelem_object_p,
	char *opp, char *info, const char *namep, int xx);
void logelem_log_opskipped(cfsd_logelem_object_t *logelem_object_p,
	const char *namep);
void logelem_log_timelogmesg(cfsd_logelem_object_t *logelem_object_p,
	char *opp, const char *namep, char *mesgp, int time_log);


cfsd_logelem_object_t *cfsd_logelem_setattr_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_setsecattr_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_create_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_remove_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_rmdir_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_mkdir_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_link_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_symlink_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_rename_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_modified_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

cfsd_logelem_object_t *cfsd_logelem_mapfid_create(
    cfsd_maptbl_object_t *maptbl_object_p,
    cfsd_logfile_object_t *logfile_object_p,
    cfsd_kmod_object_t *kmod_object_p);

int logelem_roll(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);

int logelem_roll_setattr(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_setsecattr(cfsd_logelem_object_t *logelem_object_p,
    u_long *seqp);
int logelem_roll_create(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_remove(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_rmdir(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_mkdir(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_link(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_symlink(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_rename(cfsd_logelem_object_t *logelem_object_p, u_long *seqp);
int logelem_roll_modified(cfsd_logelem_object_t *logelem_object_p,
    u_long *seqp);
int logelem_roll_mapfid(cfsd_logelem_object_t *logelem_object_p);

void logelem_dump(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_setattr(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_setsecattr(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_create(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_remove(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_rmdir(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_mkdir(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_link(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_symlink(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_rename(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_modified(cfsd_logelem_object_t *logelem_object_p);
void logelem_dump_mapfid(cfsd_logelem_object_t *logelem_object_p);
#endif /* CFSD_LOGELEM */
