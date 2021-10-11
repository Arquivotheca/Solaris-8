/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)iutil.c	1.7	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.
 *
 * This file contains C language utility routines used during MDB device
 * initialization
 *
 */

#ifdef DEBUG
    #pragma ( message, __FILE__ "built in debug mode" )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "iutil.c	1.7	94/05/23" )

#define BIOS_MEM_SIZE 0x400013


/* Reserves the requested number of kilobytes of memory.
 * Returns the selector for addressing the memory.
 */
unsigned short
reserve ( short kilobytes )
{
     int _far *top_mem = (int _far *)BIOS_MEM_SIZE;

     /* ### Need to add a test for not enough memory */
     *top_mem -= kilobytes;
     return ((*top_mem) << 6);
}


/* Returns the useable memory size in kilobytes */
int
memsize()
{
     int _far *top_mem = (int _far *)BIOS_MEM_SIZE;

     return (*top_mem);
}


void
getvec ( unsigned short vector, long *address )
{
     long _far *vec;

     vec = (long _far *)(vector << 2);
     *address = *vec;
}


void
setvec ( unsigned short vector, long newval )
{
     long _far *vec;

     vec = (long _far *)(vector << 2);
     *vec = newval;
}

