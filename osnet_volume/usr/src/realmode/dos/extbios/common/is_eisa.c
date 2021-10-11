/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)is_eisa.c	1.7	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - BIOS Extension Driver Framework
 *===========================================================================
 * Provides minimal INT 13h services for MDB devices during Solaris
 * primary boot sequence.                                          
 *
 *   Function name:	is_eisa  (is_eisa.c)
 *
 *   Calling Syntax:	rc = is_eisa ()
 *
 *   Description:	no input argument; returns 1 if system bus is EISA,
 *                0 otherwise.
 *
 */

#ifdef DEBUG
    #pragma ( message, __FILE__ "built in debug mode" )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "is_eisa.c	1.7	94/05/23" )

long _far *EISA_loc = (long _far *)0xFFF000D9;
long EISA_id = 0x41534945;


is_eisa ()
{
    if ( *EISA_loc == EISA_id ) 
        return ( 1 );
    else
        return ( 0 );
}

