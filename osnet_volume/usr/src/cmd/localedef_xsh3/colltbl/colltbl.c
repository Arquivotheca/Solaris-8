/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)colltbl.c	1.5	92/07/14 SMI"	/* SVr4.0 1.1	*/
#include <stdio.h>
#include <locale.h>
#include "colltbl.h"
#define	MAXPATH		32

/* Global Variables */
int	Status = 0, Lineno = 1;
int	regexp_flag;
int	keepflag = 0;	/* hidden option to keep the generated .c file */
char	*keepfile = NULL;
char	*Cmd, *Infile;
char	codeset[50];

main(argc, argv)
int	argc;
char	**argv;
{
	extern int	optind;
	extern char	*optarg;
	int		c;

	setlocale(LC_ALL, "");

	/*  Get name of command  */
	Cmd = argv[0];

	/*  Get command line options  */
	while ((c = getopt(argc, argv, "rk:")) != EOF) {
		switch (c) {
		case 'k':
			keepflag = 1;
			keepfile = optarg;
			break;
		case 'r':
#ifdef REGEXP
			regexp_flag++;
			break;
#endif
		case '?':
			usage();
			break;
		}
	}

	/*  Get input file argument  */
	switch (argc - optind) {
	case 0:
		Infile = "stdin";
		break;
	case 1:
		if (strcmp(argv[optind], "-") == 0)
			Infile = "stdin";
		else if (freopen((Infile = argv[optind]), "r", stdin) == NULL) {
			error(BAD_OPEN, Infile);
			exit(-1);
		}
		break;
	default:
		usage();
	}

	init();
	/*  Run parser  */
	yyparse();

	genlib(codeset);

	exit(Status);
}
