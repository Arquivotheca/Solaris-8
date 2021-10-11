/*
 * Copyright (c) 1995-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consplat.c	1.8	99/09/15 SMI"

/*
 * platform-specific (prom-specific) console configuration routines
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/ddi_impldefs.h>
#include <sys/promif.h>
#include <sys/modctl.h>
#include <sys/termios.h>

static char mousepath[MAXPATHLEN];
static char kbdpath[MAXPATHLEN];
static char beeppath[MAXPATHLEN];

/*
 * Return a property name in /aliases.
 * The caller is responsible for freeing the memory.
 * The property value is NULL terminated string.
 * /aliases exists in OBP >= 2.4.
 */
static char *
get_alias(char *alias, char *buf)
{
	dnode_t node;

	/*
	 * Solve bug in positron prom; aliases aren't null terminated.
	 * This requires that all callers provide a MAXPATHLEN buffer
	 */

	bzero(buf, MAXPATHLEN); /* Some proms don't null-terminate */

	/*
	 * OBP >= 2.4 has /aliases.
	 */
	if ((node = prom_alias_node()) == OBP_BADNODE)
		return ((char *)0);

	if (prom_getproplen(node, (caddr_t)alias) <= 0)
		return ((char *)0);

	(void) prom_getprop(node, (caddr_t)alias, (caddr_t)buf);
	return (buf);
}

/*
 * Return generic path to beep device; From the alias
 */

char *
i_beeppath(void)
{
	char *path;

	if (beeppath[0] != (char)0)
		return (beeppath);

	/*
	 * Get the beep alias
	 */
	path = get_alias("beep", beeppath);
	if (path != 0) {
#ifdef PATH_DEBUG
		prom_printf("beep alias %s\n", beeppath);
#endif
		prom_pathname(path);
		return (path);
	}
	return ((char *)0);
}

/*
 * Return generic path to keyboard device; Either from the alias or
 * by walking the device tree looking for a "keyboard" property
 */

char *
i_kbdpath(void)
{
	char *path;

	if (kbdpath[0] != (char)0)
		return (kbdpath);

	/*
	 * The keyboard alias is required on 1275 systems.
	 */
	path = get_alias("keyboard", kbdpath);
	if (path != 0) {
#ifdef PATH_DEBUG
		prom_printf("keyboard alias %s\n", kbdpath);
#endif
		prom_pathname(path);
		return (path);
	}
	return ((char *)0);
}

char *
i_mousepath(void)
{
	char *path, *p, *q;

	if (mousepath[0] != (char)0)
		return (mousepath);

	/*
	 * look for the mouse property in /aliases.
	 */
	path = get_alias("mouse", mousepath);
	if (path != 0) {
#ifdef PATH_DEBUG
		prom_printf("mouse alias %s\n", mousepath);
#endif
		prom_pathname(mousepath);
		return (mousepath);
	}

	/*
	 * If we didn't find it, it could be on OBP
	 * system, or early 3.0 system or an OBP 1.x or 2.x system
	 *  with a 'zs' port keyboard/mouse duart.
	 * In this case, the mouse is the 'b' channel of the
	 * keyboard duart.
	 */

	path = i_kbdpath();
	if (path == 0)
		return ((char *)0);
	(void) strcpy(mousepath, path);

	/*
	 * Change :a to :b or append :b to the last
	 * component of the path. (It's still canonical without :a)
	 */
	p = (strrchr(mousepath, '/'));		/* p points to last comp. */
	if (p != NULL) {
		q = strchr(p, ':');
		if (q != 0)
			*q = (char)0;		/* Replace or append options */
		(void) strcat(p, ":b");
	}
#ifdef PATH_DEBUG
	prom_printf("mouse derived %s\n", mousepath);
#endif
	return (mousepath);
}

char *
i_stdinpath(void)
{
	return (prom_stdinpath());
}

char *
i_stdoutpath(void)
{
	static char *outpath;
	static char buf[MAXPATHLEN];
	char *p;

	if (outpath != 0)
		return (outpath);

	p = prom_stdoutpath();

	/*
	 * If the ouput device is a framebuffer, we don't
	 * care about monitor resolution options strings.
	 * In fact, we can't handle them at all, so strip them.
	 */
	if (prom_stdout_is_framebuffer()) {
		prom_strip_options(p, buf);
		p = buf;
	}

	outpath = p;
	return (outpath);
}

int
i_stdin_is_keyboard(void)
{
	return (prom_stdin_is_keyboard());
}

int
i_stdout_is_framebuffer(void)
{
	return (prom_stdout_is_framebuffer());
}


/* Baud rate table */
#define	MAX_SPEEDS 24

static struct speed {
	char *name;
	int code;
} speedtab[MAX_SPEEDS] = {
	{"0", B0},		{"50", B50},		{"75", B75},
	{"110", B110},		{"134", B134},		{"150", B150},
	{"200", B200},		{"300", B300},		{"600", B600},
	{"1200", B1200},	{"1800", B1800},	{"2400", B2400},
	{"4800", B4800},	{"9600", B9600},	{"19200", B19200},
	{"38400", B38400},	{"57600", B57600},	{"76800", B76800},
	{"115200", B115200},	{"153600", B153600},	{"230400", B230400},
	{"307200", B307200},	{"460800", B460800},	{"", 0}
};

/*
 * Routine to set baud rate, bits-per-char, parity and stop bits
 * on the console line when necessary.
 */

int
i_setmodes(dev_t dev, struct termios *termiosp)
{
	char buf[MAXPATHLEN];
	int len = MAXPATHLEN;
	char name[16];
	int ppos, i, j;
	char *path;
	dev_t tdev;

	/*
	 * First, search for a devalias which matches this dev_t.
	 * Try all of ttya through ttyz until no such alias
	 */

	(void) strcpy(name, "ttya");
	for (i = 0; i < ('z'-'a'); i++) {
		name[3] = 'a' + i; /* increment device name */
		path = get_alias(name, buf);
		if (path == (char)0) {
#ifdef PATH_DEBUG
			prom_printf("Didn't find alias for %X\n", dev);
#endif
			return (1);
		}
		prom_pathname(path);
		tdev = ddi_pathname_to_dev_t(path);
#ifdef PATH_DEBUG
		prom_printf("Alias for %s is %s, dev_t is %X\n",
			name, path, tdev);
#endif
		if (tdev == dev)
			break;	/* Exit loop if found */
	}

	if (i >= ('z'-'a'))
		return (1);		/* If we didn't find it, return */

	/*
	 * Now that we know which "tty" this corresponds to, retrieve
	 * the "ttya-mode" options property, which tells us how to configure
	 * the line.
	 */

	(void) strcpy(name, "ttya-mode"); /* name of option we want */
	name[3] = 'a' + i;	/* Adjust to correct line */

	for (j = 0; j < MAXPATHLEN; j++) buf[j] = 0; /* CROCK! */
	if (ddi_getlongprop_buf(DDI_DEV_T_ANY, ddi_root_node(), 0, name,
	    buf, &len) != DDI_PROP_SUCCESS) {
#ifdef PATH_DEBUG
		prom_printf("No %s property found\n", name);
#endif PATH_DEBUG
		return (1);	/* if no such option, just return */
	}

#ifdef PATH_DEBUG
	prom_printf("i_setmodes %s contains %s\n", name, buf);
#endif

	/*
	 * Clear out options we will be setting
	 */

	termiosp->c_cflag &=
	    ~(CSIZE | CBAUD | CBAUDEXT | PARODD | PARENB | CSTOPB);

	/*
	 * Now, parse the string. Wish I could use sscanf().
	 * Format 9600,8,n,1,-
	 * baud rate, bits-per-char, parity, stop-bits, ignored
	 */

	for (ppos = 0; ppos < (MAXPATHLEN-8); ppos++) { /* Find first comma */
		if ((buf[ppos] == 0) || (buf[ppos] == ','))
			break;
	}

	if (buf[ppos] != ',') {
		cmn_err(CE_WARN, "i_setmodes: invalid mode string %s", buf);
		return (1);
	}

	for (i = 0; i < MAX_SPEEDS; i++) {
		if (strncmp(buf, speedtab[i].name, ppos) == 0)
		    break;
	}

	if (i >= MAX_SPEEDS) {
		cmn_err(CE_WARN,
			"i_setmodes: unrecognized speed in %s\n", buf);
		return (1);
	}

	/*
	 * Found the baud rate, set it
	 */
	termiosp->c_cflag |= speedtab[i].code & CBAUD;
	if (speedtab[i].code > 16) 			/* cfsetospeed! */
		termiosp->c_cflag |= CBAUDEXT;

	/*
	 * Set bits per character
	 */

	switch (buf[ppos+1]) {
	case ('8'):
		termiosp->c_cflag |= CS8;
		break;
	case ('7'):
		termiosp->c_cflag |= CS7;
		break;
	default:
		cmn_err(CE_WARN, "i_setmodes: Illegal bits-per-char %s", buf);
		return (1);
	}

	/*
	 * Set parity
	 */

	switch (buf[ppos+3]) {
	case ('o'):
		termiosp->c_cflag |= PARENB | PARODD;
		break;
	case ('e'):
		termiosp->c_cflag |= PARENB; /* enabled, not odd */
		break;
	case ('n'):
		break;	/* not enabled. */
	default:
		cmn_err(CE_WARN, "i_setmodes: Illegal parity %s", buf);
		return (1);
	}

	/*
	 * Set stop bits
	 */

	switch (buf[ppos+5]) {
	case ('1'):
		break;	/* No extra stop bit */
	case ('2'):
		termiosp->c_cflag |= CSTOPB; /* 1 extra stop bit */
		break;
	default:
		cmn_err(CE_WARN, "i_setmodes: Illegal stop bits %s", buf);
		return (1);
	}

#ifdef PATH_DEBUG
	prom_printf("From %s we will set %s\n", name, buf);
#endif
	return (0);		/* Success! */
}
