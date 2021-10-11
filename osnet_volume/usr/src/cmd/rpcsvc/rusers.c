#pragma ident	"@(#)rusers.c	1.10	97/01/23 SMI"

/*
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netconfig.h>
#include <netdir.h>
#include <rpc/rpc.h>
#include <rpcsvc/rusers.h>
#include <string.h>
#include <limits.h>

#define	NMAX	12		/* These are used as field width specifiers */
#define	LMAX	8		/* when printing.			    */
#define	HMAX	16		/* "Logged in" host name. */

#define	MACHINELEN 16		/* length of machine name printed out */
#define	NUMENTRIES 256
#define	min(a, b) ((a) < (b) ? (a) : (b))

struct entry {
	int cnt;
	int idle;		/* set to INT_MAX if not present */
	char *machine;
	utmp_array users;
} entry[NUMENTRIES];

int curentry;
int hflag;			/* host: sort by machine name */
int iflag;			/* idle: sort by idle time */
int uflag;			/* users: sort by number of users */
int lflag;			/* print out long form */
int aflag;			/* all: list all machines */
int dflag;			/* debug: list only first n machines */
int debug;
int debugcnt;
int vers;
char *nettype;

int hcompare(), icompare(), ucompare();
int print_info(), print_info_3(), collectnames(), collectnames_3();
void singlehost(), printnames(), putline_2(), putline_3(), prttime(), usage();


main(argc, argv)
	char **argv;
{
	int single;
	struct utmpidlearr utmpidlearr;
	utmp_array	utmp_array_res;

	single = 0;
	while (argc > 1) {
		if (argv[1][0] != '-') {
			single++;
			singlehost(argv[1]);
		} else {
			switch (argv[1][1]) {

			case 'n' :
				nettype = argv[2];
				argc--;
				argv++;
				break;
			case 'h':
				hflag++;
				break;
			case 'a':
				aflag++;
				break;
			case 'i':
				iflag++;
				break;
			case 'l':
				lflag++;
				break;
			case 'u':
				uflag++;
				break;
			case 'd':
				dflag++;
				if (argc < 3)
					usage();
				debug = atoi(argv[2]);
				printf("Will collect %d responses.\n", debug);
				argc--;
				argv++;
				break;
			default:
				usage();
			}
		}
		argv++;
		argc--;
	}
	if (iflag + hflag + uflag > 1)
		usage();
	if (single > 0) {
		if (iflag || hflag || uflag)
			printnames();
		exit(0);
	}

	if (iflag || hflag || uflag) {
		printf("Collecting responses...\n");
		fflush(stdout);
	}
	vers = RUSERSVERS_3;
	utmp_array_res.utmp_array_val = NULL;
	utmp_array_res.utmp_array_len = 0;
	printf("Sending broadcast for rusersd protocol version 3...\n");
	(void) rpc_broadcast(RUSERSPROG, RUSERSVERS_3,
		RUSERSPROC_NAMES, (xdrproc_t) xdr_void, NULL,
		(xdrproc_t)xdr_utmp_array, (char *)&utmp_array_res,
		(resultproc_t) collectnames_3, nettype);
	vers = RUSERSVERS_IDLE;
	utmpidlearr.uia_arr = NULL;
	printf("Sending broadcast for rusersd protocol version 2...\n");
	(void) rpc_broadcast(RUSERSPROG, RUSERSVERS_IDLE,
		RUSERSPROC_NAMES, (xdrproc_t) xdr_void, NULL,
		(xdrproc_t)xdr_utmpidlearr, (char *)&utmpidlearr,
		(resultproc_t) collectnames, nettype);

	if (iflag || hflag || uflag)
		printnames();
	exit(0);
	/* NOTREACHED */
}

void
singlehost(name)
	char *name;
{
	enum clnt_stat err;
	struct utmpidlearr utmpidlearr;
	utmp_array	utmp_array_res;

	vers = RUSERSVERS_3;
	utmp_array_res.utmp_array_val = NULL;
	utmp_array_res.utmp_array_len = 0;
	err = rpc_call(name, RUSERSPROG, RUSERSVERS_3,
		RUSERSPROC_NAMES, (xdrproc_t) xdr_void, 0,
		(xdrproc_t) xdr_utmp_array, (char *)&utmp_array_res,
		nettype);
	if (err == RPC_SUCCESS) {
		print_info_3(&utmp_array_res, name);
		return;
	}
	if (err == RPC_PROGVERSMISMATCH) {
		utmpidlearr.uia_arr = NULL;
		err = rpc_call(name, RUSERSPROG, RUSERSVERS_IDLE,
				RUSERSPROC_NAMES, (xdrproc_t) xdr_void, 0,
				(xdrproc_t) xdr_utmpidlearr,
				(char *)&utmpidlearr, nettype);
	}
	if (err != RPC_SUCCESS) {
		fprintf(stderr, "%s: ", name);
		clnt_perrno(err);
		return;
	}
	print_info(&utmpidlearr, name);
	return;
}

/*
 * Collect responses from RUSERSVERS_IDLE broadcast, convert to
 * RUSERSVERS_3 format, and store in entry database.
 */
int
collectnames(resultsp, raddrp, nconf)
	char *resultsp;
	struct netbuf *raddrp;
	struct netconfig *nconf;
{
	struct utmpidlearr utmpidlearr;
	register struct entry *entryp, *lim;
	struct nd_hostservlist *hs;
	char host[MACHINELEN + 1];

	utmpidlearr = *(struct utmpidlearr *)resultsp;
	if (utmpidlearr.uia_cnt < 1 && !aflag)
		return (0);

	if (netdir_getbyaddr(nconf, &hs, raddrp)) {
#ifdef DEBUG
		netdir_perror("netdir_getbyaddr");
#endif
		/* netdir routine couldn't resolve addr;just print out uaddr */
		sprintf(host, "%.*s", MACHINELEN, taddr2uaddr(nconf, raddrp));
	} else {
		sprintf(host, "%.*s", MACHINELEN, hs->h_hostservs->h_host);
		netdir_free((char *)hs, ND_HOSTSERVLIST);
	}
	/*
	 * weed out duplicates
	 */
	lim = entry + curentry;
	for (entryp = entry; entryp < lim; entryp++) {
		if (!strcmp(entryp->machine, host))
			return (0);
	}
	return (print_info((struct utmpidlearr *)resultsp, host));
}

int
print_info(utmpidlearrp, name)
	struct utmpidlearr *utmpidlearrp;
	char *name;
{
	utmp_array *iconvert;
	int i, cnt, minidle;
	char host[MACHINELEN + 1];
	char username[NMAX+1];
	struct ru_utmp dmy;
	extern int debugcnt;

	cnt = utmpidlearrp->uia_cnt;
	(void) sprintf(host, "%.*s", MACHINELEN, name);

	/*
	 * if raw, print this entry out immediately
	 * otherwise store for later sorting
	 */
	if (!iflag && !hflag && !uflag) {
		if (lflag && (cnt > 0))
			for (i = 0; i < cnt; i++)
				putline_2(host, utmpidlearrp->uia_arr[i]);
		else {
		    printf("%-*.*s", MACHINELEN, MACHINELEN, host);
		    for (i = 0; i < cnt; i++) {
			if (sizeof (dmy.ut_name) < NMAX) {
				strncpy(username,
				    utmpidlearrp->uia_arr[i]->ui_utmp.ut_name,
				    sizeof (dmy.ut_name));
				username[sizeof (dmy.ut_name)] = '\0';
			} else {
				strncpy(username,
				    utmpidlearrp->uia_arr[i]->ui_utmp.ut_name,
				    NMAX);
				username[NMAX] = '\0';
			}
			printf(" %.*s", NMAX, username);
		    }
		    printf("\n");
		}
		/* store just the name */
		entry[curentry].machine = (char *)malloc(MACHINELEN+1);
		if (entry[curentry].machine == (char *) NULL) {
			fprintf(stderr, "Ran out of memory - exiting\n");
			exit(1);
		}
		(void) strcpy(entry[curentry].machine, name);
		entry[curentry++].cnt = 0;
		if (dflag && (++debugcnt >= debug))
			return (1);
		return (0);
	}
	entry[curentry].machine = (char *)malloc(MACHINELEN+1);
	if (entry[curentry].machine == (char *) NULL) {
		fprintf(stderr, "Ran out of memory - exiting\n");
		exit(1);
	}
	(void) strcpy(entry[curentry].machine, name);
	entry[curentry].cnt = cnt;
	iconvert = &entry[curentry].users;
	iconvert->utmp_array_len = cnt;
	iconvert->utmp_array_val = (rusers_utmp *)malloc(cnt *
		sizeof (rusers_utmp));
	minidle = INT_MAX;
	for (i = 0; i < cnt; i++) {
		iconvert->utmp_array_val[i].ut_user =
			strdup(utmpidlearrp->uia_arr[i]->ui_utmp.ut_name);
		iconvert->utmp_array_val[i].ut_line =
			strdup(utmpidlearrp->uia_arr[i]->ui_utmp.ut_line);
		iconvert->utmp_array_val[i].ut_host =
			strdup(utmpidlearrp->uia_arr[i]->ui_utmp.ut_host);
		iconvert->utmp_array_val[i].ut_time =
			utmpidlearrp->uia_arr[i]->ui_utmp.ut_time;
		iconvert->utmp_array_val[i].ut_idle =
			utmpidlearrp->uia_arr[i]->ui_idle;
		minidle = min(minidle, utmpidlearrp->uia_arr[i]->ui_idle);
	}
	entry[curentry].idle = minidle;

	if (curentry >= NUMENTRIES) {
		fprintf(stderr,
"Too many hosts on network for this program to handle (more than %d)\n",
			NUMENTRIES);
		printf("(Only processing %d hosts.)\n", NUMENTRIES);
		return (1);
	}
	curentry++;
	if (dflag && (++debugcnt >= debug))
		return (1);
	return (0);
}


/*
 * Collect responses from RUSERSVERS_3 broadcast.
 */
int
collectnames_3(resultsp, raddrp, nconf)
	caddr_t resultsp;
	struct netbuf *raddrp;
	struct netconfig *nconf;
{
	utmp_array *uap;
	register struct entry *entryp, *lim;
	struct nd_hostservlist *hs;
	char host[MACHINELEN + 1];

	uap = (utmp_array *)resultsp;
	if (uap->utmp_array_len < 1 && !aflag)
		return (0);

	if (netdir_getbyaddr(nconf, &hs, raddrp)) {
#ifdef DEBUG
	netdir_perror("netdir_getbyaddr");
#endif
		/* netdir routine couldn't resolve addr;just print out uaddr */
		sprintf(host, "%.*s", MACHINELEN, taddr2uaddr(nconf, raddrp));
	} else {
		sprintf(host, "%.*s", MACHINELEN, hs->h_hostservs->h_host);
		netdir_free((char *)hs, ND_HOSTSERVLIST);
	}
	/*
	 * weed out duplicates
	 */
	lim = entry + curentry;
	for (entryp = entry; entryp < lim; entryp++) {
		if (!strcmp(entryp->machine, host))
			return (0);
	}
	return (print_info_3(uap, host));
}

int
print_info_3(uap, name)
	utmp_array *uap;
	char *name;
{
	int i, cnt, minidle;
	char host[MACHINELEN + 1];
	extern int debugcnt;

	cnt = uap->utmp_array_len;

	(void) sprintf(host, "%.*s", MACHINELEN, name);

	/*
	 * if raw, print this entry out immediately
	 * otherwise store for later sorting
	 */
	if (!iflag && !hflag && !uflag) {
		if (lflag && (cnt > 0))
			for (i = 0; i < cnt; i++)
				putline_3(host, &uap->utmp_array_val[i]);
		else {
			printf("%-*.*s", MACHINELEN, MACHINELEN, host);
			for (i = 0; i < cnt; i++)
				printf(" %.*s", NMAX,
				    uap->utmp_array_val[i].ut_user);
			printf("\n");
		}
		/* store just the name */
		entry[curentry].machine = (char *)malloc(MACHINELEN+1);
		if (entry[curentry].machine == (char *) NULL) {
			fprintf(stderr, "Ran out of memory - exiting\n");
			exit(1);
		}
		(void) strcpy(entry[curentry].machine, name);
		entry[curentry++].cnt = 0;
		if (dflag && (++debugcnt >= debug))
			return (1);
		return (0);
	}

	entry[curentry].machine = (char *)malloc(MACHINELEN+1);
	if (entry[curentry].machine == (char *) NULL) {
		fprintf(stderr, "Ran out of memory - exiting\n");
		exit(1);
	}
	(void) strcpy(entry[curentry].machine, name);
	entry[curentry].cnt = cnt;
	entry[curentry].users.utmp_array_len = cnt;
	entry[curentry].users.utmp_array_val = (rusers_utmp *)malloc(cnt *
		sizeof (rusers_utmp));
	minidle = INT_MAX;
	for (i = 0; i < cnt; i++) {
		entry[curentry].users.utmp_array_val[i].ut_user =
			strdup(uap->utmp_array_val[i].ut_user);
		entry[curentry].users.utmp_array_val[i].ut_line =
			strdup(uap->utmp_array_val[i].ut_line);
		entry[curentry].users.utmp_array_val[i].ut_host =
			strdup(uap->utmp_array_val[i].ut_host);
		entry[curentry].users.utmp_array_val[i].ut_time =
			uap->utmp_array_val[i].ut_time;
		entry[curentry].users.utmp_array_val[i].ut_idle =
			uap->utmp_array_val[i].ut_idle;
		minidle = min(minidle, uap->utmp_array_val[i].ut_idle);
	}
	entry[curentry].idle = minidle;

	if (curentry >= NUMENTRIES) {
		fprintf(stderr, "Too many hosts on network (more than %d)\n",
			NUMENTRIES);
		exit(1);
	}
	curentry++;
	if (dflag && (++debugcnt >= debug))
		return (1);
	return (0);
}

void
printnames()
{
	int i, j;
	int (*compare)();

	/* the name of the machine should already be in the structure */
	if (iflag)
		compare = icompare;
	else if (hflag)
		compare = hcompare;
	else
		compare = ucompare;
	qsort(entry, curentry, sizeof (struct entry), compare);
	for (i = 0; i < curentry; i++) {
		if (!lflag || (entry[i].cnt < 1)) {
			printf("%-*.*s", MACHINELEN,
					MACHINELEN, entry[i].machine);
			for (j = 0; j < entry[i].cnt; j++)
				printf(" %.*s", NMAX,
				    entry[i].users.utmp_array_val[j].ut_user);
			printf("\n");
		} else {
			for (j = 0; j < entry[i].cnt; j++)
				putline_3(entry[i].machine,
					&entry[i].users.utmp_array_val[j]);
		}
	}
}

hcompare(a, b)
	struct entry *a, *b;
{
	return (strcmp(a->machine, b->machine));
}

ucompare(a, b)
	struct entry *a, *b;
{
	return (b->cnt - a->cnt);
}

icompare(a, b)
	struct entry *a, *b;
{
	return (a->idle - b->idle);
}

void
putline_2(host, uip)
	char *host;
	struct utmpidle *uip;
{
	register char *cbuf;
	struct ru_utmp *up;
	char buf[100];

	up = &uip->ui_utmp;
#define	NAMEMAX	((sizeof (up->ut_name) < NMAX) ? NMAX : sizeof (up->ut_name))
#define	NAMEMIN	((sizeof (up->ut_name) > NMAX) ? NMAX : sizeof (up->ut_name))
	/* Try and align this up nicely */
#define	LINEMAX	sizeof (up->ut_line)
#define	HOSTMAX	sizeof (up->ut_host)
	/*
	 * We copy the strings into a buffer because they aren't strictly
	 * speaking strings but byte arrays (and they may not have a
	 * terminating NULL.
	 */

	strncpy(buf, up->ut_name, NAMEMAX);
	buf[NAMEMIN] = '\0';
	printf("%-*.*s ", NAMEMAX, NAMEMAX, buf);

	(void) strcpy(buf, host);
	(void) strcat(buf, ":");
	(void) strncat(buf, up->ut_line, LINEMAX);
	buf[MACHINELEN+LINEMAX] = '\0';
	(void) printf("%-*.*s", MACHINELEN+LINEMAX, MACHINELEN+LINEMAX, buf);

	cbuf = (char *)ctime(&up->ut_time);
	(void) printf("  %.12s  ", cbuf+4);
	if (uip->ui_idle == INT_MAX)
		printf("    ??");
	else
		prttime(uip->ui_idle, "");
	if (up->ut_host[0]) {
		strncpy(buf, up->ut_host, HOSTMAX);
		buf[HOSTMAX] = '\0';
		printf(" (%.*s)", HOSTMAX, buf);
	}
	putchar('\n');
}

void
putline_3(host, rup)
	char *host;
	rusers_utmp *rup;
{
	register char *cbuf;
	char buf[100];

	printf("%-*.*s ", NMAX, NMAX, rup->ut_user);
	(void) strcpy(buf, host);
	(void) strcat(buf, ":");
	(void) strncat(buf, rup->ut_line, LMAX);
	(void) printf("%-*.*s", MACHINELEN+LMAX, MACHINELEN+LMAX, buf);

	cbuf = (char *)ctime((time_t *)&rup->ut_time);
	(void) printf("  %.12s  ", cbuf+4);
	if (rup->ut_idle == INT_MAX)
		printf("    ??");
	else
		prttime(rup->ut_idle, "");
	if (rup->ut_host[0])
		printf(" (%.*s)", HMAX, rup->ut_host);
	putchar('\n');
}

/*
 * prttime prints a time in hours and minutes.
 * The character string tail is printed at the end, obvious
 * strings to pass are "", " ", or "am".
 */
void
prttime(tim, tail)
	time_t tim;
	char *tail;
{
	register int didhrs = 0;

	if (tim >= 60) {
		printf("%3d:", tim/60);
		didhrs++;
	} else {
		printf("    ");
	}
	tim %= 60;
	if (tim > 0 || didhrs) {
		printf(didhrs && tim < 10 ? "%02d" : "%2d", tim);
	} else {
		printf("  ");
	}
	printf("%s", tail);
}

#ifdef DEBUG
/*
 * for debugging
 */
printit(i)
{
	int j, v;

	printf("%12.12s: ", entry[i].machine);
	if (entry[i].cnt) {
		putline_3(entry[i].machine, &entry[i].users.utmp_array_val[0]);
		for (j = 1; j < entry[i].cnt; j++) {
			printf("\t");
			putline_3(entry[i].machine,
				&entry[i].users.utmp_array_val[j]);
		}
	} else
		printf("\n");
}
#endif

void
usage()
{
	fprintf(stderr, "Usage: rusers [-a] [-h] [-i] [-l] [-u] [host ...]\n");
	exit(1);
}
