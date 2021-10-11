/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Declarations for the functions in libcmd.
 */

#ifndef	_LIBCMD_H
#define	_LIBCMD_H

#pragma ident	"@(#)libcmd.h 1.1	97/06/26 SMI"

#include <sum.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int getterm(char *, char *, char *, char *);

extern int mkmtab(char *, int);
extern int ckmtab(char *, int, int);
extern void prtmtab(void);

extern void sumpro(struct suminfo *sip);
extern void sumupd(struct suminfo *, char *, int);
extern void sumepi(struct suminfo *);
extern void sumout(FILE *, struct suminfo *);

extern int defcntl(int cmd, int newflags);

#ifdef	__cplusplus
}
#endif

#endif /* _LIBCMD_H */
