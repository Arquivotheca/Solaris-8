/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vga.c	1.10	99/11/01 SMI"

/*
 * Miniature VGA driver for bootstrap.
 */

#include <sys/archsystm.h>
#include <sys/vgareg.h>
#include "vga.h"

#define	VGA_COLOR_CRTC_INDEX	0x3d4
#define	VGA_COLOR_CRTC_DATA	0x3d5
#define	VGA_TEXT_COLS		80
#define	VGA_TEXT_ROWS		25
#define	VGA_SCREEN		((unsigned short *)0xb8000)

static void vga_set_crtc(int index, unsigned char val);
static unsigned char vga_get_crtc(int index);

void
vga_clear(int color)
{
	unsigned short val;
	int i;

	val = (color << 8) | ' ';

	for (i = 0; i < VGA_TEXT_ROWS * VGA_TEXT_COLS; i++) {
		VGA_SCREEN[i] = val;
	}
}

void
vga_drawc(int c, int color)
{
	int row;
	int col;

	vga_getpos(&row, &col);
	VGA_SCREEN[row*VGA_TEXT_COLS + col] = (color << 8) | c;
}

void
vga_scroll(int color)
{
	unsigned short val;
	int i;

	val = (color << 8) | ' ';

	for (i = 0; i < (VGA_TEXT_ROWS-1)*VGA_TEXT_COLS; i++) {
		VGA_SCREEN[i] = VGA_SCREEN[i + VGA_TEXT_COLS];
	}
	for (; i < VGA_TEXT_ROWS * VGA_TEXT_COLS; i++) {
		VGA_SCREEN[i] = val;
	}
}

void
vga_setpos(int row, int col)
{
	int off;

	off = row * VGA_TEXT_COLS + col;
	vga_set_crtc(VGA_CRTC_CLAH, off >> 8);
	vga_set_crtc(VGA_CRTC_CLAL, off & 0xff);
}

void
vga_getpos(int *row, int *col)
{
	int off;

	off = (vga_get_crtc(VGA_CRTC_CLAH) << 8) +
		vga_get_crtc(VGA_CRTC_CLAL);
	*row = off / VGA_TEXT_COLS;
	*col = off % VGA_TEXT_COLS;
}

static void
vga_set_crtc(int index, unsigned char val)
{
	outb(VGA_COLOR_CRTC_INDEX, index);
	outb(VGA_COLOR_CRTC_DATA, val);
}


static unsigned char
vga_get_crtc(int index)
{
	outb(VGA_COLOR_CRTC_INDEX, index);
	return (inb(VGA_COLOR_CRTC_DATA));
}
