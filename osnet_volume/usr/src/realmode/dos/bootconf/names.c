/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  names.h - Eisa function name conversion:
 *
 *    This file contains code to convert 7-byte EISA device (board) names
 *    to and from their internal compressed form.
 *
 *    NOTE:  We avoid using str* functions so that these routines can be
 *	     included in the BEF library.
 */

#ident "@(#)names.c   1.3   97/08/27 SMI"

#include <names.h>

static const char hextab[] = "0123456789ABCDEF";

static int
hexdig(int c)
{
	/*
	 *  Get hex digit:
	 *
	 *  Returns the 4-bit hex digit named by the input character.  Returns
	 *  zero if the input character is not valid hex!
	 */

	int x = ((c < 'a') || (c > 'z')) ? c : (c - ' ');
	int j = sizeof (hextab);

	while (--j && (x != hextab[j]));
	return (j);
}

unsigned long
CompressName(char far *np)
{
	/*
	 *  Compress an EISA device name:
	 *
	 *  This routine converts a 7-byte ASCII device name into the 4-byte
	 *  compressed form used by EISA (50 bytes of ROM to save 1 byte of
	 *  NV-RAM!)
	 */

	union { char id[4]; unsigned long ret; } u;

	u.id[0] = ((np[0] & 0x1F) << 2) + ((np[1] >> 3) & 0x03);
	u.id[1] = ((np[1] & 0x07) << 5) + (np[2] & 0x1F);
	u.id[2] = (hexdig(np[3]) << 4) + hexdig(np[4]);
	u.id[3] = (hexdig(np[5]) << 4) + hexdig(np[6]);

	return (u.ret);
}

void
DecompressName(unsigned long id, char far *np)
{
	/*
	 *  Expand an EISA device name:
	 *
	 *  This is the inverse of the above routine.  It converts a 32-bit EISA
	 *  device "id" to a 7-byte ASCII device name, which is stored at "np".
	 */

	*np++ = '@' + ((id >> 2)  & 0x1F);
	*np++ = '@' + ((id << 3)  & 0x18) + ((id >> 13) & 0x07);
	*np++ = '@' + ((id >> 8)  & 0x1F);
	*np++ = hextab[(id >> 20) & 0x0F];
	*np++ = hextab[(id >> 16) & 0x0F];
	*np++ = hextab[(id >> 28) & 0x0F];
	*np++ = hextab[(id >> 24) & 0x0F];
	*np = 0;
}
