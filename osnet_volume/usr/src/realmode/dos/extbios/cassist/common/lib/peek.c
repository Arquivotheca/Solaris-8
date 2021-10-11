/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Realmode access to I/O ports:
 *
 *    This file contains two routines, "inp" and "outp", which may be used
 *    to read/write bytes from/to a given I/O port.
 */

#ident	"<@(#)peek.c	1.5	96/04/26	SMI>"

int
inp(int port)
{
	/*
	 *  Read from an I/O port:
	 *
	 *  This routine reads a byte from the specified I/O "port" and returns
	 *  it to the caller.
	 */

	unsigned char v;

	_asm {
		/*
		 *  Issue an "in" instruction to read from the named port.
		 *  Result appears in the low byte of the accumulator, which
		 *  we save in "v" to be returned to the caller.
		 */

		mov   dx, port
		xor   ax, ax
		in    al, dx
		mov   v, al
	}

	return (v & 0xFF);
}

void
outp(int port, int v)
{
	/*
	 *  Write to an I/O port:
	 *
	 *  This routine writes the given "v"alue byte to the specified "port".
	 */

	_asm {
		/*
		 *  Put the "v"alue byte in the accumulator and write it to the
		 *  named port with an "out" instruction.
		 */

		mov   dx, port
		mov   ax, v
		out   dx, al
	}
}
