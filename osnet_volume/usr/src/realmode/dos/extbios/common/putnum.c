/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"<@(#)putnum.c	1.8	95/10/27 SMI>"

/*
 *  Some old output routines, re-written to use the new realmode printf!
 */


#ifdef DEBUG
    #pragma message ( __FILE__ ": << WARNING! DEBUG MODE >>" )
    #pragma comment ( user, __FILE__ ": DEBUG ON " __TIMESTAMP__ )
#endif

#pragma comment ( compiler )
#pragma comment ( user, "putnum.c	1.8	95/10/27" )

#include <stdio.h>

void
putnum(unsigned short num)
{
	printf("%d", num);
}

void
puthex(unsigned short num)
{
	printf("%x", num);
}

void
put1hex(unsigned short num)
{
	printf("%x", (num & 0xF));
}

void
put2hex(unsigned short num)
{
	printf("%x", (num & 0xFF));
}

void
putstr(char *p)
{
	printf("%s", p);
}
