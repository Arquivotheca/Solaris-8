/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)localedef.h 1.1	98/11/19  SMI"

#ifndef _LOCALEDEF_H
#define	_LOCALEDEF_H

/*
 * Command line options to the C compiler.
 * Don't remove the white space in the beginning of each macro content.
 */
#define	CCFLAGS_COM		" -v -K PIC -DPIC -G -z defs -D_REENTRANT"
#define	CCFLAGS_SPARC	" -xO3 -xcg89 -Wa,-cg92 -xregs=no%appl"
#define	CCFLAGS_SPARCV9	" -xO3 -xarch=v9 -dalign -xregs=no%appl"
#define	CCFLAGS_I386	" -O"
#define	CCFLAGS_IA64	" -O"

/*
 * Architecture dependent command line options
 */
#if defined(__sparc)
#define	CCFLAGS		CCFLAGS_COM CCFLAGS_SPARC
#define	CCFLAGS64	CCFLAGS_COM CCFLAGS_SPARCV9
#define	ISA32	"sparc"
#define	ISA64	"sparcv9"
#else
#define	CCFLAGS		CCFLAGS_COM CCFLAGS_I386
#define	CCFLAGS64	CCFLAGS_COM CCFLAGS_I386
#define	ISA32	"i386"
#define	ISA64	"ia64"
#endif

/*
 * C compiler name
 */
#define	CCPATH  "cc"

/*
 * to specify libc for linking
 */
#define	LINKC	" -lc"

/*
 * localedef options
 */
#define	ARGSTR	"cc,"

/*
 * Command line to the C compiler
 * <tpath><CCPATH> <ccopts> -h <soname> -o <objname> <filename>
 */
#define	CCCMDLINE	"%s%s %s -h %s -o %s %s"

/*
 * Shared object name
 */
#define	SONAME			"%s.so.%d"
#define	SLASH_SONAME	"/" SONAME

#define	CCPATH_LEN		2
#define	SPC_LEN			1
#define	SONAMEF_LEN		2
#define	OBJNAMEF_LEN	2
#define	SLASH_LEN		1
#define	SOSFX_LEN		14	/* .so.?????????? */

#define	BIT32	0x01
#define	BIT64	0x02

#endif
