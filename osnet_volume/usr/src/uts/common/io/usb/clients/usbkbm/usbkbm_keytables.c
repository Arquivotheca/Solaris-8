/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)usbkbm_keytables.c	1.7	99/10/07 SMI"

/*
 * This module contains the translation tables for the up-down encoded
 * USB keyboards.
 */

#define	KEYMAP_SIZE_VARIABLE

#include <sys/param.h>
#include <sys/kbd.h>
#include <sys/stream.h>
#include <sys/consdev.h>
#include <sys/usb/clients/hid/hid.h>
#include <sys/usb/clients/hid/hid_polled.h>
#include <sys/usb/clients/hidparser/hidparser.h>
#include <sys/kbtrans.h>
#include <sys/usb/clients/usbkbm/usbkbm.h>

/* handy way to define control characters in the tables */
#define	c(char)(char&0x1F)
#define	ESC 0x1B
#define	DEL 0x7F

/* Unshifted keyboard table for USB keyboard */

keymap_entry_t keytab_usb_lc[KEYMAP_SIZE_USB] = {
/*   0 */	HOLE, HOLE, HOLE, ERROR, 'a', 'b', 'c', 'd',
/*   8 */	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l',
/*  16 */	'm', 'n', 'o', 'p', 'q', 'r', 's', 't',
/*  24 */	'u', 'v', 'w', 'x', 'y', 'z', '1', '2',
/*  32 */	'3', '4', '5', '6', '7', '8', '9', '0',
/*  40 */	'\r', ESC, '\b', '\t', ' ', '-', '=', '[',
/*  48  */	']',   '\\',   HOLE,    ';',   '\'',    '`',   ',',   '.',
/*  56 */	'/', SHIFTKEYS+CAPSLOCK, TF(1), TF(2), TF(3),
		TF(4), TF(5), TF(6),
/*  64 */	TF(7), TF(8), TF(9), TF(10), TF(11), TF(12),
		RF(2), RF(3),
/*  72 */	RF(1), BF(8), RF(7), RF(9), DEL, RF(13), RF(15),
					STRING+RIGHTARROW,
/*  80 */	STRING+LEFTARROW, STRING+DOWNARROW, STRING+UPARROW,
					SHIFTKEYS+NUMLOCK, RF(5),
		RF(6), BF(15), BF(14),
/*  88 */	BF(11), RF(13), STRING+DOWNARROW, RF(15), STRING+LEFTARROW, \
		RF(11), STRING+RIGHTARROW, RF(7),
/*  96 */	STRING+UPARROW, RF(9), BF(8), BF(10), HOLE, COMPOSE,
		BF(13), HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, LF(7), LF(16), LF(3), LF(5),
/* 120 */	BUCKYBITS+SYSTEMBIT, LF(2), LF(4), LF(10), LF(6), LF(8), \
		LF(9), RF(4),
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, '\r', HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT, SHIFTKEYS+ALT,
		BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL, SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
};


/* Shifted keyboard table for USB keyboard */

keymap_entry_t keytab_usb_uc[KEYMAP_SIZE_USB] = {
/*   0 */	HOLE, HOLE, HOLE, ERROR, 'A', 'B', 'C', 'D',
/*   8 */	'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
/*  16 */	'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
/*  24 */	'U', 'V', 'W', 'X', 'Y', 'Z', '!', '@',
/*  32 */	'#', '$', '%', '^', '&', '*', '(', ')',
/*  40 */	'\r', ESC, '\b', '\t', ' ', '_', '+', '{',
/*  48  */	'}',   '|',   HOLE,    ':',   '"',  '~',   '<',   '>',
/*  56 */	'?', SHIFTKEYS+CAPSLOCK, TF(1), TF(2), TF(3),
		TF(4), TF(5), TF(6),
/*  64 */	TF(7), TF(8), TF(9), TF(10), TF(11), TF(12),
		RF(2), RF(3),
/*  72 */	RF(1), BF(8), RF(7), RF(9), DEL, RF(13), RF(15),
					STRING+RIGHTARROW,
/*  80 */	STRING+LEFTARROW, STRING+DOWNARROW, STRING+UPARROW,
					SHIFTKEYS+NUMLOCK, RF(5), RF(6), \
		BF(15), BF(14), \
/*  88 */	BF(11), RF(13), STRING+DOWNARROW, RF(15), \
		STRING+LEFTARROW, RF(11), STRING+RIGHTARROW, RF(7),
/*  96 */	STRING+UPARROW, RF(9), BF(8), BF(10), HOLE, COMPOSE,
		BF(13), HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, LF(7), LF(16), LF(3), LF(5),
/* 120 */	BUCKYBITS+SYSTEMBIT, LF(2), LF(4), LF(10), LF(6), \
		LF(8), LF(9), RF(4),
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, '\r', HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT, SHIFTKEYS+ALT,
		BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL, SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
	};


/* Caps Locked keyboard table for USB keyboard */

keymap_entry_t keytab_usb_cl[KEYMAP_SIZE_USB] = {

/*   0 */	HOLE, HOLE, HOLE, ERROR, 'A', 'B', 'C', 'D',
/*   8 */	'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
/*  16 */	'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
/*  24 */	'U', 'V', 'W', 'X', 'Y', 'Z', '1', '2',
/*  32 */	'3', '4', '5', '6', '7', '8', '9', '0',
/*  40 */	'\r', ESC, '\b', '\t', ' ', '-', '=', '[',
/*  48  */	']',   '\\',   HOLE,    ';',   '\'',  '`',   ',',   '.',
/*  56 */	'/', SHIFTKEYS+CAPSLOCK, TF(1), TF(2), TF(3),
		TF(4), TF(5), TF(6),
/*  64 */	TF(7), TF(8), TF(9), TF(10), TF(11), TF(12),
		RF(2), RF(3),
/*  72 */	RF(1), BF(8), RF(7), RF(9), DEL, RF(13), RF(15),
						STRING+RIGHTARROW,
/*  80 */	STRING+LEFTARROW, STRING+DOWNARROW, STRING+UPARROW,
			SHIFTKEYS+NUMLOCK, RF(5), RF(6), BF(15), BF(14),
/*  88 */	BF(11), RF(13), STRING+DOWNARROW, RF(15),
		STRING+LEFTARROW, RF(11), STRING+RIGHTARROW, RF(7),
/*  96 */	STRING+UPARROW, RF(9), BF(8), BF(10), HOLE, COMPOSE,
		BF(13), HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, LF(7), LF(16), LF(3), LF(5),
/* 120 */	BUCKYBITS+SYSTEMBIT, LF(2), LF(4), LF(10), LF(6),
		LF(8), LF(9), RF(4),
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, '\r', HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT,
		SHIFTKEYS+ALT, BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL,
		SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
	};


/* Alt Graph keyboard table for USB keyboard */

keymap_entry_t keytab_usb_ag[KEYMAP_SIZE_USB] = {
/*  0 */	HOLE,	HOLE, HOLE,	ERROR,	NOP,	NOP,	NOP,	NOP,
/*  8 */	NOP, 	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 16 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 24 */	NOP, 	NOP, 	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 32 */	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 40 */	'\r',	ESC,	'\b',	'\t',	' ',	NOP,	NOP,	NOP,
/* 48 */	NOP,	NOP,	HOLE,	NOP,	NOP,	NOP,	NOP,	NOP,
/* 56 */	NOP,	SHIFTKEYS+CAPSLOCK,	TF(1), TF(2),
				TF(3),	TF(4),	TF(5),	TF(6),
/* 64 */	TF(7),	TF(8),	 TF(9),	TF(10),
					TF(11),	TF(12), RF(2),	RF(3),
/* 72 */	RF(1),	BF(8),	RF(7),	RF(9),	DEL, RF(13), RF(15),
					STRING+RIGHTARROW,
/* 80 */	STRING+LEFTARROW, STRING+DOWNARROW, STRING+UPARROW,
			SHIFTKEYS+NUMLOCK, RF(5), RF(6), BF(15), BF(14),
/* 88 */	BF(11),	RF(13),	STRING+DOWNARROW, RF(15),
			STRING+LEFTARROW, RF(11), STRING+RIGHTARROW, RF(7),
/* 96 */	STRING+UPARROW,	RF(9),	BF(8), BF(10),
					HOLE,	COMPOSE, BF(13), HOLE,
/* 104 */	HOLE,	HOLE,	HOLE,	HOLE,	HOLE,	HOLE, HOLE,	HOLE,
/* 112 */	HOLE,	HOLE, HOLE,	HOLE,	LF(7),	LF(16), LF(3), LF(5),
/* 120 */	BUCKYBITS+SYSTEMBIT, LF(2),	LF(4), LF(10), LF(6),
		LF(8),	LF(9),	RF(4),
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, '\r', HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT, SHIFTKEYS+ALT,
		BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL, SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
};

/* Num Locked keyboard table for USB keyboard */

keymap_entry_t keytab_usb_nl[KEYMAP_SIZE_USB] = {

/*   0 */	HOLE, HOLE, HOLE, NONL, NONL, NONL, NONL, NONL,
/*   8 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  16 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  24 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  32 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  40 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  48 */	NONL, NONL, HOLE, NONL, NONL, NONL, NONL, NONL,
/*  56 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  64 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  72 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/*  80 */	NONL, NONL, NONL, NONL, PADSLASH, PADSTAR, PADMINUS, PADPLUS,
/*  88 */	PADENTER, PAD1, PAD2, PAD3, PAD4, PAD5, PAD6, PAD7,
/*  96 */	PAD8, PAD9, PAD0, PADDOT, HOLE, NONL, NONL, HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, NONL, NONL, NONL, NONL,
/* 120 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, PADEQUAL,
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, NONL, HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	NONL, NONL, NONL, NONL, NONL, NONL, NONL, NONL,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,




};

/* Controlled keyboard table for USB keyboard */

keymap_entry_t keytab_usb_ct[KEYMAP_SIZE_USB] = {
/*   0 */	HOLE, HOLE, HOLE, ERROR, c('a'), c('b'), c('c'), c('d'),
/*   8 */	c('e'), c('f'), c('g'), c('h'), c('i'), c('j'), c('k'), c('l'),
/*  16 */	c('m'), c('n'), c('o'), c('p'), c('q'), c('r'), c('s'), c('t'),
/*  24 */	c('u'), c('v'), c('w'), c('x'), c('y'), c('z'), '1', c(' '),
/*  32 */	'3', '4', '5', c('^'), '7', '8', '9', '0',
/*  40 */	'\r', ESC, '\b', '\t', c(' '), c('_'), '=', ESC,
/*  48  */	c(']'),   c('\\'),   HOLE,    ';',   '\'',    c('^'),
		',',   '.',
/*  56 */	c('_'), SHIFTKEYS+CAPSLOCK, TF(1), TF(2), TF(3),
		TF(4), TF(5), TF(6),
/*  64 */	TF(7), TF(8), TF(9), TF(10), TF(11), TF(12),
		RF(2), RF(3),
/*  72 */	RF(1), BF(8), RF(7), RF(9), DEL, RF(13), RF(15),
						STRING+RIGHTARROW,
/*  80 */	STRING+LEFTARROW, STRING+DOWNARROW, STRING+UPARROW,
		SHIFTKEYS+NUMLOCK, RF(5), RF(6), BF(15), BF(14),
/*  88 */	BF(11), RF(13), STRING+DOWNARROW, RF(15),
		STRING+LEFTARROW, RF(11), STRING+RIGHTARROW, RF(7),
/*  96 */	STRING+UPARROW, RF(9), BF(8), BF(10), HOLE, COMPOSE,
		BF(13), HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, LF(7), LF(16), LF(3), LF(5),
/* 120 */	BUCKYBITS+SYSTEMBIT, LF(2), LF(4), LF(10), LF(6),
		LF(8), LF(9), RF(4),
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, '\r', HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT, SHIFTKEYS+ALT,
		BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL, SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,


};

/* "Key Up" keyboard table for USB keyboard */

keymap_entry_t keytab_usb_up[KEYMAP_SIZE_USB] = {

/*   0 */	HOLE, HOLE, HOLE, NOP, NOP, NOP, NOP, NOP,
/*   8 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  16 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  24 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  32 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  40 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  48  */	NOP, NOP, HOLE, NOP, NOP, NOP, NOP, NOP,
/*  56 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  64 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  72 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  80 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  88 */	NOP, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/*  96 */	NOP, NOP, NOP, NOP, HOLE, NOP, NOP, HOLE,
/* 104 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 112 */	HOLE, HOLE, HOLE, HOLE, NOP, NOP, NOP, NOP,
/* 120 */	BUCKYBITS+SYSTEMBIT, NOP, NOP, NOP, NOP, NOP, NOP, NOP,
/* 128 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 136 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 144 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 152 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, NOP, HOLE,
/* 160 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 168 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 176 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 184 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 192 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 200 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 208 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 216 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 224 */	SHIFTKEYS+LEFTCTRL, SHIFTKEYS+LEFTSHIFT, SHIFTKEYS+ALT,
		BUCKYBITS+METABIT, SHIFTKEYS+RIGHTCTRL, SHIFTKEYS+RIGHTSHIFT,
		SHIFTKEYS+ALTGRAPH, BUCKYBITS+METABIT,
/* 232 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 240 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
/* 248 */	HOLE, HOLE, HOLE, HOLE, HOLE, HOLE, HOLE,
	};



/* Index to keymaps for USB keyboard */
struct keyboard keyindex_usb = {
	KEYMAP_SIZE_USB,
	keytab_usb_lc,
	keytab_usb_uc,
	keytab_usb_cl,
	keytab_usb_ag,
	keytab_usb_nl,
	keytab_usb_ct,
	keytab_usb_up,
	0x0000,		/* Shift bits which stay on with idle keyboard */
	0x0000,		/* Bucky bits which stay on with idle keyboard */
	120, 58, 4,		/* abort keys {stop,F1}-a */
	CAPSMASK|NUMLOCKMASK,	/* Shift bits which toggle on down event */
	NULL,		/* Exception table */
};
