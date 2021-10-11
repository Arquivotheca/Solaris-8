/*
 * Copyright (c) 1991, 1996, 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)kbd.c	1.10	99/05/04 SMI"

/*
 *	Usage:	kbd [-r] [-t]  [-i] [-c on|off] [-a enable|disable|alternate]
 *		    [-d keyboard device]
 *	-r			reset the keyboard as if power-up
 *	-t			return the type of the keyboard being used
 *	-i			read in the default configuration file
 *	-c on|off		turn on|off clicking
 *	-a enable|disable|alternate	sets abort sequence
 *	-d keyboard device	chooses the kbd device, default /dev/kbd.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/kbio.h>
#include <sys/kbd.h>
#include <stdio.h>
#include <fcntl.h>
#include <deflt.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define	KBD_DEVICE	"/dev/kbd"		/* default keyboard device */
#define	DEF_FILE	"/etc/default/kbd"	/* kbd defaults file	*/
#define	DEF_ABORT	"KEYBOARD_ABORT="
#define	DEF_CLICK	"KEYCLICK="

static void reset(int);
static void get_type(int);
static void kbd_defaults(int);
static void usage(void);

static int click(char *, int);
static int abort_enable(char *, int);

main(int argc, char **argv)
{
	int c, error;
	int rflag, tflag, cflag, dflag, aflag, iflag, errflag;
	char *copt, *aopt;
	char *kbdname = KBD_DEVICE;
	int kbd;
	extern char *optarg;
	extern int optind;

	rflag = tflag = cflag = dflag = aflag = iflag = errflag = 0;
	copt = aopt = (char *)0;

	while ((c = getopt(argc, argv, "rtic:a:d:")) != EOF) {
		switch (c) {
		case 'r':
			rflag++;
			break;
		case 't':
			tflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'c':
			copt = optarg;
			cflag++;
			break;
		case 'a':
			aopt = optarg;
			aflag++;
			break;
		case 'd':
			kbdname = optarg;
			dflag++;
			break;
		case '?':
			errflag++;
			break;
		}
	}

	/*
	 * check for valid arguments
	 */
	if ((errflag != 0) || (cflag && argc < 3) || (dflag && argc < 3) ||
	    (aflag && argc < 3) || (argc != optind) || (argc == 1)) {
		usage();
		exit(1);
	}

	if (dflag && !rflag && !tflag && !cflag && !aflag && !iflag) {
		usage();
		exit(1);
	}

	if (iflag && (rflag || tflag || cflag || aflag)) {
		usage();
		exit(1);
	}

	/*
	 * Open the keyboard device
	 */
	if ((kbd = open(kbdname, O_RDWR)) < 0) {
		perror("opening the keyboard");
		(void) fprintf(stderr, "kbd: Cannot open %s\n", kbdname);
		exit(1);
	}

	if (iflag) {
		kbd_defaults(kbd);
		exit(0);	/* A mutually exclusive option */
		/*NOTREACHED*/
	}

	if (tflag)
		get_type(kbd);

	if (cflag)
		if ((error = click(copt, kbd)) != 0)
			exit(error);

	if (rflag)
		reset(kbd);

	if (aflag)
		if ((error = abort_enable(aopt, kbd)) != 0)
			exit(error);

	return (0);
}

/*
 * this routine resets the state of the keyboard as if power-up
 */
static void
reset(int kbd)
{
	int cmd;

	cmd = KBD_CMD_RESET;

	if (ioctl(kbd, KIOCCMD, &cmd)) {
		perror("kbd: ioctl error");
		exit(1);
	}

}

/*
 * this routine gets the type of the keyboard being used
 */
static void
get_type(int kbd)
{
	int kbd_type;

	if (ioctl(kbd, KIOCTYPE, &kbd_type)) {
		perror("ioctl (kbd type)");
		exit(1);
	}

	switch (kbd_type) {

	case KB_SUN3:
		(void) printf("Type 3 Sun keyboard\n");
		break;

	case KB_SUN4:
		(void) printf("Type 4 Sun keyboard\n");
		break;

	case KB_ASCII:
		(void) printf("ASCII\n");
		break;

	case KB_PC:
		(void) printf("PC\n");
		break;

	case KB_USB:
		(void) printf("USB keyboard\n");
		break;

	default:
		(void) printf("Unknown keyboard type\n");
		break;
	}
}

/*
 * this routine enables or disables clicking of the keyboard
 */
static int
click(char *copt, int kbd)
{
	int cmd;

	if (strcmp(copt, "on") == 0)
		cmd = KBD_CMD_CLICK;
	else if (strcmp(copt, "off") == 0)
		cmd = KBD_CMD_NOCLICK;
	else {
		(void) fprintf(stderr, "wrong option -- %s\n", copt);
		usage();
		return (1);
	}

	if (ioctl(kbd, KIOCCMD, &cmd)) {
		perror("kbd ioctl (keyclick)");
		return (1);
	}
	return (0);
}

/*
 * this routine enables/disables/sets BRK or abort sequence feature
 */
static int
abort_enable(char *aopt, int kbd)
{
	int enable;

	if (strcmp(aopt, "alternate") == 0)
		enable = KIOCABORTALTERNATE;
	else if (strcmp(aopt, "enable") == 0)
		enable = KIOCABORTENABLE;
	else if (strcmp(aopt, "disable") == 0)
		enable = KIOCABORTDISABLE;
	else {
		(void) fprintf(stderr, "wrong option -- %s\n", aopt);
		usage();
		return (1);
	}

	if (ioctl(kbd, KIOCSKABORTEN, &enable)) {
		perror("kbd ioctl (abort enable)");
		return (1);
	}
	return (0);
}

static char *bad_default = "kbd: bad default value for %s: %s\n";

static void
kbd_defaults(int kbd)
{
	char *p;

	if (defopen(DEF_FILE) != 0) {
		(void) fprintf(stderr, "Can't open default file: %s\n",
		    DEF_FILE);
		exit(1);
	}

	p = defread(DEF_CLICK);
	if (p != NULL) {
		/*
		 * KEYCLICK must equal "on" or "off"
		 */
		if ((strcmp(p, "on") == 0) || (strcmp(p, "off") == 0))
			(void) click(p, kbd);
		else
			(void) fprintf(stderr, bad_default, DEF_CLICK, p);
	}

	p = defread(DEF_ABORT);
	if (p != NULL) {
		/*
		 * ABORT must equal "enable", "disable" or "alternate"
		 */
		if ((strcmp(p, "enable") == 0) ||
		    (strcmp(p, "alternate") == 0) ||
		    (strcmp(p, "disable") == 0))
			(void) abort_enable(p, kbd);
		else
			(void) fprintf(stderr, bad_default, DEF_ABORT, p);
	}
}

static char *usage1 = "kbd [-r] [-t] [-a enable|disable|alternate] [-c on|off]";
static char *usage2 = "    [-d keyboard device]";
static char *usage3 = "kbd -i [-d keyboard device]";

static void
usage(void)
{
	(void) fprintf(stderr, "Usage:\t%s\n\t%s\n\t%s\n", usage1, usage2,
	    usage3);
}
