/*
 * Copyright (c) 1990 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)main.c	6.12	96/01/17 SMI"

#include <string.h>
#include "once.h"
#include "sgs.h"
#include <locale.h>
#include <limits.h>

static wchar_t  L_INITIAL[] = {'I', 'N', 'I', 'T', 'I', 'A', 'L', 0};

char run_directory[PATH_MAX];
char current_work_directory[PATH_MAX];
extern int find_run_directory(char *, char *, char *, char **, char *);

main(int argc, char **argv)
{
	register int i;
	int c;
	char *path = NULL;
	extern char *optarg;
	extern int  getopt();
	Boolean eoption = 0, woption = 0;

	sargv = argv;
	sargc = argc;
	setlocale(LC_ALL, "");
#ifdef DEBUG
	while ((c = getopt(argc, argv, "dyctvnewVQ:Y:")) != EOF) {
#else
	while ((c = getopt(argc, argv, "ctvnewVQ:Y:")) != EOF) {
#endif
		switch (c) {
#ifdef DEBUG
			case 'd':
				debug++;
				break;
			case 'y':
				yydebug = TRUE;
				break;
#endif
			case 'V':
				(void) fprintf(stderr, "lex: %s %s\n",
				    (const char *)SGU_PKG,
				    (const char *)SGU_REL);
				break;
			case 'Q':
				v_stmp = optarg;
				if (*v_stmp != 'y' && *v_stmp != 'n')
					error(
					"lex: -Q should be followed by [y/n]");
				break;
			case 'Y':
				path = (char *) malloc(strlen(optarg) +
						sizeof ("/nceucform") + 1);
				path = strcpy(path, optarg);
				break;
			case 'c':
				ratfor = FALSE;
				break;
			case 't':
				fout = stdout;
				break;
			case 'v':
				report = 1;
				break;
			case 'n':
				report = 0;
				break;
			case 'w':
			case 'W':
				woption = 1;
				handleeuc = 1;
				widecio = 1;
				break;
			case 'e':
			case 'E':
				eoption = 1;
				handleeuc = 1;
				widecio = 0;
				break;
			default:
				(void) fprintf(stderr,
				"Usage: lex [-ewctvnVY] [-Q(y/n)] [file]\n");
				exit(1);
		}
	}
	if (woption && eoption) {
		error(
		"You may not specify both -w and -e simultaneously.");
	}
	no_input = argc - optind;
	if (no_input) {
		/* XCU4: recognize "-" file operand for stdin */
		if (strcmp(argv[optind], "-") == 0)
			fin = stdin;
		else {
			fin = fopen(argv[optind], "r");
			if (fin == NULL)
				error(
				"Can't open input file -- %s", argv[optind]);
		}
	} else
		fin = stdin;

	/* may be gotten: def, subs, sname, schar, ccl, dchar */
	(void) gch();

	/* may be gotten: name, left, right, nullstr, parent */
	get1core();

	scopy(L_INITIAL, sp);
	sname[0] = sp;
	sp += slength(L_INITIAL) + 1;
	sname[1] = 0;

	/* XCU4: %x exclusive start */
	exclusive[0] = 0;

	if (!handleeuc) {
		/*
		* Set ZCH and ncg to their default values
		* as they may be needed to handle %t directive.
		*/
		ZCH = ncg = NCH; /* ncg behaves as constant in this mode. */
	}

	/* may be disposed of: def, subs, dchar */
	if (yyparse())
		exit(1);	/* error return code */

	if (handleeuc) {
		ncg = ncgidtbl * 2;
		ZCH = ncg;
		if (ncg >= MAXNCG)
			error(
			"Too complex rules -- requires too many char groups.");
		sortcgidtbl();
	}
	repbycgid(); /* Call this even in ASCII compat. mode. */

	/*
	 * maybe get:
	**		tmpstat, foll, positions, gotof, nexts,
	**		nchar, state, atable, sfall, cpackflg
	*/
	free1core();
	get2core();
	ptail();
	mkmatch();
#ifdef DEBUG
	if (debug)
		pccl();
#endif
	sect  = ENDSECTION;
	if (tptr > 0)
		cfoll(tptr-1);
#ifdef DEBUG
	if (debug)
		pfoll();
#endif
	cgoto();
#ifdef DEBUG
	if (debug) {
		(void) printf("Print %d states:\n", stnum + 1);
		for (i = 0; i <= stnum; i++)
			stprt(i);
	}
#endif
	/*
	* may be disposed of:
	**		positions, tmpstat, foll, state, name,
	**		left, right, parent, ccl, schar, sname
	** maybe get:	 verify, advance, stoff
	*/
	free2core();
	get3core();
	layout();
	/*
	 * may be disposed of:
	**		verify, advance, stoff, nexts, nchar,
	**		gotof, atable, ccpackflg, sfall
	*/

#ifdef DEBUG
	free3core();
#endif
	if (path == NULL) {
		current_work_directory[0] = '.';
		current_work_directory[1] = '\0';
		if (find_run_directory(sargv[0],
		    current_work_directory,
		    run_directory,
		    (char **) 0,
		    getenv("PATH")) != 0) {
			(void) fprintf(stderr,
			"Error in finding run directory. Using default %s\n",
			current_work_directory);
			path = current_work_directory;
		} else {
			path = run_directory;
		}
	}

	if (handleeuc) {
		if (ratfor)
			error("Ratfor is not supported by -w or -e option.");
		(void) strcat(path, "/nceucform");
	}
	else
		(void) strcat(path, ratfor ? "/nrform" : "/ncform");

	fother = fopen(path, "r");
	if (fother == NULL)
		error("Lex driver missing, file %s", path);
	while ((i = getc(fother)) != EOF)
		(void) putc((char)i, fout);
	(void) fclose(fother);
	(void) fclose(fout);
	if (report == 1)
		statistics();
	(void) fclose(stdout);
	(void) fclose(stderr);
	exit(0);	/* success return code */
	/* NOTREACHED */
}

get1core()
{
	ccptr =	ccl = (CHR *) myalloc(CCLSIZE, sizeof (*ccl));
	pcptr = pchar = (CHR *) myalloc(pchlen, sizeof (*pchar));
	def = (CHR **) myalloc(DEFSIZE, sizeof (*def));
	subs = (CHR **) myalloc(DEFSIZE, sizeof (*subs));
	dp = dchar = (CHR *) myalloc(DEFCHAR, sizeof (*dchar));
	sname = (CHR **) myalloc(STARTSIZE, sizeof (*sname));
	/* XCU4: exclusive start array */
	exclusive = (int *) myalloc(STARTSIZE, sizeof (*exclusive));
	sp = schar = (CHR *) myalloc(STARTCHAR, sizeof (*schar));
	if (ccl == 0 || def == 0 ||
	    pchar == 0 || subs == 0 || dchar == 0 ||
	    sname == 0 || exclusive == 0 || schar == 0)
		error("Too little core to begin");
}

free1core()
{
	cfree((BYTE *)def, DEFSIZE, sizeof (*def));
	cfree((BYTE *)subs, DEFSIZE, sizeof (*subs));
	cfree((BYTE *)dchar, DEFCHAR, sizeof (*dchar));
}

get2core()
{
	register int i;
	gotof = (int *)myalloc(nstates, sizeof (*gotof));
	nexts = (int *)myalloc(ntrans, sizeof (*nexts));
	nchar = (CHR *)myalloc(ntrans, sizeof (*nchar));
	state = (int **)myalloc(nstates, sizeof (*state));
	atable = (int *)myalloc(nstates, sizeof (*atable));
	sfall = (int *)myalloc(nstates, sizeof (*sfall));
	cpackflg = (Boolean *)myalloc(nstates, sizeof (*cpackflg));
	tmpstat = (CHR *)myalloc(tptr+1, sizeof (*tmpstat));
	foll = (int **)myalloc(tptr+1, sizeof (*foll));
	nxtpos = positions = (int *)myalloc(maxpos, sizeof (*positions));
	if (tmpstat == 0 || foll == 0 || positions == 0 ||
	    gotof == 0 || nexts == 0 || nchar == 0 ||
	    state == 0 || atable == 0 || sfall == 0 || cpackflg == 0)
		error("Too little core for state generation");
	for (i = 0; i <= tptr; i++)
		foll[i] = 0;
}

free2core()
{
	cfree((BYTE *)positions, maxpos, sizeof (*positions));
	cfree((BYTE *)tmpstat, tptr+1, sizeof (*tmpstat));
	cfree((BYTE *)foll, tptr+1, sizeof (*foll));
	cfree((BYTE *)name, treesize, sizeof (*name));
	cfree((BYTE *)left, treesize, sizeof (*left));
	cfree((BYTE *)right, treesize, sizeof (*right));
	cfree((BYTE *)parent, treesize, sizeof (*parent));
	cfree((BYTE *)nullstr, treesize, sizeof (*nullstr));
	cfree((BYTE *)state, nstates, sizeof (*state));
	cfree((BYTE *)sname, STARTSIZE, sizeof (*sname));
	/* XCU4: exclusive start array */
	cfree((BYTE *)exclusive, STARTSIZE, sizeof (*exclusive));
	cfree((BYTE *)schar, STARTCHAR, sizeof (*schar));
	cfree((BYTE *)ccl, CCLSIZE, sizeof (*ccl));
}

get3core()
{
	verify = (int *)myalloc(outsize, sizeof (*verify));
	advance = (int *)myalloc(outsize, sizeof (*advance));
	stoff = (int *)myalloc(stnum+2, sizeof (*stoff));
	if (verify == 0 || advance == 0 || stoff == 0)
		error("Too little core for final packing");
}

#ifdef DEBUG
free3core()
{
	cfree((BYTE *)advance, outsize, sizeof (*advance));
	cfree((BYTE *)verify, outsize, sizeof (*verify));
	cfree((BYTE *)stoff, stnum+1, sizeof (*stoff));
	cfree((BYTE *)gotof, nstates, sizeof (*gotof));
	cfree((BYTE *)nexts, ntrans, sizeof (*nexts));
	cfree((BYTE *)nchar, ntrans, sizeof (*nchar));
	cfree((BYTE *)atable, nstates, sizeof (*atable));
	cfree((BYTE *)sfall, nstates, sizeof (*sfall));
	cfree((BYTE *)cpackflg, nstates, sizeof (*cpackflg));
}
#endif
BYTE *
myalloc(int a, int b)
{
	register BYTE *i;
	i = calloc(a,  b);
	if (i == 0)
		warning("calloc returns a 0");
	return (i);
}
#ifdef DEBUG
buserr()
{
	(void) fflush(errorf);
	(void) fflush(fout);
	(void) fflush(stdout);
	(void) fprintf(errorf, "Bus error\n");
	if (report == 1)
		statistics();
	(void) fflush(errorf);
}

segviol()
{
	(void) fflush(errorf);
	(void) fflush(fout);
	(void) fflush(stdout);
	(void) fprintf(errorf, "Segmentation violation\n");
	if (report == 1)
		statistics();
	(void) fflush(errorf);
}
#endif

#ifdef __cplusplus
void
yyerror(const char *s)
#else
yyerror(s)
char *s;
#endif
{
	(void) fprintf(stderr,
		"\"%s\":line %d: Error: %s\n", sargv[optind], yyline, s);
}
