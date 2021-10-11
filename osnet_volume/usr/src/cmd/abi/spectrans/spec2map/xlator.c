/*
 * Copyright (c) 1997-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)xlator.c	1.2	99/02/26 SMI"

/*
 *  Back-end functions for spec to mapfile converter
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include "xlator.h"
#include "util.h"
#include "bucket.h"

/* Globals */
enum {
	/* These first four (commented out) are defined in parser.h */
	/* XLATOR_KW_NOTFOUND = 0, */
	/* XLATOR_KW_FUNC, */
	/* XLATOR_KW_DATA, */
	/* XLATOR_KW_END, */
	XLATOR_KW_VERSION = 4,
	XLATOR_KW_ARCH
};
#define	FIRST_TOKEN 4	/* Must match the first token in the enum above */

static xlator_keyword_t Keywords[] = {
	{ "version", XLATOR_KW_VERSION },
	{ "arch", XLATOR_KW_ARCH },
	{ NULL, XLATOR_KW_NOTFOUND }
};

static char	const *OutputFile;
static char	const *Curfile;
static char	*Curfun;
static int	Curline;
static Interface Iface;

static int  Verbosity;
static int  TargetArchToken;		/* set from -a option to front-end */
char *TargetArchStr = NULL;		/* from -a option to front-end */
static int  Supported_Arch = XLATOR_ALLARCH;	/* from "Arch" SPEC keyword */

/*
 * WHAT!?
 * from Version line
 * 0 means architecture is not specified in the
 * version line so it applies to all versions
 */
static int  Version_Arch;
int  Num_versfiles = 0;
static int  Has_Version;
static int  IsFilterLib;

static char *Versfile;

static char *getversion(const char *);
static int version_sanity(const char *value);
static int arch_version_sanity(char *av);
static void writemapfile(FILE *);
static int set_version_arch(const char *);
static int set_supported_arch(const char *);

/*
 * xlator_init()
 *    back-end initialization
 *    returns pointer to Keywords on success
 *    returns NULL pointer on failure
 */
xlator_keyword_t *
xlator_init(const Translator_info *t_info)
{
	/*
	 * initially so we don't lose error messages from version_check
	 * we'll set this again later based on ti_info.ti_verbosity
	 */
	seterrseverity(WARNING);

	/* set verbosity */
	Verbosity = t_info->ti_verbosity;
	seterrseverity(t_info->ti_verbosity);

	/*
	 * set Library Type
	 * 1 if filter lib, 0 otherwise
	 */
	IsFilterLib = t_info->ti_libtype;

	/* set target architecture */
	TargetArchStr = t_info->ti_arch;
	TargetArchToken = t_info->ti_archtoken;

	errlog(WARNING, "Architecture set to \"%s\"", TargetArchStr);

	/* set output file */
	OutputFile = t_info->ti_output_file;
	if (OutputFile) {
		errlog(WARNING, "Output will go into %s",
		    OutputFile);
	} else {
		OutputFile = "mapfile";
		errlog(WARNING, "Using default output filename: %s",
		    OutputFile);
	}

	/* obtain name of version file */
	Versfile = t_info->ti_versfile;

	/* call create_lists() to setup for parse_versions() */
	create_lists();

	/* Process Vers Files */
	if (parse_versions(Versfile)) {
		return (NULL);
	}

	return (Keywords);
}

/*
 * xlator_startlib()
 *    start of library
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_SKIP		if library is to be skipped
 *              XLATOR_NONFATAL	on error
 */
/*ARGSUSED*/
int
xlator_startlib(char const *libname)
{
	errlog(TRACING, "xlator_startlib");
	return (XLATOR_SUCCESS);
}

/*
 * xlator_startfile()
 *    start of spec file
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_SKIP		if file is to be skipped
 *              XLATOR_NONFATAL	on error
 */
int
xlator_startfile(char const *filename)
{
	errlog(TRACING, "xlator_startfile");

	Curfile = filename;

	return (XLATOR_SUCCESS);
}

/*
 * xlator_start_if ()
 *    start of interface specification
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_SKIP		if interface is to be skipped
 *              XLATOR_NONFATAL	on error
 *              XLATOR_FATAL	on fatal error
 */
int
xlator_start_if(const Meta_info meta_info, const int token, char *value)
{
	char rhs[BUFSIZ];

	errlog(TRACING, "xlator_start_if %s", value);

	Curline = meta_info.mi_line_number;
	seterrline(Curline, meta_info.mi_filename,
	    Keywords[token-FIRST_TOKEN].key, value);

	if (Curfun != NULL) {
		errlog(INPUT|ERROR,
		    "Error in SPEC!  Spec without an End: %s", Curfun);
		return (XLATOR_NONFATAL);
	}

	if (sscanf(value, "%s", rhs) == 0) {
		errlog(INPUT|ERROR, "Error in SPEC!  Invalid Begin line");
		return (XLATOR_NONFATAL);
	}

	Curfun = strdup(rhs);

	if (Curfun == NULL) {
		errlog(ERROR,
		    "Error: Couldn't allocate memory for function name");
		return (XLATOR_NONFATAL);
	}

	Iface.IF_name = Curfun;
	Iface.IF_type = NOTYPE;

	if (IsFilterLib) {
		Iface.IF_type = token;
	}
	Iface.IF_version = NULL;
	Iface.IF_class = NULL;
	Has_Version = 0;
	Supported_Arch = XLATOR_ALLARCH;
	Version_Arch = 0;

	return (XLATOR_SUCCESS);
}

/*
 * xlator_take_kvpair()
 *    processes spec keyword-value pairs
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_NONFATAL	on error
 */
int
xlator_take_kvpair(const Meta_info meta_info, const int token,
	char *value)
{
	char *p;
	char *key = Keywords[token-FIRST_TOKEN].key;

	Curline = meta_info.mi_line_number;
	seterrline(Curline, meta_info.mi_filename, key, value);

	errlog(TRACING,
	    "take_kvpair called. ext_cnt=%d", meta_info.mi_ext_cnt);

	if (Curfun == NULL) {
		errlog(INPUT|ERROR, "Error in spec file. Statement outside "
		    "Begin-End block, line %d", Curline);
		return (XLATOR_NONFATAL);
	}

	switch (token) {
	case XLATOR_KW_VERSION:
		if (meta_info.mi_ext_cnt  !=  0)
			return (XLATOR_SUCCESS);

		errlog(TRACING, "Version found. Setting Version to %s", value);

		/* Version line found ; used for auditing the SPEC */
		Has_Version = 1;

		/* remove trailing white space */
		p = strrchr(value, '\n');
		if (p) {
			while (p >= value && isspace(*p)) {
				*p = '\0';
				--p;
			}
		}

		/* is the version line valid */
		switch (version_sanity(value)) {
		case VS_OK:		/* OK */
			break;

		case VS_INVARCH:	/* Invalid Arch */
			errlog(INPUT|ERROR, "Error in SPEC/versfile! "
			    "Invalid Architecture String", TargetArchStr);
			return (XLATOR_NONFATAL);

		case VS_INVVERS:	/* Invalid Version String */
			errlog(INPUT|ERROR, "Error in SPEC/versfile! "
			    "Invalid Version String");
			return (XLATOR_NONFATAL);

		case VS_INVALID:	/* Both Version and Arch are invalid */
			errlog(INPUT|ERROR, "Error in SPEC/versfile! Invalid "
			    "Version and Architecture String");
			return (XLATOR_NONFATAL);

		default:	/* BAD IMPLEMENTATION OF version_sanity */
			errlog(FATAL, "Error in version_sanity! This should "
			    "never happen!");
		}

		errlog(TRACING, "Version_Arch=%d", Version_Arch);

		Iface.IF_version = getversion(value);
		break;

	case XLATOR_KW_ARCH:
		if (meta_info.mi_ext_cnt  !=  0)
			return (XLATOR_SUCCESS);

		if (value[0] != '\0') {
			Supported_Arch = 0;
			if (set_supported_arch(value)) {
				errlog(INPUT|ERROR, "Error in SPEC! Can't "
				    "parse Arch line");
				return (XLATOR_NONFATAL);
			}
		} else {
			errlog(INPUT | ERROR, "Error in SPEC! No architectures "
			    "defined in ARCH line");
		}

		errlog(TRACING, "Interface %s supports the following "
		    "architectures: %s\tSupported_Arch=%d", Curfun,
		    value, Supported_Arch);

		break;
	default:
		errlog(INPUT|ERROR, "Error: Unrecognized keyword snuck in!"
		    "\tThis is a programmer error: %s", key);
		return (XLATOR_NONFATAL);
	}

	return (XLATOR_SUCCESS);
}

/*
 * xlator_end_if ()
 *  signal end of spec interface spec
 *     returns: XLATOR_SUCCESS on success
 *		XLATOR_NONFATAL	on error
 */
/*ARGSUSED*/
int
xlator_end_if(const Meta_info M, const char *value)
{
	int retval = XLATOR_NONFATAL;

	seterrline(M.mi_line_number, M.mi_filename, "End", "");
	errlog(TRACING, "xlator_end_if");

	if (Curfun == NULL) {
		errlog(INPUT | ERROR, "Error in SPEC!  End without Begin in "
		    "file \"%s\"", Curfile);
		goto cleanup;
	}

	errlog(TRACING, "Interface=%s", Iface.IF_name);

	if (!Has_Version) {
		errlog(INPUT | WARNING, "Warning:  Interface has no Version!\n"
		    "\tInterface=%s\n\tSPEC File=%s", Iface.IF_name, Curfile);
		retval = XLATOR_SUCCESS;
		goto cleanup;
	}

	if (Iface.IF_version == NULL) { /* not an error... Ignore interface */
		errlog(WARNING|INPUT,
		    "Warning:  Version was not found for "
		    "\"%s\" architecture\n\tInterface=%s",
		    TargetArchStr, Iface.IF_name);

		retval = XLATOR_SUCCESS;
		goto cleanup;
	}

	/* check Iface.IF_type */
	switch (Iface.IF_type) {
	case FUNCTION:
		errlog(VERBOSE, "Interface type = FUNCTION");
		break;
	case DATA:
		errlog(VERBOSE, "Interface type = DATA");
		break;
	case NOTYPE:
		if (IsFilterLib) {
			errlog(WARNING,
			    "Warning in SPEC!: Interface is neither "
			    "DATA nor FUNCTION!!\n\t"
			    "Interface=%s\n\tSPEC File=%s",
			    Iface.IF_name, Curfile);
		}
		break;
	default:
		errlog(ERROR, "Error in spec2map implementation!\n"
		    "\tInterface type is invalid\n"
		    "\tThis should never happen.\n"
		    "\tInterface=%s\tSPEC File=%s", Iface.IF_name, Curfile);
		goto cleanup;
	}

	if (Version_Arch & (~Supported_Arch)) {
		errlog(ERROR, "Error in SPEC!  Architectures in Version "
		    "line must be a subset of Architectures in Arch line\n"
		    "\tInterface=%s\n\tSPEC File=%s", Iface.IF_name, Curfile);
		goto cleanup;
	}

	if (TargetArchToken & Supported_Arch)
		(void) add_by_name(Iface.IF_version, &Iface);

	retval = XLATOR_SUCCESS;

cleanup:

	/* cleanup */
	Iface.IF_name = NULL;

	free(Iface.IF_version);
	Iface.IF_version = NULL;

	free(Iface.IF_class);
	Iface.IF_class = NULL;

	free(Curfun);
	Curfun = NULL;

	Supported_Arch = XLATOR_ALLARCH;
	return (retval);
}

/*
 * xlator_endfile()
 *   signal end of spec file
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_NONFATAL	on error
 */
int
xlator_endfile(void)
{

	errlog(TRACING, "xlator_endfile");

	Curfile = NULL;

	return (XLATOR_SUCCESS);
}

/*
 * xlator_endlib()
 *   signal end of library
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_NONFATAL	on error
 */
int
xlator_endlib(void)
{
	FILE *mapfp;
	int retval = XLATOR_SUCCESS;

	errlog(TRACING, "xlator_endlib");

	/* Pretend to print mapfile */
	if (Verbosity >= TRACING) {
		print_all_buckets();
	}

	/* Everything read, now organize it! */
	sort_buckets();
	add_local();

	/* Create Output */
	mapfp = fopen(OutputFile, "w");
	if (mapfp == NULL) {
		errlog(ERROR,
		    "Error! Unable to open output file \"%s\"\n\t%s",
		    OutputFile, strerror(errno));
		retval = XLATOR_NONFATAL;
	} else {
		writemapfile(mapfp);
		(void) fclose(mapfp);
	}

	return (retval);
}

/*
 * xlator_end()
 *   signal end of translation
 *    returns:  XLATOR_SUCCESS	on success
 *              XLATOR_NONFATAL	on error
 */
int
xlator_end(void)
{
	errlog(TRACING, "xlator_end");

	/* Destroy the list created by create_lists */
	delete_lists();

	return (XLATOR_SUCCESS);
}

/*
 * getversion()
 * called by xlator_take_kvpair when Version keyword is found
 * parses the Version string and returns the one that matches
 * the current target architecture
 *
 * the pointer returned by this function must be freed later.
 */
static char *
getversion(const char *value)
{
	char *v, *p;
	char arch[ARCHBUFLEN];
	int archlen;

	/* up to ARCHBUFLEN-1 */
	(void) strncpy(arch, TargetArchStr, ARCHBUFLEN-1);
	arch[ARCHBUFLEN-2] = '\0';
	(void) strcat(arch, "=");		/* append an '=' */
	archlen = strlen(arch);

	errlog(VERBOSE, "getversion: value=%s", value);

	if (strchr(value, '=') != NULL) {
		if ((v = strstr(value, arch)) != NULL) {
			p = strdup(v + archlen);
			if (p == NULL) {
				errlog(ERROR,
				    "Error: strdup failure in getversion");
				return (NULL);
			}
			v = p;
			while (!isspace(*v) && *v != '\0')
				++v;
			*v = '\0';
		} else {
			errlog(VERBOSE, "getversion returns: NULL");
			return (NULL);
		}
	} else {
		p = strdup(value);
		if (p == NULL) {
			errlog(ERROR, "Error: strdup failure in getversion");
		}
	}

	if (p != NULL)
		errlog(VERBOSE, "getversion returns: %s", p);
	else
		errlog(VERBOSE, "getversion returns: NULL");

	return (p);
}

/*
 * version_sanity()
 *    for each version info in the Version line
 *    check for its validity.
 *    Set Version_arch to reflect all supported architectures if successful.
 *    returns: VS_OK	OK
 *             VS_INVARCH    Invalid Architecture
 *             VS_INVVERS    Invalid Version String
 *             VS_INVALID    Both Version and Architecture are invalid;
 */
static int
version_sanity(const char *value)
{
	char *p, *v, *a;
	int retval = VS_INVALID;

	if (strchr(value, '=')) {
		/* Form 1:   Version	arch=Version_string */
		v = strdup(value);
		if (v == NULL) {
			errlog(ERROR,
			    "Error checking version: strdup() failure");
			return (retval);
		}

		/* process each arch=version string */
		p = v;
		while ((a = strtok(p, " \t\n"))) {
			if ((retval = arch_version_sanity(a))) {
				break;
			}
			if (set_version_arch(a)) {
				/* set the global Version_arch */
				break;
			}
			p = NULL;
		}
		free(v);
	} else {
		/* Form 2: Version		Version_string */
		if (valid_version(value))
			retval = VS_OK;
	}
	return (retval);
}

/*
 * arch_version_sanity()
 *    checks version lines of the form "arch=version"
 *    av MUST be a string of the form "arch=version" (no spaces)
 *    returns: VS_OK	OK
 *             VS_INVARCH    Invalid Architecture
 *             VS_INVVERS    Invalid Version String
 *             VS_INVALID    Both Versions are invalid;
 */
static int
arch_version_sanity(char *av)
{
	char *p, *v;
	int retval = VS_OK;

	p = strchr(av, '=');
	if (p == NULL) {
		errlog(INPUT|ERROR, "Error: Incorrect format of Version line");
		return (VS_INVALID);
	}

	*p = '\0';	/* stick a '\0' where the '=' was */
	v = p + 1;

	if (valid_arch(av) == 0)
		retval = VS_INVARCH;

	if (valid_version(v) == 0)
		retval += VS_INVVERS;

	*p = '=';	/* restore the '=' */

	return (retval);
}

/*
 * writemapfile()
 *    called by xlator_endlib();
 *    writes out the map file
 */
static void
writemapfile(FILE *mapfp)
{
	bucket_t *l;	/* List of buckets. */
	bucket_t *b;	/* Bucket within list. */
	struct bucketlist *bl;
	table_t *t;
	int i = 0, n = 0;
	char **p;

	errlog(BEGIN, "writemapfile() {");
	for (l = first_list(); l != NULL; l = next_list()) {

		for (b = first_from_list(l); b != NULL; b = next_from_list()) {
			errlog(TRACING, "b_name = %s", b->b_name);
			print_bucket(b); /* Debugging routine. */

			if (!b->b_was_printed) {
				/* Ok, we can print it. */
				b->b_was_printed = 1;
				(void) fprintf(mapfp, "%s {\n", b->b_name);

				if (b->b_weak != 1) {
					char *strtab;

					(void) fprintf(mapfp, "    global:\n");

					strtab = get_stringtable(
						b->b_interface_table, 0);

					if (strtab == NULL) {
						/*
						 * There were no interfaces
						 * in the bucket.
						 * Insert a dummy entry
						 * to avoid a "weak version"
						 */
						(void) fprintf(mapfp,
						    "\t%s;\n", b->b_name);
					}
				} else {
					(void) fprintf(mapfp,
					    "    # Weak version\n");
				}
				/* Print all the interfaces in the bucket. */
				t = b->b_interface_table;
				n = t->used;

				for (i = 0; i <= n; ++i) {
					(void) fprintf(mapfp, "\t%s;\n",
					    get_stringtable(t, i));
				}

				/* Conditionally add ``local: *;''. */
				if (b->b_has_locals) {
					(void) fprintf(mapfp,
					    "    local:\n\t*;\n}");
				} else {
					(void) fprintf(mapfp, "}");
				}
				/* Print name of all parents. */
				for (p = parents_of(b);
				    p !=  NULL && *p != '\0'; ++p) {
					(void) fprintf(mapfp, " %s", *p);
				}
				bl = b->b_uncles;
				while (bl != NULL) {
					(void) fprintf(mapfp, " %s",
					    bl->bl_bucket->b_name);
					bl = bl->bl_next;
				}

				(void) fprintf(mapfp, ";\n\n");
			} else {
				/*
				 * We've printed this one before,
				 * so don't do it again.
				 */
				/*EMPTY*/;
			}
		}
	}
	errlog(END, "}");
}

/*
 * set_version_arch ()
 * input must be a string of the form "arch=version"
 * turns on bits of global Version_Arch that correspond to the "arch"
 * return 0 upon success, EINVAL on failure
 */
static int
set_version_arch(const char *arch)
{
	char	*a, *p;
	int	x;
	int	retval = EINVAL;

	if (arch == NULL)
		return (retval);

	a = strdup(arch);
	if (a == NULL) {
		errlog(ERROR,
		    "Error: Failed to set Version_Arch; strdup() failure");
		return (EINVAL);
	}

	p = strchr(a, '=');
	if (p) {
		*p = '\0';
		x = arch_strtoi(a);
		if (x < 0) {
			errlog(ERROR, "Error: malformed version string");
		} else {
			Version_Arch |= x;
			retval = 0;
		}
	}

	free(a);
	return (retval);
}

/*
 * set_supported_arch ()
 * input must be a string listing the architectures to be supported
 * turns on bits of global Supported_Arch that correspond to the architecture
 * return 0 upon success, EINVAL on failure
 */
static int
set_supported_arch(const char *arch)
{
	char	*a, *p, *tmp;
	int	retval = EINVAL;

	if (arch == NULL || *arch == '\0')
		return (EINVAL);

	tmp = strdup(arch);
	if (tmp == NULL) {
		errlog(ERROR, "Error: Unable to strdup arch for checking");
		return (EINVAL);
	}

	p = tmp;
	while ((a = strtok(p, " ,\t\n"))) {
		int x;
		x = arch_strtoi(a);
		if (x < 0) {
			errlog(INPUT|ERROR,
			    "Error: Invalid architecture: %s", a);
			free(tmp);
			return (EINVAL);
		}
		Supported_Arch |= x;
		retval = 0;
		p = NULL;
	}

	free(tmp);
	return (retval);
}
