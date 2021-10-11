/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Single-byte output routines for use with realmode drivers:
 *
 *    This file contains the various flavors of "putc" available to Solaris
 *    realmode device drivers.  These consist of:
 *
 *         putchar()      ... Calls indirectly thru the 'putvec" 
 *	   bios_putchar() ... Which converts putchar's to int10's
 *
 *    The "putvec" initially points to "bios_putchar", but is reset to point
 *    to thru the BEF callback vector if and when the driver is invoked at
 *    one of the BEF extention entry points.
 *
 *    Since we only support a single file descriptor (the console), fputc
 *    is just a macro call to putchar().
 */

#ident "@(#)putc.c	1.9	96/08/28 SMI\n"
#include <stdio.h>

FILE _iob = {0};	/* Dummy IOB to resolve stdout references	    */

int
_bios_putchar(int c)
{
	/*
	 *  Local putchar routine:
	 *
	 *  This version of putchar is used when drivers are running in MDB
	 *  emulation mode (i.e, when they're not called thru on the BEF
	 *  extension entry points).
	 */

	char row, col;
	int n = 1, rc = c;
	static int page = 100;

	if (page > 7) _asm {
		/*
		 *  If we don't know the current page number yet, read it now
		 *  and save it for later.
		 */

		push  bx;		  Save register
		mov   ah, 0Fh
		int   10h;		  "Read video mode" gives screen number
		mov   byte ptr [page], bh; .. in %bh
		pop   bx
	}

	switch (c) {
		/*
		 *  Special preprocessing required by some output characters:
		 */

		case '\t': case '\b': {
		 	/*
			 *  If next output character is a tab or a backspace,
			 *  we'll need need know the current cursor position.
			 *  Read it here.
			 */

			_asm {
				/*
				 *  Issue BIOS call to read the cursor position.
				 *  This comes back as row/column in %dh/%dl.
				 */

				push  bx;	Save argument register
				mov   bx, page;	Get screen number & cursor pos
				mov   ax, 0300h;
				int   10h
				mov   row, dh
				mov   col, dl
				pop   bx
			}

			if (c == '\t') {
				/*
				 *  If we're doing tab processing, emit spaces
				 *  up to the next tab stop and change the
				 *  output character to a space as well.
				 */

				while (++col % 7) putchar(' ');
				c = ' ';

			} else if (col) _asm {
				/*
				 *  If we're back spacing over a charcter that's
				 *  already on the screen, print ("\b ") to
				 *  erase it before backing the cursor up!
				 */

				push  bx;	Save argument register
				mov   bx, page
				mov   ax, c
				mov   ah, 0Eh;	Print a backspace
				int   10h
				mov   al, 40h;	Print a space
				int   10h
				pop   bx
			}

			break;
		}

		case '\n': {
			/*
			 *  Emit a carriage return before each line feed.
			 *  We do this by increasing the output loop count
			 *  and changing the output char.
			 */

			c = '\r';
			n = 2;
			break;
		}
	}

	while (n--) _asm {
	 	/*
		 *  Issue BIOS call to write the next output character to the
		 *  console!
		 */

		push  bx
		mov   bx, page
		mov   ax, c
		mov   ah, 0Eh
		int   10h
		mov   ax, rc
		mov   c, ax
		pop   bx
	}

	return (rc);
}

void putchar(char c)
{
	/*
	 *  The putchar routine:
	 *
	 *  All we do here is call thru the putchar vector at "putvec".  If
	 *  This vector is null upon entry, however, it means that we were
	 *  not entered thru a bef extension entry point and must set the
	 *  pointer to point at the "int10" putchar routine, above.
	 */

	if (c != '\r') {
		/*
		 *  Ignore carriage returns!  These screw things up badly
		 *  if we're using the bootconf callback.
		 */

		extern int (*putvec)(int);

		if (putvec == 0) putvec = _bios_putchar;
		(void)(*putvec)(c);
	}
}
