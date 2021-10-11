/*
 * Copyright (c) 1994-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PLATNAMES_H
#define	_SYS_PLATNAMES_H

#pragma ident	"@(#)platnames.h	1.7	99/10/22 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * External interfaces
 */
extern char *get_mfg_name(void);
extern int find_platform_dir(int (*)(char *), char *, int);
extern int open_platform_file(char *,
    int (*)(char *, void *), void *, char *, char *);
extern void mod_path_uname_m(char *, char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PLATNAMES_H */
