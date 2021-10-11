/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1998 by Sun Microsystems, Inc.
 */
#pragma ident	"@(#)main.c	6.21	98/10/09 SMI"

#include "conv.h"
#include "mcs.h"
#include "extern.h"
#define	OPTUNIT	100

static size_t optcnt = 0;
static size_t optbufsz = OPTUNIT;

/*
 * Function prototypes.
 */
static void usage(int);
static void sigexit(int);
static int setup_sectname(char *);
static int who_am_i(char *);
static void check_swap();
static void queue(int, char *);

void
main(int argc, char ** argv, char ** envp)
{
	const char *opt = "Da:cdn:pV?";
	const char *strip_opt = "DlxV?";
	int error_count = 0;
	int num_sect = 0;
	int errflag = 0;
	int c, i;
	int Dflag = 0;
	Cmd_Info *cmd_info;
	int my_prog = 0;
	/*
	 * section names to be stripped by
	 * the strip command.
	 */
	const char *sec_strip[] = {
		".debug",
		".line",
		".stab",
		".stab.excl",
		".stab.index",
		".stab.sbfocus",
		".stab.sbfocusstr",
		".stab.exclstr",
		".stabstr",
		".stab.indexstr",
		NULL
	};




	/*
	 * Save the program name.
	 * This could be mcs or strip.
	 */
	prog = argv[0];

	/*
	 * decide who i am
	 */
	if ((my_prog = who_am_i(prog)) == 0)
		exit(1);
	if (my_prog == STRIP)
		opt = strip_opt;

	/*
	 * Check for a binary that better fits this architecture.
	 */
	conv_check_native(argv, envp,
		((my_prog == STRIP) ? "strip" : 0));


	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);


	for (i = 0; signum[i]; i++)
		if (signal(signum[i], SIG_IGN) != SIG_IGN)
			(void) signal(signum[i], sigexit);

	if ((Action = (struct action *)
	    malloc(optbufsz * sizeof (struct action))) == NULL) {
		error_message(MALLOC_ERROR, PLAIN_ERROR, (char *)0, prog);
		exit(FAILURE);
	}

	/*
	 * Allocate command info structure
	 */
	cmd_info = (Cmd_Info *) calloc(1, sizeof (Cmd_Info));
	if (cmd_info == NULL) {
		error_message(MALLOC_ERROR, PLAIN_ERROR, (char *)0, prog);
		exit(FAILURE);
	}
	if (my_prog == STRIP)
		cmd_info->flags |= I_AM_STRIP;

	while ((c = getopt(argc, argv, (char *)opt)) != EOF) {
		switch (c) {
		case 'D':
			optcnt++;
			Dflag++;
			break;
		case 'a':
			optcnt++;
			queue(ACT_APPEND, optarg);
			cmd_info->flags |= MIGHT_CHG;
			cmd_info->flags |= aFLAG;
			cmd_info->str_size += strlen(optarg) + 1;
			break;
		case 'c':
			optcnt++;
			queue(ACT_COMPRESS, NULL);
			cmd_info->flags |= MIGHT_CHG;
			cmd_info->flags |= cFLAG;
			break;
		case 'd':
			optcnt++;
			if (CHK_OPT(cmd_info, dFLAG) == 0)
				queue(ACT_DELETE, NULL);
			cmd_info->flags |= MIGHT_CHG;
			cmd_info->flags |= dFLAG;
			break;
		case 'n':
			(void) setup_sectname(optarg);
			num_sect++;
			break;
		case 'l':
			optcnt++;
			cmd_info->flags |= lFLAG;
			break;
		case 'p':
			optcnt++;
			queue(ACT_PRINT, NULL);
			cmd_info->flags |= pFLAG;
			break;
		case 'x':
			optcnt++;
			cmd_info->flags |= xFLAG;
			break;
		case 'V':
			cmd_info->flags |= VFLAG;
			(void) fprintf(stderr, "%s: %s %s\n", prog,
			    (const char *)SGU_PKG, (const char *)SGU_REL);
			break;
		case '?':
			errflag++;
			break;
		default:
			break;
		}
	}

	if (errflag) {
		usage(my_prog);
		exit(FAILURE);
	}

	if (Dflag)
		check_swap();

	/*
	 * strip command may not take any options.
	 */
	if (my_prog != STRIP) {
		if (argc == optind &&
		    (CHK_OPT(cmd_info, MIGHT_CHG) || CHK_OPT(cmd_info, pFLAG) ||
		    argc == 1))
			usage(my_prog);
		else if (!CHK_OPT(cmd_info, MIGHT_CHG) &&
		    !CHK_OPT(cmd_info, pFLAG) && !CHK_OPT(cmd_info, VFLAG))
			usage(my_prog);
	}

	/*
	 * This version only allows multiple section names
	 * only for -d option.
	 */
	if ((num_sect >= 2) && (CHK_OPT(cmd_info, pFLAG) ||
	    CHK_OPT(cmd_info, aFLAG) ||
	    CHK_OPT(cmd_info, cFLAG))) {
		error_message(USAGE_ERROR, PLAIN_ERROR, (char *)0,  prog);
		exit(1);
	}

	/*
	 * If no -n was specified,
	 * set the default, ".comment".
	 * This is for mcs only.
	 */
	if (num_sect == 0 && my_prog == MCS) {
		(void) setup_sectname(".comment");
	}

	/*
	 * If I am strip command, then add needed
	 * section names.
	 */
	if (my_prog == STRIP) {
		int i = 0;
		if (CHK_OPT(cmd_info, lFLAG) != 0) {
			(void) setup_sectname(".line");
		} else {
			while (sec_strip[i] != NULL) {
				(void) setup_sectname((char *)sec_strip[i]);
				i++;
			}
		}
		if (CHK_OPT(cmd_info, dFLAG) == 0) {
			queue(ACT_DELETE, NULL);
			cmd_info->flags |= MIGHT_CHG;
			cmd_info->flags |= dFLAG;
		}
	}

	(void) elf_version(EV_NONE);
	if (elf_version(EV_CURRENT) == EV_NONE) {
		error_message(ELFVER_ERROR, LIBelf_ERROR, elf_errmsg(-1), prog);
		exit(FAILURE);
	}

	if (CHK_OPT(cmd_info, pFLAG) || CHK_OPT(cmd_info, MIGHT_CHG)) {
		for (; optind < argc; optind++) {
			error_count = error_count +
			    (each_file(argv[optind], cmd_info));
		}
	}

	if (Dflag)
		check_swap();
	mcs_exit(error_count);
	/*NOTREACHED*/
}

/*
 * Supplementary functions
 */
static void
queue(int activity, char *string)
{
	if (optcnt > optbufsz) {
		optbufsz = optbufsz * 2;
		if ((Action = (struct action *)
		    realloc((struct action *) Action,
		    optbufsz * sizeof (struct action))) == NULL) {
			error_message(MALLOC_ERROR,
			PLAIN_ERROR, (char *)0, prog);
			mcs_exit(FAILURE);
		}
	}
	Action[actmax].a_action = activity;
	Action[actmax].a_cnt = 0;
	Action[actmax].a_string = string;
	actmax++;
}

/*ARGSUSED0*/
static void
sigexit(int i)
{
	(void) unlink(artmpfile);
	(void) unlink(elftmpfile);
	exit(100);
}

static void
usage(int me)
{
	if (me == MCS)
		(void) fprintf(stderr, gettext(
		"usage: %s -Vcdp -a 'string' [-n 'name'] files...\n"),
		prog);
	else
		(void) fprintf(stderr, gettext(
		"usage: %s [-l -x -V] files...\n"),
		prog);
	mcs_exit(FAILURE);
}

void
mcs_exit(int val)
{
	(void) unlink(artmpfile);
	(void) unlink(elftmpfile);
	exit(val);
}

/*
 * Insert the section name 'name' into the
 * section list.
 */
static int
setup_sectname(char *name)
{
	S_Name *new;

	/*
	 * Check if the name is already specified or not.
	 */
	if (sectcmp(name) == 0)
		return (0);

	/*
	 * Allocate one
	 */
	new = (S_Name *)malloc(sizeof (S_Name));
	if (new == NULL) {
		error_message(MALLOC_ERROR, PLAIN_ERROR, (char *)0, prog);
		exit(FAILURE);
	}
	new->name = strdup(name);
	if (new->name == NULL) {
		error_message(MALLOC_ERROR, PLAIN_ERROR, (char *)0, prog);
		exit(FAILURE);
	}
	new->next = NULL;

	/*
	 * Put this one in the list
	 */
	new->next = sect_head;
	sect_head = new;

	return (0);
}

/*
 * Check if the 'name' exists in the section list.
 *
 * If found
 *	return 0;
 * else
 *	return 1
 */
int
sectcmp(char *name)
{
	/*
	 * Check if the name is already specified or not.
	 */
	if (sect_head != NULL) {
		S_Name *p1 = sect_head;
		while (p1 != NULL) {
			if (strcmp(p1->name, name) == 0) {
				return (0);	/* silently ignore */
			}
			p1 = p1->next;
		}
	}
	return (1);
}

/*
 * default program name is mcs.
 */
static int
who_am_i(char *s)
{
	char *p;
	char *l;

	l = p = s;
	while (*p != 0) {
		if (*p == '/')
			l = p;
		p++;
	}

	if (*l == '/')
		++l;

	if (strcmp(l, "mcs") == 0)
		return (MCS);
	else if (strcmp(l, "strip") == 0)
		return (STRIP);
	return (0);
}

static void
check_swap()
{
	(void) system("/usr/sbin/swap -s");
}
