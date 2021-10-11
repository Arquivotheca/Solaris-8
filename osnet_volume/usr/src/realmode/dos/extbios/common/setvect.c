/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */
 
#ident	"@(#)setvect.c	1.5	94/05/23 SMI\n"

void
setvector(unsigned short num, long addr)
{
long far *memloc;

	memloc = (long far *)(num*4);
	*memloc = addr;
}

