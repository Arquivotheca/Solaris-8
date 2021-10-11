/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _IO_H
#define	_IO_H

#pragma ident	"@(#)io.h	1.1	99/01/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern FILE *Bodyfp;
extern FILE *Headfp;
extern FILE *Mapfp;

extern void explain_fopen_failure(int, char *);
extern void explain_fclose_failure(int, char *);

extern int open_code_file(void);
extern int commit_code_file(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _IO_H */
