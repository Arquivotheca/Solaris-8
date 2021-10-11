/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * main.h -- public definitions for main module
 */

#ifndef _MAIN_H
#define	_MAIN_H

#ident "@(#)main.h   1.9   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Global bootconf flags
 */
extern char *Script;	/* -p<script> (keystroke script file) */
extern int Floppy;	/* -f we are running of the floppy */
extern int Autoboot;

void init_main(int argc, char *argv[]);

#ifdef	__cplusplus
}
#endif

#endif /* _MAIN_H */
