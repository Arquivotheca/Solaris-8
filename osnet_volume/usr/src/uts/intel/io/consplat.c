/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)consplat.c	1.11	99/05/12 SMI"

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

extern	char	*prom_stdinpath(void);
extern	char	*prom_stdoutpath(void);
extern	void	prom_strip_options(char *from, char *to);
extern	void	prom_pathname(char *);

static char kbdpath[MAXPATHLEN];
static char fbpath[MAXPATHLEN];
static char mousepath[MAXPATHLEN];

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
		return (NULL);

	if (prom_getproplen(node, (caddr_t)alias) <= 0)
		return (NULL);

	(void) prom_getprop(node, (caddr_t)alias, (caddr_t)buf);
	return (buf);
}

/*
 * Return generic path to keyboard device from the alias.
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
#ifdef PATH_DEBUG
		prom_printf("i_kbdpath returns  %s\n", path);
#endif
		return (path);
	}

	cmn_err(CE_WARN, "No usable keyboard alias!");

	return (NULL);
}

/*
 * Return generic path to display device from the alias.
 */

char *
i_fbpath(void)
{
	char *path;

	if (fbpath[0] != (char)0)
		return (fbpath);

	path = get_alias("screen", fbpath);
	if (path != 0) {
#ifdef PATH_DEBUG
		prom_printf("screen alias %s\n", fbpath);
#endif
		prom_pathname(path);
#ifdef PATH_DEBUG
		prom_printf("i_fbpath returns  %s\n", path);
#endif
		return (path);
	}

	cmn_err(CE_WARN, "No usable screen alias!");

	return (NULL);
}

/*
 * Return generic path to mouse device from the alias.
 */

char *
i_mousepath(void)
{
	char *path;

	if (mousepath[0] != (char)0)
		return (mousepath);

	/*
	 * If we have a "mouse" alias, use it.
	 */
	path = get_alias("mouse", mousepath);
	if (path != 0) {
#ifdef PATH_DEBUG
		prom_printf("mouse alias %s\n", mousepath);
#endif
		prom_pathname(path);
#ifdef PATH_DEBUG
		prom_printf("i_mousepath returns  %s\n", path);
#endif
		return (path);
	}

#undef	BOOTCONF_SUPPLIES_MOUSE_ALIAS
#if	defined(BOOTCONF_SUPPLIES_MOUSE_ALIAS)
	cmn_err(CE_WARN, "No usable mouse alias!");
#endif

	return (NULL);
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
	if (p == NULL)
		return (NULL);

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
