/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)tbltst.c	1.6	97/03/27 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <dd_impl.h>

#ifndef PERF_DEBUG
#include <sys/time.h>

static struct timeval ts_start, ts_now, ts_elapsed, ts_previous = { 0 };

enum { TS_NOELAPSED, TS_ELAPSED };

#define	timestamp(message, variable, totalflag) {			\
		(void) gettimeofday(&ts_now, "");			\
		if (ts_previous.tv_sec == 0) {				\
			/* first time */				\
			ts_start = ts_now;				\
			ts_elapsed = ts_now;				\
		} else {						\
			ts_elapsed.tv_sec =				\
			    (ts_now.tv_sec - ts_previous.tv_sec);	\
			ts_elapsed.tv_usec =				\
			    (ts_now.tv_usec - ts_previous.tv_usec);	\
		}							\
		if (ts_elapsed.tv_usec < 0) {				\
			ts_elapsed.tv_sec -= 1;				\
			ts_elapsed.tv_usec += 1000000L;			\
		} else if (ts_elapsed.tv_usec > 999999L) {		\
			ts_elapsed.tv_sec += 1;				\
			ts_elapsed.tv_usec -= 1000000L;			\
		}							\
		(void) fprintf(stderr, 				\
		    "%9.1d.%03d %.80s %.80s\n", ts_elapsed.tv_sec,	\
		    (ts_elapsed.tv_usec/1000L),				\
		    message ? message : "(null)", variable);		\
		if ((totalflag) == TS_ELAPSED) {			\
			ts_previous = ts_start;				\
			ts_elapsed.tv_sec =				\
			    (ts_now.tv_sec - ts_previous.tv_sec);	\
			ts_elapsed.tv_usec =				\
			    (ts_now.tv_usec - ts_previous.tv_usec);	\
			if (ts_elapsed.tv_usec < 0) {			\
				ts_elapsed.tv_sec -= 1;			\
				ts_elapsed.tv_usec += 1000000L;		\
			} else if (ts_elapsed.tv_usec > 999999L) {	\
				ts_elapsed.tv_sec += 1;			\
				ts_elapsed.tv_usec -= 1000000L;		\
			}						\
			(void) fprintf(stderr, 				\
			    "%9.1d.%03d Total Elapsed Time\n",		\
			    ts_elapsed.tv_sec,				\
			    (ts_elapsed.tv_usec/1000L));		\
		}							\
		ts_previous = ts_now;					\
		(void) fflush(stderr);					\
	}
#endif /* ! PERF_DEBUG */

#define	Usage "usage: %s -t table_index [-p -o file -d domain \
-n name\n\t -N nameservice -r repeat_cnt] list|add|modify|remove \
\n\t match_args replace_args\n"

main(int argc, char *argv[])
{

	int i;
	int status;
	Tbl_error *tbl_err;
	Tbl tbl = { NULL, 0 };
	int rows;
	extern char *optarg;
	extern int optind;
	int c;
	int ns = TBL_NS_DFLT;
	int dflg = 0;
	char *domain = NULL;
	int nflg = 0;
	char *name = NULL;
	int oflg = 0;
	char *ofile = NULL;
	int pflg = 0;
	int rflg = 0;
	int tflg = 0;
	int table = -1;
	char *function;
	int (*fn)(uint_t, int, char *, char *, Tbl_error **, ...);
	struct tbl_trans_data *ttp;
	FILE *ofp;
	char *args[TBL_MAX_ARGS];
	int nargs;
	Row *rp;

	while ((c = getopt(argc, argv, "N:d:n:o:pr:t:")) != EOF)
		switch (c) {
		case 'N':
			ns = atoi(optarg);
			break;
		case 'd':
			++dflg;
			domain = optarg;
			break;
		case 'n':
			++nflg;
			name = optarg;
			break;
		case 'o':
			++oflg;
			ofile = optarg;
			break;
		case 'p':
			++pflg;
			break;
		case 'r':
			rflg = atoi(optarg);
			break;
		case 't':
			++tflg;
			table = atoi(optarg);
			break;
		case '?':
			(void) fprintf(stderr, Usage, argv[0]);
			exit(2);
		}

	if (!tflg) {
		(void) fprintf(stderr, Usage, argv[0]);
		exit(2);
	}

	if (oflg) {
		if ((ofp = freopen(ofile, "w", stdout)) == NULL) {
			(void) fprintf(stderr, "Unable to open %s\n", ofile);
			exit(1);
		}
	}

	ttp = _ttd[table];

	function = argv[optind++];
	if (function == NULL) {
		(void) fprintf(stderr, Usage, argv[0]);
		exit(2);
	}
	for (i = 0; i < TBL_MAX_ARGS; ++i)
		args[i] = NULL;
	i = 0;
	while (argv[optind] != NULL)
		args[i++] = argv[optind++];

do {
	if (!pflg || oflg)
		timestamp("Starting", function, TS_NOELAPSED);
	if (strcmp(function, "remove") == 0 || strcmp(function, "add") == 0 ||
	    strcmp(function, "modify") == 0) {
		if (strcmp(function, "remove") == 0) {
			fn = rm_tbl_entry;
			nargs = ttp->search_args;
		} else if (strcmp(function, "add") == 0) {
			fn = add_tbl_entry;
			nargs = ttp->args;
		} else {
			fn = mod_tbl_entry;
			nargs = ttp->args + ttp->search_args;
		}
		switch (nargs) {
		case 1:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0]);
			break;
		case 2:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1]);
			break;
		case 3:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2]);
			break;
		case 4:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3]);
			break;
		case 5:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4]);
			break;
		case 6:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5]);
			break;
		case 7:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6]);
			break;
		case 8:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7]);
			break;
		case 9:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8]);
			break;
		case 10:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9]);
			break;
		case 11:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10]);
			break;
		case 12:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10], args[11]);
			break;
		case 13:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10], args[11], args[12]);
			break;
		case 14:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10], args[11], args[12],
			    args[13]);
			break;
		case 15:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10], args[11], args[12],
			    args[13], args[14]);
			break;
		case 16:
			status = (*fn)(table, ns, name, domain,
			    &tbl_err, args[0], args[1], args[2], args[3],
			    args[4], args[5], args[6], args[7], args[8],
			    args[9], args[10], args[11], args[12],
			    args[13], args[14], args[15]);
			break;
		}
	} else if (strcmp(function, "list") == 0) {
		switch (ttp->search_args) {
		case 1:
			status = list_tbl(table, ns, name, domain,
			    &tbl_err, &tbl, args[0]);
			break;
		case 2:
			status = list_tbl(table, ns, name, domain,
			    &tbl_err, &tbl, args[0], args[1]);
			break;
		case 3:
			status = list_tbl(table, ns, name, domain,
			    &tbl_err, &tbl, args[0], args[1], args[2]);
			break;
		case 4:
			status = list_tbl(table, ns, name, domain,
			    &tbl_err, &tbl, args[0], args[1], args[2], args[3]);
		case 5:
			status = list_tbl(table, ns, name, domain,
			    &tbl_err, &tbl, args[0], args[1], args[2], args[3],
			    &args[4]);
		}
	} else
		(void) fprintf(stderr, "Illegal function: %s\n", function);

	if (!pflg || oflg)
		timestamp("Completed", function, TS_NOELAPSED);
	if (status != TBL_SUCCESS) {
		(void) fprintf(stderr, "%s: %s\n", function, tbl_err->msg);
		exit(1);
	}

	if (strcmp(function, "list") == 0 && (pflg || oflg)) {
		(void) fprintf(stderr, "Number of rows = %d\n",
		    tbl.rows);
		for (rows = 0; rows < tbl.rows; ++rows) {
			for (i = 0; i < ttp->args; ++i) {
				if (i != 0)
					(void) putchar(ttp->column_sep[0]);
				if (tbl.ra[rows]->ca[i] != NULL) {
					if ((ttp->comment_sep != NULL) &&
					    (i == ttp->
					    fmts[_tbl_ns(&tbl_err)].
					    comment_col))
						(void) printf("%s",
							ttp->comment_sep);

					(void) printf("%s",
					    tbl.ra[rows]->ca[i]);
				}
			}
			(void) putchar('\n');
		}
		if (!pflg || oflg)
			timestamp("Retrieved all data", "", TS_ELAPSED);
	}

} while (--rflg > 0);

	exit(0);
}
