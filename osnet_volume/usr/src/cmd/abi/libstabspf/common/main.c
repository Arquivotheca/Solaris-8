/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)main.c	1.1	99/05/14 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stab.h>
#include <apptrace.h>
#include "stabspf_impl.h"
#include "test.h"

int stab_line;
static char *stab_file;

/*
 * collect_as_stabs() - uses yylex to extract the string section of a stab
 * and the rudimentary information required from it.
 * Returns:
 *	STAB_NOMEM	on allocation failure.
 *	STAB_SUCCESS	on successful completion to EOF
 */
static stabsret_t
collect_as_stabs(const char *asname)
{
	stabkey_t key;
	int stab_type;
	char *str;
	size_t strsz;
	char *prep = NULL;
	size_t prepsz = 0;
	char *tmp;
	int stabs_count = 0;
	int good_stabs = 0;
	stabsret_t ret;

	if ((yyin = fopen(asname, "r")) == NULL) {
		(void) fprintf(stderr,
		    "collect_as_stabs(\"%s\") failed\n", asname);
		perror("fopen:");
		return (STAB_FAIL);
	}

	/* All output, if any, should go to stderr. */
	yyout = stderr;


	while ((key = yylex()) != 0) {
		switch (key) {
		case K_STAB:	/* not interesting at this point */
		case K_NUM:
		case K_DESC:
		case K_VAL:
			break;
		case K_STRC:	/* string will be continued */
			/* strip the trailing "\\\\\"," */
			strsz = yyleng - 4;
			if (prep == NULL) {
				if ((prep = malloc(strsz + 1)) == NULL) {
					(void) fclose(yyin);
					return (STAB_NOMEM);
				}
				tmp = prep;
				prepsz = strsz;
			} else {
				if ((prep = realloc(prep,
				    prepsz + strsz + 1)) == NULL) {
					(void) fclose(yyin);
					return (STAB_NOMEM);
				}
				tmp = prep + prepsz;
				prepsz += strsz;
			}
			(void) strncpy(tmp, yytext, strsz);
			/* null terminate */
			prep[prepsz] = '\0';
			break;
		case K_STR:	/* whole or final string */
			/* strip the trailing "\" */
			strsz = yyleng - 2;
			if (prep == NULL) {
				if ((str = malloc(strsz + 1)) == NULL) {
					(void) fclose(yyin);
					return (STAB_NOMEM);
				}
				(void) strncpy(str, yytext, strsz);
				/* null terminate */
				str[strsz] = '\0';
			} else {
				if ((str = realloc(prep,
				    prepsz + strsz + 1)) == NULL) {
					(void) fclose(yyin);
					return (STAB_NOMEM);
				}
				tmp = str + prepsz;
				(void) strncpy(tmp, yytext, strsz);
				strsz += prepsz;

				/* null terminate */
				str[strsz] = '\0';

				/* reset prep */
				prep = NULL;
				prepsz = 0;
			}
			break;

		case K_TYPE:
			if (prep != NULL) {
				continue;
			}
			stab_type = strtol(yytext, (char **)NULL, 0);
			ret = add_stab(stab_type, str);
			switch (ret) {
			case STAB_SUCCESS:
#ifdef VERBOSE
				(void) fprintf(stderr,
				    "GOOD\t.stab[%s:%d]= \"%s\"\n",
				    stab_file, stab_line, str);
#endif
				++good_stabs;
				break;
			case STAB_NA:
#ifdef VERBOSER
				(void) fprintf(stderr,
				    "NA\t.stab[%s:%d]= \"%s\"\n",
				    stab_file, stab_line, str);
#endif
				break;
			case STAB_FAIL:
				(void) fprintf(stderr,
				    "FAIL\t.stab[%s:%d]= \"%s\"\n",
				    stab_file, stab_line, str);
				break;
			case STAB_NOMEM:
				(void) fprintf(stderr,
				    "NOMEM\t.stab[%s:%d]= \"%s\"\n",
				    stab_file, stab_line, str);
				(void) fclose(yyin);
				return (STAB_NOMEM);
			default:
				(void) fprintf(stderr,
				    "{%d}\t.stab[%s:%d]= \"%s\"\n",
				    ret, stab_file, stab_line, str);
				(void) fclose(yyin);
				return (ret);
			}
			free(str);
			str = NULL;
			break;
		case K_END:
			if (prep != NULL) {
				continue;
			}
			++stabs_count;
			break;
		}
	}

	if (keypair_flush_table() != STAB_SUCCESS) {
		(void) fprintf(stderr,
		    "XXX: %s: keypair flush failed\n",
		    stab_file);
	}


	(void) fprintf(stderr, "\nfile:\t%s\n\tstabs_count = %d\n"
	    "\tgood_stabs = %d\n", stab_file, stabs_count, good_stabs);

	(void) fclose(yyin);
	return (STAB_SUCCESS);
}	

static void
usage(char *prog)
{
	(void) fprintf(stderr,
	    "%s: [-e | -s] [-pmn] file1 [[file2] ... ]\n"
	    "\t-e  Read an elf file (default).\n"
	    "\t-s  Read assembler files.\n"
	    "\t-p  Print all known types.\n"
	    "\t-m  Issue Memory usage report.\n"
	    "\t-n  output new stabs.\n",
	    prog);

	exit(1);
}

extern char *optarg;
extern int optind;

int
main(int argc, char *argv[])
{
	int files;
	int use_elf = 1;
	int printall = 0;
	int mreport = 0;
	int newstabs = 0;
	int c;

	while ((c = getopt(argc, argv, "espmn")) != EOF) {
		switch (c) {
		case 'e':
			break;
		case 's':
			use_elf = 0;
			break;
		case 'p':
			++printall;
			break;
		case 'm':
			++mreport;
			break;
		case 'n':
			++newstabs;
			break;
		case '?':
			usage(argv[0]);
			break;
		}
	}

	if (argc <= optind) {
		usage(argv[0]);
	}
	for (files = optind; files < argc; files++) {
		stabsret_t ret;

		stab_line = 1;
		stab_file = argv[files];

		if (use_elf == 0) {
			ret = collect_as_stabs(stab_file);
		} else {
			ret = spf_load_stabs(stab_file);
		}

		switch (ret) {
		case STAB_SUCCESS:
		case STAB_NA:
			break;
		case STAB_FAIL:
			(void) fprintf(stderr, "Parse Error\n");
			return (1);
		case STAB_NOMEM:
			(void) fprintf(stderr, "Out of Memory\n");
			return (1);
		default:
			(void) fprintf(stderr, "Unknown return value\n");
			return (1);
		}
	}

	if (printall) {
		print_all_known();
	}


	if (mreport) {
		memory_report();
	}


	if (newstabs) {
		dump_stabs();
	}

#ifdef CHECK_MEM
	/*
	 * The following calls are not required, but they are called
	 * to check for memory leaks in the library.
	 */
#include "stabspf_impl.h"

	if (keypair_destroy_table() != STAB_SUCCESS) {
		return (1);
	}
	hash_destroy_table();
	ttable_destroy_table();
	stringt_destroy_table();

#endif /* CHECK_MEM */

	/*
	 * This must come out last so flush stdout.
	 */
	(void) fflush(stdout);

	(void) fputs("ALLS WELL THAT ENDS WELL\n", stderr);
	return (0);
}
