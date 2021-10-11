/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)libinst.h	1.24	99/10/07 SMI"

#ifndef	__PKG_LIBINST_H__
#define	__PKG_LIBINST_H__

#include <stdio.h>
#include <pkgstrct.h>
#include "install.h"

#define	DEF_NONE_SCR	"i.CompCpio"

#define	BL_ALL		-1	/* refers to all allocated lists */

/*
 * General purpose return codes used for functions which don't return a basic
 * success or failure. For those functions wherein a yes/no result is
 * possible, then 1 means OK and 0 means FAIL.
 */
#define	RESULT_OK	0x0
#define	RESULT_WRN	0x1
#define	RESULT_ERR	0x2

/* These are the file status indicators for the contents file */
#define	INST_RDY	'+'	/* entry is ready to installf -f */
#define	RM_RDY		'-'	/* entry is ready for removef -f */
#define	NOT_FND		'!'	/* entry (or part of entry) was not found */
#define	SERVED_FILE	'%'	/* using the file server's RO partition */
#define	STAT_NEXT	'@'	/* this is awaiting eptstat */
#define	DUP_ENTRY	'#'	/* there's a duplicate of this */
#define	CONFIRM_CONT	'*'	/* need to confirm contents */
#define	CONFIRM_ATTR	'~'	/* need to confirm attributes */
#define	ENTRY_OK	'\0'	/* entry is a confirmed file */

/* control bits for pkgdbmerg() */
#define	NO_COPY		0x0001
#define	CLIENT_PATHS	0x0002	/* working with a client database */

/* control bits for file verification by class */
#define	DEFAULT		0x0	/* standard full verification */
#define	NOVERIFY	0x1	/* do not verify */
#define	QKVERIFY	0x2	/* do a quick verification instead */

/* control bit for path type to pass to CAS */
#define	DEFAULT		0x0	/* standard server-relative absolute path */
#define	REL_2_CAS	0x1	/* pass pkgmap-type relative path */

/* findscripts() argument */
#define	I_ONLY		0x0	/* find install class action scripts */
#define	R_ONLY		0x1	/* find removal class action scripts */

struct cl_attr {
	char	name[CLSSIZ+1];	/* name of class */
	char	*inst_script;	/* install class action script */
	char	*rem_script;	/* remove class action script */
	unsigned	src_verify:3;	/* source verification level */
	unsigned 	dst_verify:4;	/* destination verification level */
	unsigned	relpath_2_CAS:1;	/* CAS gets relative paths */
};

#if defined(__STDC__)
#define	__P(protos) protos
#else	/* __STDC__ */
#define	__P(protos) ()
#endif	/* __STDC__ */

/* listmgr.c */
extern int	bl_create __P((int count_per_block, int struct_size,
		    char *desc));
extern char	*bl_next_avail __P((int list_handle));
extern char	*bl_get_record __P((int list_handle, int recno));
extern void	bl_free __P((int list_handle));
extern int	ar_create __P((int count_per_block, int struct_size,
		    char *desc));
extern char	**ar_next_avail __P((int list_handle));
extern char	**ar_get_head __P((int list_handle));
extern int	ar_delete __P((int list_handle, int index));

/* doulimit.c */
extern int	set_ulimit __P((char *script, char *err_msg));
extern int	clr_ulimit __P((void));
extern int	assign_ulimit __P((char *fslimit));

/* dryrun.c */
extern void	set_continue_not_ok __P((void));
extern int	continue_is_ok __P((void));
extern int	in_dryrun_mode __P((void));
extern int	in_continue_mode __P((void));
extern void	init_dryrunfile __P((char *dr_dir));
extern void	init_contfile __P((char *cn_dir));
extern void	set_dr_exitmsg __P((char *value));
extern void	set_dr_info __P((int type, int value));
extern void	write_dryrun_file __P((struct cfextra **extlist));
extern int	read_continuation __P((void));

/* lockinst.c */
extern int	lockinst __P((char *util_name, char *pkg_name));
extern void	lockupd __P((char *place));
extern void	unlockinst __P((void));

extern char	*pathdup __P((char *s));
extern char	*pathalloc __P((int n));
extern char	*fixpath __P((char *path));
extern char	*get_info_basedir __P((void));
extern char	*get_basedir __P((void));
extern char	*get_client_basedir __P((void));
extern int	set_basedirs __P((int reloc, char *adm_basedir,
		    char *pkginst, int nointeract));
extern int	eval_path __P((char **server_ptr, char **client_ptr,
		    char **map_ptr, char *path));
extern int	get_orig_offset __P((void));
extern char	*get_inst_root __P((void));
extern char	*get_mount_point __P((short n));
extern char	*get_remote_path __P((short n));
extern void	set_env_cbdir __P((void));
extern int	set_inst_root __P((char *path));
extern void	put_path_params __P((void));
extern int	mkpath __P((char *p));
extern void	mkbasedir __P((int flag, char *path));
extern int	is_an_inst_root __P((void));
extern int	is_a_basedir __P((void));
extern int	is_a_cl_basedir __P((void));
extern int	is_relocatable __P((void));
extern char	*orig_path __P((char *path));
extern char	*orig_path_ptr __P((char *path));
extern char	*qreason __P((int caller, int retcode, int started));
extern char	*qstrdup __P((char *s));
extern char	*srcpath __P((char *d, char *p, int part, int nparts));
extern int	copyf __P((char *from, char *to, long mytime));
extern int	dockdeps __P((char *depfile, int rflag));
extern int	finalck __P((struct cfent *ept, int attrchg, int contchg));

/* mntinfo.c */
extern int	get_mntinfo __P((int map_client, char *vfstab_file));
extern short	fsys __P((char *path));
extern struct fstable *get_fs_entry __P((short n));
extern int	mount_client __P((void));
extern int	unmount_client __P((void));
extern short	resolved_fsys __P((char *path));
extern char	*get_server_host __P((short n));
extern char	*server_map __P((char *path, short fsys_value));
extern int	use_srvr_map __P((char *path, short *fsys_value));
extern int	use_srvr_map_n __P((short n));
extern int	is_fs_writeable __P((char *path, short *fsys_value));
extern int	is_remote_fs __P((char *path, short *fsys_value));
extern int	is_served __P((char *path, short *fsys_value));
extern int	is_mounted __P((char *path, short *fsys_value));
extern int	is_fs_writeable_n __P((short n));
extern int	is_remote_fs_n __P((short n));
extern int	is_served_n __P((short n));
extern int	is_mounted_n __P((short n));
extern u_long	get_blk_size_n __P((short n));
extern u_long	get_frag_size_n __P((short n));
extern u_long	get_blk_used_n __P((short n));
extern void	set_blk_used_n __P((short n, ulong value));
extern u_long	get_blk_free_n __P((short n));
extern u_long	get_inode_used_n __P((short n));
extern u_long	get_inode_free_n __P((short n));
extern char	*get_source_name_n __P((short n));
extern char	*get_fs_name_n __P((short n));
extern int	load_fsentry __P((struct fstable *fs_entry, char *name,
		    char *fstype, char *remote_name));
extern int	isreloc __P((char *pkginstdir));
extern int	is_local_host __P((char *hostname));
extern void	fs_tab_free __P((void));

/* pkgdbmerg.c */
extern int	pkgdbmerg __P((FILE *mapfp, FILE *tmpfp,
		    struct cfextra **extlist, int notify));
extern int	files_installed __P((void));

/* ocfile.c */
extern int	ocfile __P((FILE **mapfp, FILE **tmpfp, ulong map_blks));
extern int	swapcfile __P((FILE *mapfp, FILE *tmpfp, char *pkginst));
extern int	set_cfdir __P((char *cfdir));
extern int	socfile __P((FILE **mapfp));
extern int	relslock __P((void));

extern long	nblk __P((long size, ulong bsize, ulong frsize));
extern struct	cfent **procmap __P((FILE *fp, int mapflag, char *ir));
extern void	repl_cfent __P((struct cfent *new, struct cfent *old));
extern struct	cfextra **pkgobjmap __P((FILE *fp, int mapflag, char *ir));
extern void	pkgobjinit __P((void));
extern int	seed_pkgobjmap __P((struct cfextra *ext_entry, char *path,
		    char *local));
extern int	init_pkgobjspace __P((void));

/* eptstat.c */
extern void	pinfo_free __P((void));
extern struct	pinfo *eptstat __P((struct cfent *entry, char *pkg, char c));

extern void	echo __P((char *fmt, ...));
extern void	notice __P((int n));

/* psvr4ck.c */
extern int	exception_pkg __P((char *pkginst, int pkg_list));
extern void	psvr4cnflct __P((void));
extern void	psvr4mail __P((char *list, char *msg, int retcode, char *pkg));
extern void	psvr4pkg __P((char **ppkg));

/* ptext.c */
extern void	ptext __P((FILE *fp, char *fmt, ...));

/* putparam.c */
extern void	putparam __P((char *param, char *value));
extern void	getuserlocale __P((void));
extern void	putuserlocale __P((void));

/* setadmin.c */
extern void	setadmin __P((char *file));

/* setlist.c */
extern char	*cl_iscript __P((int idx));
extern char	*cl_rscript __P((int idx));
extern void	find_CAS __P((int CAS_type, char *bin_ptr, char *inst_ptr));
extern int	setlist __P((struct cl_attr ***plist, char *slist));
extern void	addlist __P((struct cl_attr ***plist, char *item));
extern char	*cl_nam __P((int cl_idx));
extern char	*flex_device(char *device_name, int dev_ok);
extern int	cl_getn __P((void));
extern int	cl_idx __P((char *cl_nam));
extern void	cl_sets __P((char *slist));
extern void	cl_setl __P((struct cl_attr **cl_lst));
extern void	cl_putl __P((char *parm_name, struct cl_attr **list));
extern int	cl_deliscript __P((int i));
extern unsigned	cl_svfy __P((int i));
extern unsigned	cl_dvfy __P((int i));
extern unsigned	cl_pthrel __P((int i));
extern int nointeract;

#if defined(lint) && !defined(gettext)
#define	gettext(x)	x
#endif	/* defined(lint) && !defined(gettext) */

#endif	/* __PKG_LIBINST_H__ */
