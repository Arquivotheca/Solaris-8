/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_CONF_H
#define	_CONF_H

#pragma ident	"@(#)conf.h	1.1	97/12/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
	char **cf_dtab;		/* Array of pointers to line data */
	int cf_dsize;		/* Number of allocated lines in dtab */
	int cf_lines;		/* Number of valid lines in dtab */
	int cf_ptr;		/* Current dtab location for read/rewind */
} conf_t;

int conf_open(conf_t *, const char *, char *[]);
void conf_close(conf_t *);
void conf_rewind(conf_t *);
char *conf_read(conf_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _CONF_H */
