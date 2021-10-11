/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_DCONF_H
#define	_DCONF_H

#pragma ident	"@(#)dconf.h	1.1	98/05/01 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct dumpconf {
	char dc_device[MAXPATHLEN];	/* Dump device path */
	char dc_savdir[MAXPATHLEN];	/* Savecore dir path */
	int dc_cflags;			/* Config flags (see <sys/dumpadm.h>) */
	int dc_enable;			/* Run savecore on boot? (see below) */
	int dc_mode;			/* Mode flags (see below) */
	FILE *dc_conf_fp;		/* File pointer for config file */
	int dc_conf_fd;			/* File descriptor for config file */
	int dc_dump_fd;			/* File descriptor for dump device */
} dumpconf_t;

/*
 * Values for dc_enable (run savecore on boot) property:
 */
#define	DC_OFF		0		/* Savecore disabled */
#define	DC_ON		1		/* Savecore enabled */

/*
 * Values for dconf_open mode:
 */
#define	DC_CURRENT	1		/* Kernel overrides file settings */
#define	DC_OVERRIDE	2		/* File+defaults override kernel */

extern int dconf_open(dumpconf_t *, const char *, const char *, int);
extern int dconf_getdev(dumpconf_t *);
extern int dconf_close(dumpconf_t *);
extern int dconf_write(dumpconf_t *);
extern int dconf_update(dumpconf_t *);
extern void dconf_print(dumpconf_t *, FILE *);

extern int dconf_str2device(dumpconf_t *, char *);
extern int dconf_str2savdir(dumpconf_t *, char *);
extern int dconf_str2content(dumpconf_t *, char *);
extern int dconf_str2enable(dumpconf_t *, char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _DCONF_H */
