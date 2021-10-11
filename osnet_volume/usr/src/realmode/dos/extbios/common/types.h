/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)types.h	1.8	95/10/11 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework Header
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 *    File name: types.h
 *
 * This file contains a minimal set of typedef's used throughout the MDB
 * code base.
 *
 */

typedef unsigned char unchar;
typedef unsigned short ushort;
typedef unsigned short uint;
typedef unsigned long ulong;
typedef long paddr_t;
typedef char far *caddr_f;

union halves {
	long l;
	ushort s[2];
   unchar c[4];
};
