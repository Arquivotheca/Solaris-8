/*
 * Copyright (c) 1993, 1994 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PKGLIB_H
#define	_PKGLIB_H

#pragma ident	"@(#)pkglib.h	1.12	99/08/30 SMI"

#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <pkgdev.h>
#include <pkgstrct.h>

#ifdef	__cplusplus
extern "C" {
#endif

struct dm_buf {
	char *text_buffer;	/* start of allocated buffer */
	char *text_insert;	/* insertion point for next string */
	int offset;		/* number of bytes into the text_buffer */
	int allocation;		/* size of buffer in bytes */
};

/* setmapmode() defines */
#define	MAPALL		0	/* resolve all variables */
#define	MAPBUILD	1	/* map only build variables */
#define MAPINSTALL	2	/* map only install variables */
#define MAPNONE		3	/* map no variables */

/*
 * These three defines indicate that the prototype file contains a '?'
 * meaning do not specify this data in the pkgmap entry.
 */
#define	CURMODE		BADMODE		/* current mode has been specified */
#define	CUROWNER	BADOWNER	/* ... same for owner ... */
#define CURGROUP	BADGROUP	/* ... and group. */

/*
 * The next three mean that no mode, owner or group was specified or that the
 * one specified is invalid for some reason. Sometimes this is an error in
 * which case it is generally converted to CUR* with a warning. Other times
 * it means "look it up" by stating the existing file system object pointred
 * to in the prototype file.
 */
#define	NOMODE		(BADMODE-1)
#define NOOWNER		"@"
#define	NOGROUP		"@"

#ifdef	__STDC__

extern FILE	*epopen(char *cmd, char *mode);
extern char	**gpkglist(char *dir, char **pkg);

extern void	pkglist_cont(char *keyword);
extern char	**pkgalias(char *pkg);
extern char	*get_prog_name(void);
extern char 	*set_prog_name(char *name);
extern int	averify(int fix, char *ftype, char *path, struct ainfo *ainfo);
extern int	ckparam(char *param, char *value);
extern int	ckvolseq(char *dir, int part, int nparts);
extern int	cverify(int fix, char *ftype, char *path, struct cinfo *cinfo);
extern int	devtype(char *alias, struct pkgdev *devp);
extern int	ds_close(int pkgendflg);
extern int	ds_findpkg(char *device, char *pkg);
extern int	ds_getinfo(char *string);
extern int	ds_getpkg(char *device, int n, char *dstdir);
extern int	ds_ginit(char *device);
extern int	ds_init(char *device, char **pkg, char *norewind);
extern int	ds_next(char *device, char *instdir);
extern int	ds_readbuf(char *device);
extern int	epclose(FILE *pp);
extern int	esystem(char *cmd, int ifd, int ofd);
extern int	gpkgmap(struct cfent *ept, FILE *fp);
extern void	setmapmode(int mode_no);
extern int	iscpio(char *path, int *iscomp);
extern int	isdir(char *path);
extern int	isfile(char *dir, char *file);
extern int	pkgexecl(char *filein, char *fileout, char *uname, char *gname,
		    ...);
extern int	pkgexecv(char *filein, char *fileout, char *uname, char *gname,
		    char *arg[]);
extern int	pkghead(char *device);
extern int	pkgmount(struct pkgdev *devp, char *pkg, int part, int nparts,
		    int getvolflg);
extern int	pkgtrans(char *device1, char *device2, char **pkg,
		    int options);
extern int	pkgumount(struct pkgdev *devp);
extern int	ppkgmap(struct cfent *ept, FILE *fp);
extern int	putcfile(struct cfent *ept, FILE *fp);
extern int	rrmdir(char *path);
extern int	srchcfile(struct cfent *ept, char *path, FILE *fpin,
		    FILE *fpout);
extern struct	group *cgrgid(gid_t gid);
extern struct	group *cgrnam(char *nam);
extern struct	passwd *cpwnam(char *nam);
extern struct	passwd *cpwuid(uid_t uid);
extern void	basepath(char *path, char *basedir, char *ir);
extern void	canonize(char *file);
extern void	checksum_off(void);
extern void	checksum_on(void);
extern void	cvtpath(char *path, char *copy);
extern void	ds_order(char *list[]);
extern void	ds_putinfo(char *buf);
extern void	ds_skiptoend(char *device);
extern void	ecleanup(void);
/*PRINTFLIKE1*/
extern void	logerr(char *fmt, ...);
extern int	mappath(int flag, char *path);
extern int	mapvar(int flag, char *varname);
/*PRINTFLIKE1*/
extern void	progerr(char *fmt, ...);
extern void	rpterr(void);
extern void	tputcfent(struct cfent *ept, FILE *fp);
extern void set_nonABI_symlinks(void);
extern int nonABI_symlinks(void);

#else	/* __STDC__ */

extern FILE	*epopen();
extern void	pkglist_cont();
extern char	**gpkglist();
extern char	**pkgalias();
extern char	*get_prog_name();
extern char 	*set_prog_name();
extern int	averify();
extern int	ckparam();
extern int	ckvolseq();
extern int	cverify();
extern int	devtype();
extern int	ds_close();
extern int	ds_findpkg();
extern int	ds_getinfo();
extern int	ds_getpkg();
extern int	ds_ginit();
extern int	ds_init();
extern int	ds_next();
extern int	ds_readbuf();
extern int	epclose();
extern int	esystem();
extern int	gpkgmap();
extern int	iscpio();
extern int	isdir();
extern int	isfile();
extern int	pkgexecl();
extern int	pkgexecv();
extern int	pkghead();
extern int	pkgmount();
extern int	pkgtrans();
extern int	pkgumount();
extern int	ppkgmap();
extern int	putcfile();
extern int	rrmdir();
extern int	srchcfile();
extern struct	group *cgrgid();
extern struct	group *cgrnam();
extern struct	passwd *cpwnam();
extern struct	passwd *cpwuid();
extern void	basepath();
extern void	canonize();
extern void	checksum_off();
extern void	checksum_on();
extern void	cvtpath();
extern void	ds_order();
extern void	ds_putinfo();
extern void	ds_skiptoend();
extern void	ecleanup();
extern void	logerr();
extern int	mappath();
extern int	mapvar();
extern void	progerr();
extern void	rpterr();
extern void	tputcfent();
extern void set_nonABI_symlinks();
extern int nonABI_symlinks();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _PKGLIB_H */
