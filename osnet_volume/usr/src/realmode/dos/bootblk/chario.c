/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)chario.c	1.4	99/02/11 SMI\n"

#include "disk.h"
#include "chario.h"

char cons_getc(_char_io_p);
void cons_putc(_char_io_p, char);
int cons_avail(_char_io_p);
void cons_clear_screen(_char_io_p);
void cons_set_cursor(_char_io_p, int, int);

_char_io_t console = {
	(_char_io_p)0,
	"Console",
	0, 0, 0,
	0,
	(char *)0,
	cons_getc,
	cons_putc,
	cons_avail,
	cons_clear_screen,
	cons_set_cursor
};

char
cons_getc(_char_io_p p)
{
	char answer;

	/* ---- Read from keyboard with echo ---- */
	_asm {
		mov	ah, 0
		int	16h
		mov	answer, al
	}
	p->in++;
	return (answer);
}

void
cons_putc(_char_io_p p, char c)
{
	_asm {
		push	bx
		mov	ah, 0eh
		mov	bx, 0
		mov	cx, 1
		mov	al, c
		int	10h
		pop	bx
	}
	p->out++;
}

int
cons_avail(_char_io_p p)
{
	u_short flag;

	_asm {
		mov	ah, 1
		int	16h
		pushf
		pop	flag
	}
	return ((flag & ZERO_FLAG) ? 0 : 1);
}


void
cons_set_cursor(_char_io_p p, int row, int col)
{
	u_char page = ask_page();
	u_char crow = row;
	u_char ccol = col;

	_asm {
		push	bx
		mov	bh, page
		mov	dh, crow
		mov	dl, ccol
		mov	ah, 2
		int	10h
		pop	bx
	}
}


/*
 * []------------------------------------------------------------[]
 * |	clear_screen - clears the screen and sets the		 |
 * |	foreground and background colors			 |
 * []------------------------------------------------------------[]
 */
void
cons_clear_screen(_char_io_p p)
{
	_asm {
		push	es
		pusha

		mov	ax, 40h
		mov	es, ax;		BIOS data area
		mov	di, 49h;	address of video mode
		mov	ah, 0h;		video mode switch function number
		mov	al, es:[di]
		int	10h

		mov	di, 62h;	address of video page
		mov	ax, 0920h
		mov	bh, es:[di]
		mov	bl, 7
		mov	cx, 800h
		int	10h

		popa
		pop	es
	}
}
