/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * dir.h -- public definitions for dir routines
 */

#ifndef	_DIR_H
#define	_DIR_H

#ident "@(#)dir.h   1.6   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <dos.h>
#include <stdlib.h>
#define	PATH_MAX _MAX_PATH	/* DOS's maximum path length */

struct dirent {
    int   d_off;		/* Entry offset number */
    char *d_name;		/* Directory entry name */
};

typedef struct {
    struct dirent de;		/* Current dir entry appears here */
    struct _find_t f;		/* state for _dos_findnext() */
    char   dir[PATH_MAX];	/* Name of directory to search */
} DIR;

DIR *opendir(const char *);
struct dirent *readdir(DIR *);
void closedir(DIR *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DIR_H */
