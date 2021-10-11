/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)ctypefns.c	1.5	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Routine
 *===========================================================================
 * Provides minimal services for the real-mode environment that the operating 
 * system would normally supply.
 *
 *   Function name:	miscellaneous character test functions  (ctypefns.c)
 *
 *   Calling Syntax:	typically, a single input argument as in
 *			"isdigit ( testchar )"; see ctype.h.
 *
 *   Description:	Character-type functions ( isdigit and friends ).
 *			Interrogate character type, return 1 if true, 0
 *			otherwise.
 *
 */

#include <bioserv.h>
#include <ctype.h>


short
isalpha ( short c )
{
        return((_ctype + 1)[c] & (_U | _L));
}

short
isupper ( short c )
{
        return((_ctype + 1)[c] & _U);
}

short
islower ( short c )
{
        return((_ctype + 1)[c] & _L);
}

short
isdigit ( short c )
{
        return((_ctype + 1)[c] & _N);
}

short
isxdigit ( short c )
{
        return((_ctype + 1)[c] & _X);
}

short
isalnum ( short c )
{
        return((_ctype + 1)[c] & (_U | _L | _N));
}

short
isspace ( short c )
{
        return((_ctype + 1)[c] & _S);
}

short
ispunct ( short c )
{
        return((_ctype + 1)[c] & _P);
}

short
isprint ( short c )
{
        return((_ctype + 1)[c] & (_P | _U | _L | _N | _B));
}

short
isgraph ( short c )
{
        return((_ctype + 1)[c] & (_P | _U | _L | _N));
}

short
iscntrl ( short c )
{
        return((_ctype + 1)[c] & _C);
}

short
isascii ( short c )
{
        return(!(c & ~0177));
}

short
_toupper ( short c )
{
        return((_ctype + 258)[c]);
}

short
_tolower ( short c )
{
        return((_ctype + 258)[c]);
}

short
toascii ( short c )
{
        return ((c) & 0177);
}
