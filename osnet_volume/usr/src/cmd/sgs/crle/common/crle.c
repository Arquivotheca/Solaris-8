/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)crle.c	1.2	99/11/03 SMI"

#include	<sys/types.h>
#include	<sys/stat.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<locale.h>
#include	<dlfcn.h>
#include	<errno.h>
#include	"_crle.h"
#include	"msg.h"


/*
 * crle(1) entry point and argument processing.
 *
 * Two passes of the arguments are carried out; the first collects any single
 * instance options and establishes defaults that might be appropriate for
 * other arguments:
 *
 *  -64		operate on, or apply, 64-bit objects (default is 32-bit).
 *
 *  -c file	defines the output configuration file.
 *
 *  -f flag	flags for dldump(3x).
 *
 *  -o dir	defines the output directory for any dldump(3x) objects
 *		(if not specified, the configuration files parent directory
 *		is used).
 *
 *  -v		verbose mode.
 *
 * The second pass collects all other options and constructs an internal
 * string table which will be used to create the eventual configuration file.
 *
 *  -a name	add the individual name, with an alternative to the
 *		configuration cache.  No alternative is created via dldump(3x),
 *		it is the users responsiblity to furnish the alternative.
 *
 *  -i name	add the individual name to the configuration cache.  If name
 *		is a directory each shared object within the directory is added
 *		to the cache.
 *
 *  -I name	same as -i, but in addition any ELF objects are dldump(3x)'ed.
 *
 *  -g name	add the group name to the configuration cache.  Each object is
 * 		expanded to determine its dependencies and these are added to
 *		the cache.  If name is a directory each shared object within the
 *		directory and its depedencies are added to the cache.
 *
 *  -G app	same as -g, but in addition any ELF objects are dldump(3x)'ed.
 *
 *  -l dir	library search directory
 *
 *  -s dir	secure search directory
 *
 *  -t type	search directory type (ELF or AOUT).
 */
main(int argc, char ** argv)
{
	Crle_desc	crle = { 0 };
	int		c, error = 0;
	char **		lib;
	struct stat	ostatus, nstatus;

	/*
	 * Establish locale.
	 */
	(void) setlocale(LC_MESSAGES, MSG_ORIG(MSG_STR_EMPTY));
	(void) textdomain(MSG_ORIG(MSG_SUNW_OST_SGS));

	/*
	 * Initialization configuration information.
	 */
	crle.c_name = argv[0];
	crle.c_strbkts = 503;
	crle.c_inobkts = 251;
	crle.c_class = ELFCLASS32;
	crle.c_machine = M_MACH;

	/*
	 * First argument pass.
	 */
	while ((c = getopt(argc, argv, MSG_ORIG(MSG_ARG_OPTIONS))) != -1) {
		switch (c) {

		case '6':			/* operate on 64-bit objects */
			crle.c_class = ELFCLASS64;
#if	defined(sparc)
			crle.c_machine = EM_SPARCV9;
#else	defined(i386)
			crle.c_machine = EM_IA_64;
#endif
			break;

		case 'a':			/* create alternative */
			crle.c_flags |= (CRLE_CREAT | CRLE_ALTER);
			break;

		case 'c':			/* define the config file */
			if (crle.c_confil) {
				(void) fprintf(stderr, MSG_INTL(MSG_ARG_MULT),
				    crle.c_name, MSG_ORIG(MSG_ARG_C));
				error = 1;
			}
			crle.c_confil = optarg;
			break;

		case 'f':			/* dldump(3x) flags */
			if (crle.c_dlflags) {
				(void) fprintf(stderr, MSG_INTL(MSG_ARG_MULT),
				    crle.c_name, MSG_ORIG(MSG_ARG_F));
				error = 1;
			}
			if ((crle.c_dlflags = dlflags(&crle,
			    (const char *)optarg)) == 0)
				error = 1;
			break;

		case 'G':			/* group object */
			crle.c_flags |= (CRLE_DUMP | CRLE_ALTER);
			/* FALLTHROUGH */
		case 'g':
			crle.c_flags |= CRLE_CREAT;
			break;

		case 'I':			/* individual object */
			crle.c_flags |= (CRLE_DUMP | CRLE_ALTER);
			/* FALLTHROUGH */
		case 'i':
			crle.c_flags |= CRLE_CREAT;
			break;

		case 'l':			/* library search path */
			crle.c_flags |= CRLE_CREAT;
			break;

		case 'o':			/* define an object directory */
			if (crle.c_objdir) {
				(void) fprintf(stderr, MSG_INTL(MSG_ARG_MULT),
				    crle.c_name, MSG_ORIG(MSG_ARG_O));
				error = 1;
			}
			crle.c_objdir = optarg;
			break;

		case 's':			/* secure search path */
			crle.c_flags |= CRLE_CREAT;
			break;

		case 't':			/* search path type */
			if ((strcmp((const char *)optarg,
			    MSG_ORIG(MSG_STR_ELF)) != 0) &&
			    (strcmp((const char *)optarg,
			    MSG_ORIG(MSG_STR_AOUT)) != 0)) {
				(void) fprintf(stderr, MSG_INTL(MSG_ARG_TYPE),
				    crle.c_name, optarg);
				error = 1;
			}
			break;

		case 'v':			/* verbose mode */
			crle.c_flags |= CRLE_VERBOSE;
			break;

		default:
			error = 2;
		}
	}

	if (optind != argc)
		error = 2;

	/*
	 * Now that we've generated as many file/directory processing errors
	 * as we can, return if any fatal error conditions occurred.
	 */
	if (error) {
		if (error == 2)
			(void) fprintf(stderr, MSG_INTL(MSG_ARG_USAGE),
			    crle.c_name);
		return (1);
	}

	/*
	 * Apply any additional defaults.
	 */
	if (crle.c_confil == 0) {
		if (crle.c_class == ELFCLASS32)
			crle.c_confil = (char *)MSG_ORIG(MSG_PTH_CONFIG);
		else
			crle.c_confil = (char *)MSG_ORIG(MSG_PTH_CONFIG_64);
	}

	if (crle.c_dlflags == 0)
		crle.c_dlflags = RTLD_REL_RELATIVE;

	crle.c_audit = (char *)MSG_ORIG(MSG_ENV_LD_AUDIT);

	/*
	 * If we're not creating a configuration file simply dump the
	 * existing one.
	 */
	if (!(crle.c_flags & CRLE_CREAT))
		return (printconfig(&crle));

	if (crle.c_flags & CRLE_VERBOSE)
		(void) printf(MSG_INTL(MSG_DIA_CONFILE), crle.c_confil);

	/*
	 * Make sure the configuration file is accessible.  Stat the file to
	 * determine its dev number - this is used to determine whether the
	 * temporary configuration file we're about to build can be renamed or
	 * must be copied to its final destination.
	 */
	if (access(crle.c_confil, (R_OK | W_OK)) == 0) {
		crle.c_flags |= CRLE_EXISTS;

		if (stat(crle.c_confil, &ostatus) != 0) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
			    crle.c_name, crle.c_confil, strerror(err));
			return (1);
		}
	} else if (errno != ENOENT) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_ACCESS), crle.c_name,
		    crle.c_confil, strerror(err));
		return (1);
	} else {
		int	fd;

		/*
		 * Try opening the file now, if it works delete it, there may
		 * be a lot of processing ahead of us, so we'll come back and
		 * create the real thing later.
		 */
		if ((fd = open(crle.c_confil, (O_RDWR | O_CREAT | O_TRUNC),
		    0666)) == -1) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
			    crle.c_name, crle.c_confil, strerror(err));
			return (1);
		}
		if (fstat(fd, &ostatus) != 0) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
			    crle.c_name, crle.c_confil, strerror(err));
			return (1);
		}
		(void) close(fd);
		(void) unlink(crle.c_confil);
	}


	/*
	 * If an object directory is required to hold dldump(3x) output assign a
	 * default if necessary and insure we're able to write there.
	 */
	if (crle.c_flags & CRLE_ALTER) {
		if (crle.c_objdir == 0) {
			char *	str;

			/*
			 * Use the configuration files directory.
			 */
			if ((str = strrchr(crle.c_confil, '/')) == NULL)
				crle.c_objdir = (char *)MSG_ORIG(MSG_DIR_DOT);
			else {
				int	len = str - crle.c_confil;

				if ((crle.c_objdir = malloc(len + 1)) == 0) {
					int err = errno;
					(void) fprintf(stderr,
					    MSG_INTL(MSG_SYS_MALLOC),
					    crle.c_name, strerror(err));
					return (1);
				}
				(void) strncpy(crle.c_objdir,
				    crle.c_confil, len);
				crle.c_objdir[len] = '\0';
			}
		}

		if (crle.c_flags & CRLE_VERBOSE)
			(void) printf(MSG_INTL(MSG_DIA_OBJDIR), crle.c_objdir);

		/*
		 * If we're going to dldump(3x) image ourself make sure we can
		 * access the directory.
		 */
		if ((crle.c_flags & CRLE_DUMP) &&
		    access(crle.c_objdir, (R_OK | W_OK)) != 0) {
			int err = errno;
			(void) fprintf(stderr, MSG_INTL(MSG_SYS_ACCESS),
			    crle.c_name, crle.c_objdir, strerror(err));
			return (1);
		}
	}

	/*
	 * Create a temporary file name in which to build the configuration
	 * information.
	 */
	if ((crle.c_tempname = tempnam(MSG_ORIG(MSG_TMP_DIR),
	    MSG_ORIG(MSG_TMP_PFX))) == NULL) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_TEMPNAME),
		    crle.c_name, strerror(err));
		return (1);
	}
	if ((crle.c_tempfd = open(crle.c_tempname, (O_RDWR | O_CREAT),
	    0666)) == -1) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
		    crle.c_name, crle.c_tempname, strerror(err));
		return (1);
	}
	if (stat(crle.c_tempname, &nstatus) != 0) {
		int err = errno;
		(void) fprintf(stderr, MSG_INTL(MSG_SYS_OPEN),
		    crle.c_name, crle.c_tempname, strerror(err));
		return (1);
	}
	if (ostatus.st_dev != nstatus.st_dev)
		crle.c_flags |= CRLE_DIFFDEV;


	(void) elf_version(EV_CURRENT);

	/*
	 * Second pass.
	 */
	error = 0;
	optind = 1;
	while ((c = getopt(argc, argv, MSG_ORIG(MSG_ARG_OPTIONS))) != -1) {
		const char *	str;
		int		flag = 0;

		switch (c) {

		case '6':
			break;

		case 'a':			/* alternative required */
			if (inspect(&crle, (const char *)optarg,
			    CRLE_ALTER) != 0)
				error = 1;
			break;

		case 'c':
			/* FALLTHROUGH */
		case 'f':
			break;

		case 'G':			/* group object */
			flag = (CRLE_DUMP | CRLE_ALTER);
			/* FALLTHROUGH */
		case 'g':
			if (inspect(&crle, (const char *)optarg,
			    (flag | CRLE_GROUP)) != 0)
				error = 1;
			break;

		case 'I':			/* individual object */
			flag = (CRLE_DUMP | CRLE_ALTER);
			/* FALLTHROUGH */
		case 'i':
			if (inspect(&crle, (const char *)optarg, flag) != 0)
				error = 1;
			break;

		case 'l':			/* library search path */
			if (crle.c_flags & CRLE_AOUT) {
				str = MSG_ORIG(MSG_STR_AOUT);
				lib = &crle.c_adlibpath;
			} else {
				str = MSG_ORIG(MSG_STR_ELF);
				lib = &crle.c_edlibpath;
			}
			if (addlib(&crle, lib, (const char *)optarg) != 0)
				error = 1;
			else if (crle.c_flags & CRLE_VERBOSE)
				(void) printf(MSG_INTL(MSG_DIA_DLIBPTH),
				    str, (const char *)optarg);
			break;

		case 'o':
			break;

		case 's':			/* secure search path */
			if (crle.c_flags & CRLE_AOUT) {
				str = MSG_ORIG(MSG_STR_AOUT);
				lib = &crle.c_aslibpath;
			} else {
				str = MSG_ORIG(MSG_STR_ELF);
				lib = &crle.c_eslibpath;
			}
			if (addlib(&crle, lib, (const char *)optarg) != 0)
				error = 1;
			else if (crle.c_flags & CRLE_VERBOSE)
				(void) printf(MSG_INTL(MSG_DIA_SLIBPTH),
				    str, (const char *)optarg);
			break;

		case 't':			/* search path type */
			if (strcmp((const char *)optarg,
			    MSG_ORIG(MSG_STR_ELF)) == 0)
				crle.c_flags &= ~CRLE_AOUT;
			else
				crle.c_flags |= CRLE_AOUT;
			break;

		case 'v':
			break;
		}
	}

	/*
	 * Now that we've generated as many file/directory processing errors
	 * as we can, return if any fatal error conditions occurred.
	 */
	if (error) {
		(void) unlink(crle.c_tempname);
		return (1);
	}

	/*
	 * Create a temporary configuration file.
	 */
	if (genconfig(&crle) != 0) {
		(void) unlink(crle.c_tempname);
		return (1);
	}

	/*
	 * If dldump(3x) images are required spawn a process to create them.
	 */
	if (crle.c_flags & CRLE_DUMP) {
		if (dump(&crle) != 0) {
			(void) unlink(crle.c_tempname);
			return (1);
		}
	}

	/*
	 * Copy the finished temporary configuration file to its final home.
	 */
	if (updateconfig(&crle) != 0)
		return (1);

	return (0);
}
