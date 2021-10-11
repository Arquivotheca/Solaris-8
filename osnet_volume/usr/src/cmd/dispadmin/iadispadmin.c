
/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)iadispadmin.c	1.1	94/01/05 SMI"
#include	<stdio.h>
#include	<string.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/priocntl.h>
#include	<sys/iapriocntl.h>
#include	<sys/param.h>
#include	<sys/ia.h>

#include	"dispadmin.h"

/*
 * This file contains the class specific code implementing
 * the interactive dispadmin sub-command.
 */

#define	BASENMSZ	16

extern int	errno;
extern char	*basename();
extern void	fatalerr();
extern long	hrtconvert();

static void	get_iadptbl(), set_iadptbl();

static char usage[] =
"usage:	dispadmin -l\n\
	dispadmin -c IA -g [-r res]\n\
	dispadmin -c IA -s infile\n";

static char	basenm[BASENMSZ];
static char	cmdpath[256];


main(argc, argv)
int	argc;
char	**argv;
{
	extern char	*optarg;

	int		c;
	int		lflag, gflag, rflag, sflag;
	ulong		res;
	char		*infile;

	strcpy(cmdpath, argv[0]);
	strcpy(basenm, basename(argv[0]));
	lflag = gflag = rflag = sflag = 0;
	while ((c = getopt(argc, argv, "lc:gr:s:")) != -1) {
		switch (c) {

		case 'l':
			lflag++;
			break;

		case 'c':
			if (strcmp(optarg, "IA") != 0)
				fatalerr("error: %s executed for %s class, \
%s is actually sub-command for IA class\n", cmdpath, optarg, cmdpath);
			break;

		case 'g':
			gflag++;
			break;

		case 'r':
			rflag++;
			res = strtoul(optarg, (char **)NULL, 10);
			break;

		case 's':
			sflag++;
			infile = optarg;
			break;

		case '?':
			fatalerr(usage);

		default:
			break;
		}
	}

	if (lflag) {
		if (gflag || rflag || sflag)
			fatalerr(usage);

		printf("IA\t(Interactive)\n");
		exit(0);

	} else if (gflag) {
		if (lflag || sflag)
			fatalerr(usage);

		if (rflag == 0)
			res = 1000;

		get_iadptbl(res);
		exit(0);

	} else if (sflag) {
		if (lflag || gflag || rflag)
			fatalerr(usage);

		set_iadptbl(infile);
		exit(0);

	} else {
		fatalerr(usage);
	}
}


/*
 * Retrieve the current ia_dptbl from memory, convert the time quantum
 * values to the resolution specified by res and write the table to stdout.
 */
static void
get_iadptbl(res)
ulong	res;
{
	int		i;
	int		iadpsz;
	pcinfo_t	pcinfo;
	pcadmin_t	pcadmin;
	iaadmin_t	iaadmin;
	iadpent_t	*ia_dptbl;
	hrtimer_t	hrtime;

	strcpy(pcinfo.pc_clname, "IA");
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("%s: Can't get IA class ID, priocntl system \
call failed with errno %d\n", basenm, errno);

	pcadmin.pc_cid = pcinfo.pc_cid;
	pcadmin.pc_cladmin = (char *)&iaadmin;
	iaadmin.ia_cmd = IA_GETDPSIZE;

	if (priocntl(0, 0, PC_ADMIN, (caddr_t)&pcadmin) == -1)
		fatalerr("%s: Can't get ia_dptbl size, priocntl system \
call failed with errno %d\n", basenm, errno);

	iadpsz = iaadmin.ia_ndpents * sizeof(iadpent_t);
	if ((ia_dptbl = (iadpent_t *)malloc(iadpsz)) == NULL)
		fatalerr("%s: Can't allocate memory for ia_dptbl\n", basenm);

	iaadmin.ia_dpents = ia_dptbl;

	iaadmin.ia_cmd = IA_GETDPTBL;
	if (priocntl(0, 0, PC_ADMIN, (caddr_t)&pcadmin) == -1)
		fatalerr("%s: Can't get ia_dptbl, priocntl system call \
call failed with errno %d\n", basenm, errno);

	printf("# Interactive Dispatcher Configuration\n");
	printf("RES=%ld\n\n", res);
	printf("# ia_quantum  ia_tqexp  ia_slpret  ia_maxwait ia_lwait  \
PRIORITY LEVEL\n");

	for (i = 0; i < iaadmin.ia_ndpents; i++) {
		if (res != HZ) {
			hrtime.hrt_secs = 0;
			hrtime.hrt_rem = ia_dptbl[i].ia_quantum;
			hrtime.hrt_res = HZ;
			if (_hrtnewres(&hrtime, res, HRT_RNDUP) == -1)
				fatalerr("%s: Can't convert to requested \
resolution\n", basenm);
			if ((ia_dptbl[i].ia_quantum = hrtconvert(&hrtime))
			    == -1)
				fatalerr("%s: Can't express time quantum in \
requested resolution,\ntry coarser resolution\n", basenm);
		}
		printf("%10d%10d%10d%12d%10d        #   %3d\n",
		    ia_dptbl[i].ia_quantum, ia_dptbl[i].ia_tqexp,
		    ia_dptbl[i].ia_slpret, ia_dptbl[i].ia_maxwait,
		    ia_dptbl[i].ia_lwait, i);
	}
}


/*
 * Read the ia_dptbl values from infile, convert the time quantum values
 * to HZ resolution, do a little sanity checking and overwrite the table
 * in memory with the values from the file.
 */
static void
set_iadptbl(infile)
char	*infile;
{
	int		i;
	int		niadpents;
	char		*tokp;
	pcinfo_t	pcinfo;
	pcadmin_t	pcadmin;
	iaadmin_t	iaadmin;
	iadpent_t	*ia_dptbl;
	int		linenum;
	ulong		res;
	hrtimer_t	hrtime;
	FILE		*fp;
	char		buf[512];
	int		wslength;

	strcpy(pcinfo.pc_clname, "IA");
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("%s: Can't get IA class ID, priocntl system \
call failed with errno %d\n", basenm, errno);

	pcadmin.pc_cid = pcinfo.pc_cid;
	pcadmin.pc_cladmin = (char *)&iaadmin;
	iaadmin.ia_cmd = IA_GETDPSIZE;

	if (priocntl(0, 0, PC_ADMIN, (caddr_t)&pcadmin) == -1)
		fatalerr("%s: Can't get ia_dptbl size, priocntl system \
call failed with errno %d\n", basenm, errno);

	niadpents = iaadmin.ia_ndpents;
	if ((ia_dptbl =
	    (iadpent_t *)malloc(niadpents * sizeof(iadpent_t))) == NULL)
		fatalerr("%s: Can't allocate memory for ia_dptbl\n", basenm);

	if ((fp = fopen(infile, "r")) == NULL)
		fatalerr("%s: Can't open %s for input\n", basenm, infile);

	linenum = 0;

	/*
	 * Find the first non-blank, non-comment line.  A comment line
	 * is any line with '#' as the first non-white-space character.
	 */
	do {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			fatalerr("%s: Too few lines in input table\n",basenm);
		linenum++;
	} while (buf[0] == '#' || buf[0] == '\0' ||
	    (wslength = strspn(buf, " \t\n")) == strlen(buf) ||
	    strchr(buf, '#') == buf + wslength);

	if ((tokp = strtok(buf, " \t")) == NULL)
		fatalerr("%s: Bad RES specification, line %d of input file\n",
		    basenm, linenum);
	if ((int)strlen(tokp) > 4) {
		if (strncmp(tokp, "RES=", 4) != 0)
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		if (tokp[4] == '-')
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		res = strtoul(&tokp[4], (char **)NULL, 10);
	} else if (strlen(tokp) == 4) {
		if (strcmp(tokp, "RES=") != 0)
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		if ((tokp = strtok(NULL, " \t")) == NULL)
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		if (tokp[0] == '-')
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		res = strtoul(tokp, (char **)NULL, 10);
	} else if (strlen(tokp) == 3) {
		if (strcmp(tokp, "RES") != 0)
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		if ((tokp = strtok(NULL, " \t")) == NULL)
			fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
		if ((int)strlen(tokp) > 1) {
			if (strncmp(tokp, "=", 1) != 0)
				fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
			if (tokp[1] == '-')
				fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
			res = strtoul(&tokp[1], (char **)NULL, 10);
		} else if (strlen(tokp) == 1) {
			if ((tokp = strtok(NULL, " \t")) == NULL)
				fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
			if (tokp[0] == '-')
				fatalerr("%s: Bad RES specification, \
line %d of input file\n", basenm, linenum);
			res = strtoul(tokp, (char **)NULL, 10);
		}
	} else {
		fatalerr("%s: Bad RES specification, line %d of input file\n",
		    basenm, linenum);
	}

	/*
	 * The remainder of the input file should contain exactly enough
	 * non-blank, non-comment lines to fill the table (ia_ndpents lines).
	 * We assume that any non-blank, non-comment line is data for the
	 * table and fail if we find more or less than we need.
	 */
	for (i = 0; i < iaadmin.ia_ndpents; i++) {

		/*
		 * Get the next non-blank, non-comment line.
		 */
		do {
			if (fgets(buf, sizeof(buf), fp) == NULL)
				fatalerr("%s: Too few lines in input table\n",
				    basenm);
			linenum++;
		} while (buf[0] == '#' || buf[0] == '\0' ||
		    (wslength = strspn(buf, " \t\n")) == strlen(buf) ||
		    strchr(buf, '#') == buf + wslength);

		if ((tokp = strtok(buf, " \t")) == NULL)
			fatalerr("%s: Too few values, line %d of input file\n",
			    basenm, linenum);

		if (res != HZ) {
			hrtime.hrt_secs = 0;
			hrtime.hrt_rem = atol(tokp);
			hrtime.hrt_res = res;
			if (_hrtnewres(&hrtime, HZ, HRT_RNDUP) == -1)
				fatalerr("%s: Can't convert specified \
resolution to ticks\n", basenm);
			if((ia_dptbl[i].ia_quantum = hrtconvert(&hrtime)) == -1)
				fatalerr("%s: ia_quantum value out of \
valid range; line %d of input,\ntable not overwritten\n", basenm, linenum);
		} else {
			ia_dptbl[i].ia_quantum = atol(tokp);
		}
		if (ia_dptbl[i].ia_quantum <= 0)
			fatalerr("%s: ia_quantum value out of valid range; \
line %d of input,\ntable not overwritten\n", basenm, linenum);

		if ((tokp = strtok(NULL, " \t")) == NULL || tokp[0] == '#')
			fatalerr("%s: Too few values, line %d of input file\n",
			    basenm, linenum);
		ia_dptbl[i].ia_tqexp = (short)atoi(tokp);
		if (ia_dptbl[i].ia_tqexp < 0 ||
		    ia_dptbl[i].ia_tqexp > iaadmin.ia_ndpents)
			fatalerr("%s: ia_tqexp value out of valid range; \
line %d of input,\ntable not overwritten\n", basenm, linenum);

		if ((tokp = strtok(NULL, " \t")) == NULL || tokp[0] == '#')
			fatalerr("%s: Too few values, line %d of input file\n",
			    basenm, linenum);
		ia_dptbl[i].ia_slpret = (short)atoi(tokp);
		if (ia_dptbl[i].ia_slpret < 0 ||
		    ia_dptbl[i].ia_slpret > iaadmin.ia_ndpents)
			fatalerr("%s: ia_slpret value out of valid range; \
line %d of input,\ntable not overwritten\n", basenm, linenum);

		if ((tokp = strtok(NULL, " \t")) == NULL || tokp[0] == '#')
			fatalerr("%s: Too few values, line %d of input file\n",
			    basenm, linenum);
		ia_dptbl[i].ia_maxwait = (short)atoi(tokp);
		if (ia_dptbl[i].ia_maxwait < 0)
			fatalerr("%s: ia_maxwait value out of valid range; \
line %d of input,\ntable not overwritten\n", basenm, linenum);

		if ((tokp = strtok(NULL, " \t")) == NULL || tokp[0] == '#')
			fatalerr("%s: Too few values, line %d of input file\n",
			    basenm, linenum);
		ia_dptbl[i].ia_lwait = (short)atoi(tokp);
		if (ia_dptbl[i].ia_lwait < 0 ||
		    ia_dptbl[i].ia_lwait > iaadmin.ia_ndpents)
			fatalerr("%s: ia_lwait value out of valid range; \
line %d of input,\ntable not overwritten\n", basenm, linenum);

		if ((tokp = strtok(NULL, " \t")) != NULL && tokp[0] != '#')
			fatalerr("%s: Too many values, line %d of input file\n",
			    basenm, linenum);
	}

	/*
	 * We've read enough lines to fill the table.  We fail
	 * if the input file contains any more.
	 */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (buf[0] != '#' && buf[0] != '\0' &&
		    (wslength = strspn(buf, " \t\n")) != strlen(buf) &&
		    strchr(buf, '#') != buf + wslength)
			fatalerr("%s: Too many lines in input table\n",
			    basenm);
	}

	iaadmin.ia_dpents = ia_dptbl;
	iaadmin.ia_cmd = IA_SETDPTBL;
	if (priocntl(0, 0, PC_ADMIN, (caddr_t)&pcadmin) == -1)
		fatalerr("%s: Can't set ia_dptbl, priocntl system call \
failed with errno %d\n", basenm, errno);
}
