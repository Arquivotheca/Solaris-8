/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_SUBR_H
#define	_SUBR_H

#pragma ident	"@(#)subr.h   1.14     98/06/16 SMI"

/*
 *
 *			subr.h
 *
 * Function prototypes for subr.c
 */

#include <ftw.h>

/* size to make a buffer for holding a pathname */
#define	CACHEFS_XMAXPATH (PATH_MAX + MAXNAMELEN + 2)

#ifdef __cplusplus
extern "C" {
#endif

/* resource file info */
struct cachefs_rinfo {
	int	r_fsize;	/* total file size */
	int	r_ptroffset;	/* offset to pointers area */
	int	r_ptrsize;	/* size of pointers area */
	int	r_identoffset;	/* offset to idents area */
	int	r_identsize;	/* size of idents area */
};

struct cachefs_user_values {
	int uv_maxblocks;
	int uv_minblocks;
	int uv_threshblocks;
	int uv_maxfiles;
	int uv_minfiles;
	int uv_threshfiles;
	int uv_maxfilesize;
	int uv_hiblocks;
	int uv_lowblocks;
	int uv_hifiles;
	int uv_lowfiles;
};

int cachefs_dir_lock(const char *cachedirp, int shared);
int cachefs_dir_unlock(int fd);
int cachefs_label_file_get(const char *filep, struct cache_label *clabelp);
int cachefs_label_file_put(const char *filep, struct cache_label *clabelp);
int cachefs_inuse(const char *cachedirp);
int cachefs_label_file_vcheck(char *filep, struct cache_label *clabelp);
void cachefs_resource_size(int maxinodes, struct cachefs_rinfo *rinfop);
int cachefs_create_cache(char *dirp, struct cachefs_user_values *,
    struct cache_label *);
int cachefs_delete_all_cache(char *dirp);
int cachefs_delete_cache(char *dirp, char *namep);
int cachefs_delete_file(const char *namep, const struct stat64 *statp, int flg,
    struct FTW *ftwp);
int cachefs_convert_uv2cl(const struct cachefs_user_values *uvp,
    struct cache_label *clp, const char *dirp);
int cachefs_convert_cl2uv(const struct cache_label *clp,
    struct cachefs_user_values *uvp, const char *dirp);
char *cachefs_file_to_dir(const char *);
int cachefs_clean_flag_test(const char *cachedirp);
void pr_err(char *fmt, ...);
time32_t get_boottime(void);

#ifdef __cplusplus
}
#endif

#endif /* _SUBR_H */
