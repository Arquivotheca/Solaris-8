/*
 * Copyright (c) 1994-1995,1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)uname-i.c	1.11	98/05/15 SMI"

#include <sys/param.h>
#include <sys/platnames.h>
#include <sys/salib.h>
#include <sys/bootconf.h>

extern struct bootops *bopp;

#define	MAXNMLEN	1024		/* # of chars in a property */

enum ia_state_mach {
	STATE_ARCH_NAME,
	STATE_NAME,
	STATE_COMPAT_INIT,
	STATE_COMPAT,
	STATE_PROBED_ARCH_NAME,
	STATE_PROBED_COMPAT_INIT,
	STATE_PROBED_COMPAT,
	STATE_DEFAULT,
	STATE_FINI
};

phandle_t root_nodep = NULL;

/*
 * Return the implementation architecture name (uname -i) for this platform.
 *
 * Use the named rootnode property to determine the iarch; if the name is
 * an empty string, use the cputype.
 */
static char *
get_impl_arch_name(enum ia_state_mach *state, int use_default)
{
	static char iarch[MAXNMLEN];
	static int len;
	static char *ia;
	char *namename, ch;
	int i;

newstate:
	switch (*state) {
	case STATE_ARCH_NAME:
		*state = STATE_NAME;
		namename = "arch-name";
		len = BOP_GETPROPLEN(bopp, namename);
		if (len <= 0 || len >= MAXNMLEN)
			goto newstate;
		(void) BOP_GETPROP(bopp, namename, iarch);
		iarch[len] = '\0';
		ia = iarch;
		break;

	case STATE_NAME:
		*state = STATE_COMPAT_INIT;
		namename = "name";
		len = BOP1275_GETPROPLEN(bopp, root_nodep, namename);
		if (len <= 0 || len >= MAXNMLEN)
			goto newstate;
		(void) BOP1275_GETPROP(bopp, root_nodep, namename, iarch, len);
		iarch[len] = '\0';
		ia = iarch;
		break;

	case STATE_COMPAT_INIT:
		*state = STATE_COMPAT;
		namename = "compatible";
		len = BOP1275_GETPROPLEN(bopp, root_nodep, namename);
		if (len <= 0 || len >= MAXNMLEN) {
			*state = STATE_DEFAULT;
			goto newstate;
		}
		(void) BOP1275_GETPROP(bopp, root_nodep, namename, iarch, len);
		iarch[len] = '\0';	/* ensure null termination */
		for (i = 0; i < len; i++)
			if ((ch = iarch[i]) == ':' || ch == ' ')
				iarch[i] = '\0';
		ia = iarch;
		break;

	case STATE_COMPAT:
		/*
		 * Advance 'ia' to point to next string in
		 * compatible property array (if any).
		 */
		while (*ia++)
			;
		if ((ia - iarch) >= len) {
			*state = STATE_DEFAULT;
			goto newstate;
		}
		break;

	case STATE_DEFAULT:
		*state = STATE_FINI;
		if (! use_default)
			goto newstate;
		(void) strcpy(iarch, "i86pc");
		ia = iarch;
		break;

	case STATE_FINI:
		return (NULL);
	}

	return (ia);
}

static void
make_platform_path(char *fullpath, char *iarch, char *filename)
{
	(void) strcpy(fullpath, "/platform/");
	(void) strcat(fullpath, iarch);
	(void) strcat(fullpath, "/");
	if (filename)
		(void) strcat(fullpath, filename);
}

/*
 * Generate impl_arch_name by searching the /platform hierarchy
 * for a matching directory.  We are not looking for any particular
 * file here, but for a directory hierarchy for the module path.
 */
int
find_platform_dir(int (*isdirfn)(char *), char *iarch, int use_default)
{
	char fullpath[MAXPATHLEN];
	char *ia;
	enum ia_state_mach state = STATE_ARCH_NAME;

	if ((bopp == NULL) || (root_nodep == NULL &&
			((root_nodep = BOP1275_PEER(bopp, NULL)) == NULL))) {
		/* this shouldn't happen, but just in case return error */
		return (0);
	}

	/*
	 * Hunt the filesystem looking for a directory hierarchy.
	 */
	while ((ia = get_impl_arch_name(&state, use_default)) != NULL) {
		make_platform_path(fullpath, ia, NULL);
		if (((*isdirfn)(fullpath)) != 0) {
			(void) strcpy(iarch, ia);
			return (1);
		}
	}
	return (0);
}

/*
 * Search the /platform hierarchy looking for a particular file.
 *
 * impl_arch_name is given as an optional hint as to where the
 * file might be found.
 */
int
open_platform_file(
	char *filename,
	int (*openfn)(char *, void *),
	void *arg,
	char *fullpath,
	char *given_iarch)
{
	char *ia;
	int fd;
	enum ia_state_mach state = STATE_ARCH_NAME;

	if ((bopp == NULL) || (root_nodep == NULL &&
			((root_nodep = BOP1275_PEER(bopp, NULL)) == NULL))) {
		/*
		 * this shouldn't happen, but just in case default to i86pc
		 */
		make_platform_path(fullpath, "i86pc", filename);
		if ((fd = (*openfn)(fullpath, arg)) != -1)
			return (fd);
		else
			return (-1);
	}

	/*
	 * First try the impl_arch_name hint.
	 *
	 * This is only here to support the -I flag to boot.
	 */
	if (given_iarch != NULL) {
		make_platform_path(fullpath, given_iarch, filename);
		return ((*openfn)(fullpath, arg));
	}

	/*
	 * Hunt the filesystem for one that works ..
	 */
	while ((ia = get_impl_arch_name(&state, 1)) != NULL) {
		make_platform_path(fullpath, ia, filename);
		if ((fd = (*openfn)(fullpath, arg)) != -1) {
			return (fd);
		}
	}
	return (-1);
}

/*
 * Is path "/platform/"dir"/" ?
 */
static int
platcmp(char *path, char *dir)
{
	static char prefix[] = "/platform/";
	static char suffix[] = "/kernel";
	int len;

	if (strncmp(path, prefix, sizeof (prefix) - 1) != 0)
		return (0);
	len = strlen(dir);
	path += sizeof (prefix) - 1;
	if (strncmp(path, dir, len) != 0)
		return (0);
	path += len;
	if (strcmp(path, suffix) != 0)
		return (0);
	return (1);
}

/*
 * This function provides a hook for enhancing the module_path.
 */
/*ARGSUSED*/
void
mod_path_uname_m(char *mod_path, char *ia_name)
{
	/*
	 * If we found the kernel in the default "i86pc" dir, prepend the
	 * ia_name directory (e.g. /platform/SUNW,foo/kernel) to the mod_path
	 * unless ia_name is the same as the default dir.
	 *
	 * If we found the kernel in the ia_name dir, append the default
	 * directory to the modpath.
	 *
	 * If neither of the above are true, we were given a specific kernel
	 * to boot, so we leave things well enough alone.
	 */
	if (platcmp(mod_path, "i86pc")) {
		if (strcmp(ia_name, "i86pc") != 0) {
			char tmp[MAXPATHLEN];

			(void) strcpy(tmp, mod_path);
			(void) strcpy(mod_path, "/platform/");
			(void) strcat(mod_path, ia_name);
			(void) strcat(mod_path, "/kernel ");
			(void) strcat(mod_path, tmp);
		}
	} else if (platcmp(mod_path, ia_name))
		(void) strcat(mod_path, " /platform/i86pc/kernel");
}
