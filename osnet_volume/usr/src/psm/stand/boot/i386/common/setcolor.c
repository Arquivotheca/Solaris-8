/*
 * Copyright (c) 1993-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)setcolor.c	1.7	99/03/02 SMI"

/* boot shell setcolor built-in commands */

#include <sys/types.h>
#include <sys/bsh.h>
#include <sys/bootlink.h>
#include <sys/colors.h>
#include <sys/salib.h>

#define	NCOLORS 16
struct { char *name; int value; } xlat[] = {
	{ "black",	BLACK	 },
	{ "blue",	BLUE	 },
	{ "green",	GREEN	 },
	{ "cyan",	CYAN	 },
	{ "red",	RED	 },
	{ "magenta",	MAGENTA	 },
	{ "brown",	BROWN	 },
	{ "white",	WHITE	 },
	{ "gray",	GRAY	 },
	{ "lt_blue",	LBLUE	 },
	{ "lt_green",	LGREEN	 },
	{ "lt_cyan",	LCYAN	 },
	{ "lt_red",	LRED	 },
	{ "lt_magenta",	LMAGENTA },
	{ "yellow",	YELLOW	 },
	{ "hi_white",	LWHITE	 }
};

static void set_screen_attrs();
extern struct int_pb	ic;
extern int doint();

static int getcolor(char *);
static int get_bg();

/* setcolor_cmd() - implements setcolor command - changes display attributes */

#define	FG	2
#define	BOTH	3

void
setcolor_cmd(int argc, char *argv[])
{
	register int fg, bg;

	switch (argc) {
	case FG:
		fg = getcolor(argv[1]);
		if (fg == -1)
			return;
		bg = get_bg();
		set_screen_attrs(fg, bg);
		return;

	case BOTH:
		fg = getcolor(argv[1]);
		bg = getcolor(argv[2]);
		if (bg == fg) {
			printf("error: foreground/background colors "
			    "are the same\n");
		} else if (fg != -1 && bg != -1)
			set_screen_attrs(fg, bg);
		return;

	default:
		printf("error: invalid setcolor command syntax.\n");
		return;
	}
}

static int
getcolor(register char *clrp)
{
	register int idx;

	for (idx = 0; idx < NCOLORS; idx++) {
		if (strcmp(clrp, xlat[idx].name) == 0)
			return (idx);
	}

	printf("error: invalid color `%s` specified\n", clrp);
	return (-1);
}

static int
get_bg()
{
	/*
	 * In this case, we're only setting one attribute, so we need to
	 * preserve the original background color.
	 *
	 * Read character at cursor position.
	 */
	ic.intval = 0x10;
	ic.ax = 0x0800;
	ic.bx = ic.cx = ic.dx = 0;
	(void) doint();
	return ((int)(ic.ax >> 12));
}

static void
set_screen_attrs(register int fg, register int bg)
{
	extern int console_color_attr;

	/* Cursor to 0,0 */
	ic.intval = 0x10;
	ic.ax = 0x0200;
	ic.bx = ic.cx = ic.dx = 0;
	(void) doint();

	/* Write screen with spaces of the appropriate color */
	ic.intval = 0x10;
	ic.ax = 0x0920;
	ic.bx = (bg << 4) | fg;
	console_color_attr = ic.bx;
	ic.cx = 25*80;
	(void) doint();
}
