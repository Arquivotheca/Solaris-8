/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)main.c	1.6	97/07/28 SMI"	/* SVr4.0 k18.2   */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "symtab.h"
#include "kbd.h"

extern int optind, opterr;
extern char *optarg;
extern int inamap;	/* TRUE when in a map */
extern int linnum;	/* line number */

int nerrors = 0;	/* number of calls to yyerror */
int optreach = 0;	/* check for reachability */
int optR = 0;		/* print unreachables as themselves, not octal */
int optt = 0;		/* table summary */
int optv = 0;		/* verification only */
unsigned char oneone[256];	/* one-one mapping table */
int oneflag = 0;

char *prog;
FILE *lexfp;	/* file pointer for lexical analyzer */

extern void	sym_init(void);
extern int	yyparse(void);
extern void	output(void);

void
main(int argc, char **argv)
{
	char *outfile;
	int c;
	int fd;

	extern struct node *root;
	extern int numnode;

	opterr = optt = optv = 0;
	prog = *argv;

	sym_init();
	lexfp = stdin;	/* default to compiling standard-in */
	outfile = NULL;
	while ((c = getopt(argc, argv, "Rrtvo:")) != EOF) {
		switch (c) {
			case 'R': optR = 1; /* fall through... */
			case 'r': optreach = 1; break;
#if 0	/* optt removed */
			case 't': optt = 1; break;
#endif
			case 'v': optv = 1; break;
			case 'o': outfile = optarg;
				break;
			default:
			case '?':
				fprintf(stderr, gettxt("kbdcomp:19",
"Usage: %s [-vrR] [-o outfile] [infile]\n"),
					prog);
				exit(1);
		}
	}
	if (outfile && optv) {
		fprintf(stderr, gettxt("kbdcomp:20",
			"Option -o cannot be used with -v.\n"));
		exit(1);
	}
	if (! outfile) {
		if (optv)
			outfile = "/dev/null";
		else {
			outfile = "kbd.out";
			fprintf(stderr, gettxt("kbdcomp:21",
				"Output file is \"%s\".\n"), outfile);
		}
	}
	close(1);
	if (! optv) {
		if ((fd = open(outfile, O_CREAT|O_TRUNC|O_WRONLY, 0644)) < 0) {
			fprintf(stderr, gettxt("kbdcomp:22",
				"Can't create output file \"%s\"\n"), outfile);
			exit(1);
		}
		if (fd != 1) {
			fprintf(stderr, gettxt("kbdcomp:23",
"Internal error: unexpected file descriptor (%d).\n"),
				fd);
			exit(1);
		}
	}
	if (optind < argc) {
		if (!(lexfp = fopen(argv[optind], "r"))) {
			fprintf(stderr, gettxt("kbdcomp:24",
				"Can't open %s for reading.\n"), argv[optind]);
			exit(1);
		}
	}
	yyparse();	/* compile it */
	/* s_dump(); dump symbol table */
	if (nerrors == 0)
		output();	/* output maps, etc. */
	else {
		if (inamap)
			fprintf(stderr, gettxt("kbdcomp:25",
				"Map not terminated (?)\n"));
		fprintf(stderr, gettxt("kbdcomp:26",
			"Errors in input; output empty.\n"));
		exit(1);
	}
	exit(0);
}

void
yyerror(char *s)
{
	fprintf(stderr, gettxt("kbdcomp:27", "%s on line %d.\n"), s, linnum);
	++nerrors;
}
