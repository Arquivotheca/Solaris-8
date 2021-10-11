/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)iapriocntl.c	1.3	95/02/16 SMI"	/* SVr4.0 1.6	*/
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/types.h>
#include	<sys/procset.h>
#include	<sys/priocntl.h>
#include	<sys/iapriocntl.h>
#include	<errno.h>

#include	"priocntl.h"

/*
 * This file contains the class specific code implementing
 * the time-sharing priocntl sub-command.
 */

#define	BASENMSZ	16

static void	print_iainfo(), print_iaprocs(), set_iaprocs(), exec_iacmd();

static char usage[] =
"usage:	priocntl -l\n\
	priocntl -d [-i idtype] [idlist]\n\
	priocntl -s [-c IA] [-m iauprilim] [-p iaupri] [-t iamode]\n\
		    [-i idtype] [idlist]\n\
	priocntl -e [-c IA] [-m iauprilim] [-p iaupri] [-t iamode]\n\
		    command [argument(s)]\n";

static char	cmdpath[256];
static char	basenm[BASENMSZ];


main(argc, argv)
int	argc;
char	**argv;
{
	extern char	*optarg;
	extern int	optind;

	int		c;
	int		lflag, dflag, sflag, mflag, pflag, eflag, iflag, tflag;
	int		iamode;
	int		iauprilim;
	int		iaupri;
	char		*idtypnm;
	idtype_t	idtype;
	int		idargc;

	strcpy(cmdpath, argv[0]);
	strcpy(basenm, basename(argv[0]));
	lflag = dflag = sflag = mflag = pflag = eflag = iflag = tflag = 0;
	while ((c = getopt(argc, argv, "ldsm:p:t:ec:i:")) != -1) {
		switch (c) {

		case 'l':
			lflag++;
			break;

		case 'd':
			dflag++;
			break;

		case 's':
			sflag++;
			break;

		case 'm':
			mflag++;
			iauprilim = (int)atoi(optarg);
			break;

		case 'p':
			pflag++;
			iaupri = (int)atoi(optarg);
			break;

		case 't':
			tflag++;
			iamode = (int)atoi(optarg);
			break;

		case 'e':
			eflag++;
			break;

		case 'c':
			if (strcmp(optarg, "IA") != 0)
				fatalerr("error: %s executed for %s class, \
%s is actually sub-command for IA class\n", cmdpath, optarg, cmdpath);
			break;

		case 'i':
			iflag++;
			idtypnm = optarg;
			break;

		case '?':
			fatalerr(usage);

		default:
			break;
		}
	}

	if (lflag) {
		if (dflag || sflag || mflag || pflag || tflag || eflag || iflag)
			fatalerr(usage);

		print_iainfo();
		exit(0);

	} else if (dflag) {
		if (lflag || sflag || mflag || pflag || tflag || eflag)
			fatalerr(usage);

		print_iaprocs();
		exit(0);

	} else if (sflag) {
		if (lflag || dflag || eflag)
			fatalerr(usage);

		if (iflag) {
			if (str2idtyp(idtypnm, &idtype) == -1)
				fatalerr("%s: Bad idtype %s\n", basenm,
				    idtypnm);
		} else
			idtype = P_PID;

		if (mflag == 0)
			iauprilim = IA_NOCHANGE;

		if (pflag == 0)
			iaupri = IA_NOCHANGE;

		if (tflag == 0)
			iamode = IA_NOCHANGE;

		if (optind < argc)
			idargc = argc - optind;
		else
			idargc = 0;

		set_iaprocs(idtype, idargc, &argv[optind], iauprilim, iaupri,
		    iamode);
		exit(0);

	} else if (eflag) {
		if (lflag || dflag || sflag || iflag)
			fatalerr(usage);

		if (mflag == 0)
			iauprilim = IA_NOCHANGE;

		if (pflag == 0)
			iaupri = IA_NOCHANGE;

		if (tflag == 0)
			iamode = IA_NOCHANGE;

		exec_iacmd(&argv[optind], iauprilim, iaupri, iamode);

	} else {
		fatalerr(usage);
	}
}


/*
 * Print our class name and the configured user priority range.
 */
static void
print_iainfo()
{
	pcinfo_t	pcinfo;

	strcpy(pcinfo.pc_clname, "IA");

	printf("IA (Interactive)\n");

	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("\tCan't get configured IA user priority range\n");

	printf("\tConfigured IA User Priority Range: -%d through %d\n",
	    ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri,
	    ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri);
}


/*
 * Read a list of pids from stdin and print the user priority and user
 * priority limit for each of the corresponding processes.
 * print their interactive mode and nice values
 */
static void
print_iaprocs()
{
	pid_t		pidlist[NPIDS];
	int		numread;
	int		i;
	id_t		iacid;
	pcinfo_t	pcinfo;
	pcparms_t	pcparms;

	numread = fread(pidlist, sizeof (pid_t), NPIDS, stdin);

	printf("INTERACTIVE CLASS PROCESSES:");
	printf("\n    PID    IAUPRILIM    IAUPRI    IAMODE\n");

	strcpy(pcinfo.pc_clname, "IA");

	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("%s: Can't get IA class ID\n", basenm);

	iacid = pcinfo.pc_cid;

	if (numread <= 0)
		fatalerr("%s: No pids on input\n", basenm);


	pcparms.pc_cid = PC_CLNULL;
	for (i = 0; i < numread; i++) {
		printf("%7ld", pidlist[i]);
		if (priocntl(P_PID, pidlist[i], PC_GETPARMS,
		    (caddr_t)&pcparms) == -1) {
			printf("\tCan't get IA user priority\n");
			continue;
		}

		if (pcparms.pc_cid == iacid) {
			printf("    %5d       %5d     %5d\n",
			    ((iaparms_t *)pcparms.pc_clparms)->ia_uprilim,
			    ((iaparms_t *)pcparms.pc_clparms)->ia_upri,
			    ((iaparms_t *)pcparms.pc_clparms)->ia_mode);
		} else {

			/*
			 * Process from some class other than interactive.
			 * It has probably changed class while priocntl
			 * command was executing (otherwise we wouldn't
			 * have been passed its pid).  Print the little
			 * we know about it.
			 */
			pcinfo.pc_cid = pcparms.pc_cid;
			if (priocntl(0, 0, PC_GETCLINFO,
			    (caddr_t)&pcinfo) != -1)
				printf("%ld\tChanged to class %s while \
priocntl command executing\n", pidlist[i], pcinfo.pc_clname);

		}
	}
}


/*
 * Set all processes in the set specified by idtype/idargv to interactive
 * (if they aren't already interactive ) and set their user priority limit
 * and user priority to those specified by iauprilim and iaupri.
 */
static void
set_iaprocs(idtype, idargc, idargv, iauprilim, iaupri, iamode)
idtype_t	idtype;
int		idargc;
char		**idargv;
int		iauprilim;
int		iaupri;
int		iamode;
{
	pcinfo_t	pcinfo;
	pcparms_t	pcparms;
	int		maxupri;
	char		idtypnm[12];
	int		i;
	id_t		id;

	/*
	 * Get the interactive class ID and max configured user priority.
	 */
	strcpy(pcinfo.pc_clname, "IA");
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("%s: Can't get IA class ID, priocntl system call \
failed with errno %d\n", basenm, errno);
	maxupri = ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri;

	/*
	 * Validate the iauprilim and iaupri arguments.
	 */
	if ((iauprilim > maxupri || iauprilim < -maxupri) &&
	    iauprilim != IA_NOCHANGE)
		fatalerr("%s: Specified user priority limit %d out of \
configured range\n", basenm, iauprilim);

	if ((iaupri > maxupri || iaupri < -maxupri) &&
	    iaupri != IA_NOCHANGE)
		fatalerr("%s: Specified user priority %d out of \
configured range\n", basenm, iaupri);

	if (iamode != IA_INTERACTIVE_OFF && iamode != IA_SET_INTERACTIVE &&
	    iamode != IA_NOCHANGE)
		fatalerr("%s: Specified illegal mode %d\n", basenm, iamode);

	pcparms.pc_cid = pcinfo.pc_cid;
	((iaparms_t *)pcparms.pc_clparms)->ia_uprilim = iauprilim;
	((iaparms_t *)pcparms.pc_clparms)->ia_upri = iaupri;
	((iaparms_t *)pcparms.pc_clparms)->ia_mode = iamode;

	if (idtype == P_ALL) {
		if (priocntl(P_ALL, 0, PC_SETPARMS, (caddr_t)&pcparms) == -1) {
			if (errno == EPERM)
				fprintf(stderr, "Permissions error \
encountered on one or more processes.\n");
			else
				fatalerr("%s: Can't reset interactive \
parameters\npriocntl system call failed with errno %d\n", basenm, errno);
		}
	} else if (idargc == 0) {
		if (priocntl(idtype, P_MYID, PC_SETPARMS,
		    (caddr_t)&pcparms) == -1) {
			if (errno == EPERM) {
				(void) idtyp2str(idtype, idtypnm);
				fprintf(stderr, "Permissions error \
encountered on current %s.\n", idtypnm);
			} else {
				fatalerr("%s: Can't reset interactive \
parameters\npriocntl system call failed with errno %d\n", basenm, errno);
			}
		}
	} else {
		(void) idtyp2str(idtype, idtypnm);
		for (i = 0; i < idargc; i++) {
		    if (idtype == P_CID) {
			strcpy(pcinfo.pc_clname, idargv[i]);
			if (priocntl(0, 0, PC_GETCID,
					(caddr_t)&pcinfo) == -1)
			    fatalerr("%s: Invalid or unconfigured class %s, \
priocntl system call failed with errno %d\n", basenm, pcinfo.pc_clname, errno);
			id = pcinfo.pc_cid;
		    } else
			id = (id_t)atol(idargv[i]);

		    if (priocntl(idtype, id,
			    PC_SETPARMS, (caddr_t)&pcparms) == -1) {
				if (errno == EPERM)
					fprintf(stderr, "Permissions error \
encountered on %s %s.\n", idtypnm, idargv[i]);
				else
					fatalerr("%s: Can't reset interactive \
parameters\npriocntl system call failed with errno %d\n", basenm, errno);
			}
		}
	}

}


/*
 * Execute the command pointed to by cmdargv as a interactive process
 * with the user priority limit given by iauprilim and user priority iaupri.
 */
static void
exec_iacmd(cmdargv, iauprilim, iaupri, iamode)
char	**cmdargv;
int	iauprilim;
int	iaupri;
int	iamode;
{
	pcinfo_t	pcinfo;
	pcparms_t	pcparms;
	int		maxupri;

	/*
	 * Get the time sharing class ID and max configured user priority.
	 */
	strcpy(pcinfo.pc_clname, "IA");
	if (priocntl(0, 0, PC_GETCID, (caddr_t)&pcinfo) == -1)
		fatalerr("%s: Can't get IA class ID, priocntl system call \
failed with errno %d\n", basenm, errno);
	maxupri = ((iainfo_t *)pcinfo.pc_clinfo)->ia_maxupri;

	/*
	 * Validate the iauprilim and iaupri arguments.
	 */
	if ((iauprilim > maxupri || iauprilim < -maxupri) &&
	    iauprilim != IA_NOCHANGE)
		fatalerr("%s: Specified user priority limit %d out of \
configured range\n", basenm, iauprilim);

	if ((iaupri > maxupri || iaupri < -maxupri) &&
	    iaupri != IA_NOCHANGE)
		fatalerr("%s: Specified user priority %d out of \
configured range\n", basenm, iaupri);

	if (iamode != IA_INTERACTIVE_OFF && iamode != IA_SET_INTERACTIVE &&
	    iamode != IA_NOCHANGE)
		fatalerr("%s: Specified illegal mode %d\n", basenm, iamode);

	pcparms.pc_cid = pcinfo.pc_cid;
	((iaparms_t *)pcparms.pc_clparms)->ia_uprilim = iauprilim;
	((iaparms_t *)pcparms.pc_clparms)->ia_upri = iaupri;
	((iaparms_t *)pcparms.pc_clparms)->ia_mode = iamode;
	if (priocntl(P_PID, P_MYID, PC_SETPARMS, (caddr_t)&pcparms) == -1)
		fatalerr("%s: Can't reset interactive parameters\n\
priocntl system call failed with errno %d\n", basenm, errno);

	(void) execvp(cmdargv[0], cmdargv);
	fatalerr("%s: Can't execute %s, exec failed with errno %d\n",
	    basenm, cmdargv[0], errno);
}
