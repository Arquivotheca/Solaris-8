/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_BENV_H
#define	_BENV_H

#pragma ident	"@(#)benv.h	1.2	96/02/18 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

#define	NO_PERROR	0
#define	PERROR		1

typedef struct list {
	struct list *next;
	struct list *prev;
	void *item;
} list_t;

extern char *progname;
extern char *boottree;
extern struct utsname uts_buf;
extern int _error(int do_perror, char *fmt, ...);
extern char *strcats(char *s, ...);
extern list_t *new_list(void);
extern void add_item(void *item, list_t *list);

#ifdef	__cplusplus
}
#endif

#endif /* _BENV_H */
