/*
 * Copyright (c) 1993, Sun Microsystems Inc.
 *
 * configure contiguous memory for the SX video subsystem
 */
#pragma	ident	"@(#)sxconfig.c	1.7	94/12/20	SMI"


#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/sx_cmemio.h>

#define	DFLT_CMEM_MBREQ 0	/* No cmem by default */
#define	DFLT_CMEM_FRAG 0	/* No fragmentation by default */
#define	DFLT_CMEM_MBLEFT 32	/* 32MB for system use by default */
#define	BUFSIZE 100
#define	ONE_MBYTE 0x100000
#define	SIGNAL_NUM 4	/* The number of signals to catch */
#define	DEVNAME "/dev/sx_cmem"
#define	RECONFIGURE "/reconfigure"
#define	CONF_OLD_LOC	"/kernel/drv/sx_cmem.conf"
#define	CONF_NEW_LOC	"/platform/sun4m/kernel/drv/sx_cmem.conf"


static	char	*tmp_file;
static	struct  sigaction act[SIGNAL_NUM];


/*
 * Usage.
 */
static void
usage(char *s)
{
	fprintf(stderr, "usage: %s -c\n", s);
	fprintf(stderr, "       %s -d\n", s);
	fprintf(stderr, "       %s [-s integer] "
		"[-l integer] [-f | -n]\n", s);
}


/*
 * Clean up the tmp_file.
 */
static void
cleanup(void)
{
	if (access(tmp_file, F_OK) == 0)
		unlink(tmp_file);
}


/*
 * Does the string contain digits only?
 */
static int
isint(char *s)
{
	while (*s) {
		if (isdigit(*s))
			s++;
		else
			return (0);
	}
	return (1);
}


main(int argc, char **argv)
{
	int	cmem_mbreq;	/* amount of cmem to be reserved */
	int	cmem_frag;	/* fragmentation */
	int	cmem_mbleft;	/* amount of mem for system use */
	int	input_mbreq;	/* user's cmem_mbreq input */
	int	input_frag;	/* user's cmem_frag input */
	int	input_mbleft;	/* user's cmem_mbleft input */
	int	fd, i, status;
	struct	sx_cmem_config arg;
	char	c, s[BUFSIZE], config_str[BUFSIZE], cp_str[BUFSIZE];
	FILE	*infp, *outfp;
	struct	stat	buf;
	extern	char	*optarg;
	int	cflg, dflg, sflg, lflg, fflg, nflg, errflg;
	long	page_size, phys_mem_pages, phys_mem_installed;
	int	signal[SIGNAL_NUM] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM};
	char	confname[100];


	/*
	 * Variables Initialization.
	 */
	cflg = dflg = sflg = lflg = fflg = nflg = errflg = 0;
	input_mbreq = input_frag = input_mbleft = -1;


	/*
	 * Ignore SIGHUP, SIGINT, SIGQUIT, SIGTERM signals.
	 */
	for (i = 0; i < SIGNAL_NUM; i++) {
		act[i].sa_flags = 0;
		act[i].sa_handler = SIG_IGN;
		sigaction(signal[i], &act[i], NULL);
	}


	/*
	 * Parse the command arguments.
	 */
	if (argc <= 1) {
		usage(*argv);
		exit(1);
	}

	while ((c = getopt(argc, argv, "cdfns:l:")) != -1) {
		switch (c) {
		case 'c':
			if (!dflg && !sflg && !lflg && !fflg && !nflg)
				cflg++;
			else
				errflg++;
			break;
		case 'd':
			if (!cflg && !sflg && !lflg && !fflg && !nflg)
				dflg++;
			else
				errflg++;
			break;
		case 's':
			if (!cflg && !dflg && isint(optarg)) {
				sflg++;
				input_mbreq = atoi(optarg);
			} else
				errflg++;
			break;
		case 'l':
			if (!cflg && !dflg && isint(optarg)) {
				sflg++;
				input_mbleft = atoi(optarg);
			} else
				errflg++;
			break;
		case 'f':
			if (!cflg && !dflg && !nflg) {
				fflg++;
				input_frag = 1;
			} else
				errflg++;
			break;
		case 'n':
			if (!cflg && !dflg && !fflg) {
				nflg++;
				input_frag = 0;
			} else
				errflg++;
			break;
		case '?':
			errflg++;
		}
	}

	/*
	 * If wrong option(s) or incorrect combination of options
	 * are given by users, give them a hint and then exit.
	 */
	if (errflg) {
		usage(*argv);
		exit(2);
	}


	/*
	 * Only root can do things other than -c option.
	 */
	if (!cflg && (getuid() != 0)) {
		fprintf(stderr, "Permission denied: only super user "
			"can change the configuration.\n");
		exit(3);
	}


	/*
	 * Read the config file line by line. If the line is the
	 * config line, save the request values. The rest lines
	 * should be just comments. Copy comments to a tmp_file.
	 * Later we will append the final config line to the tmp_file
	 * and then overwrite the config file with that tmp_file.
	 *
	 */

	/*
	 * First of all, the config file must exist and is readable.
	 * The configuration file has been moved from /kernel/drv to
	 * platform specific directory since SunOS 5.5.
	 */
	if (access(CONF_NEW_LOC, F_OK) == 0)
		strcpy(confname, CONF_NEW_LOC);
	else if (access(CONF_OLD_LOC, F_OK) == 0)
		strcpy(confname, CONF_OLD_LOC);
	else {
		fprintf(stderr, "Configuration file does not exist.\n");
		exit(4);
	}

	if ((infp = fopen(confname, "r")) == NULL) {
		fprintf(stderr, "Can not open %s for read.\n", confname);
		exit(-1);
	}

	/* Create a unique tmp_file. */
	tmp_file = mktemp("/tmp/sxconfig.XXXXXX");
	if ((outfp = fopen(tmp_file, "w")) == NULL) {
		fprintf(stderr, "Can not open %s for write.\n", tmp_file);
		exit(-1);
	}

	/*
	 * Copy every line, except the config line, from the config
	 * file to the tmp_file.
	 */
	strcpy(config_str, "name=\"sx_cmem\" parent=\"pseudo\" "
		"cmem_mbreq=");
	while (fgets(s, BUFSIZE, infp) != NULL) {
		if (strncmp(s, config_str, strlen(config_str)) == 0) {
			sscanf(s, "name=\"sx_cmem\" parent=\"pseudo\" "
				"cmem_mbreq=%d cmem_frag=%d "
				"cmem_mbleft=%d;", &cmem_mbreq,
				&cmem_frag, &cmem_mbleft);
		} else
			fputs(s, outfp);
	}


	/*
	 * Take actions based on the parsed result.
	 */

	/* Report current system configuration and then exit. */
	if (cflg) {
		fd = open(DEVNAME, O_RDONLY);
		if (fd == -1) {
			/* /dev/sx_cmem not existing. Report requests. */
			printf("The sx_cmem driver is not loaded "
				"because no\n");
			printf("contiguous memory has ever been "
				"requested or\n");
			printf("was requested but failed. ");
			printf("The current requests are: \n");
			printf("\tContiguous memory: %dMB\n",
				cmem_mbreq);
			printf("\tMemory for system use: %dMB\n",
				cmem_mbleft);
			if (cmem_frag)
				printf("\tFragmentation allowed: Yes\n");
			else
				printf("\tFragmentation allowed: No\n");
			cleanup();
			exit(5);
		}

		/* Probe the driver for the real configuration. */
		if (ioctl(fd, SX_CMEM_GET_CONFIG, &arg) == -1) {
			fprintf(stderr, "SX_CMEM_GET_CONFIG ioctl "
				"failed.\n");
			cleanup();
			exit(-1);
		}

		/*
		 * Report the configuration of the sytem.
		 */
		printf("Physical memory installed: %dMB\n",
			arg.scm_meminstalled);
		printf("Contiguous memory requested: %dMB; "
			"actually reserved: %dMB\n",
			arg.scm_cmem_mbreq, arg.scm_cmem_mbrsv);
		printf("  Number of chunks: %d\n",
			arg.scm_cmem_chunks);
		for (i = 0; i < SX_CMEM_CHNK_NUM; i++) {
			if (arg.scm_cmem_chunksz[i] != 0) {
				printf("    Size Of ");
				printf("Chunk(%d): %dMB\n",
					(i + 1), arg.scm_cmem_chunksz[i]);
			}
		}
		printf("Memory for system use requested: %dMB; "
			"actually reserved: %dMB\n",
			arg.scm_cmem_mbleftreq, arg.scm_cmem_mbleft);
		if (arg.scm_cmem_frag)
			printf("Fragmentation allowed: Yes\n");
		else
			printf("Fragmentation allowed: No\n");

		cleanup();
		exit(0);
	}


	/*
	 * The rest options will write to the configuration file.
	 * Make sure the configuration file is writable.
	 */
	if (access(confname, W_OK) != 0) {
		fprintf(stderr, "Write permission denied on %s\n",
			confname);
		cleanup();
		exit(6);
	}


	/* If -d entered, set all requests to the default values. */
	if (dflg) {
		cmem_mbreq = DFLT_CMEM_MBREQ;
		cmem_frag = DFLT_CMEM_FRAG;
		cmem_mbleft = DFLT_CMEM_MBLEFT;
	}


	/*
	 * If -s, -l, -f or -n options entered, set the requests
	 * accordingly.
	 */
	if (input_mbreq != -1)
		cmem_mbreq = input_mbreq;

	if (input_mbleft != -1)
		cmem_mbleft = input_mbleft;

	if (input_frag != -1)
		cmem_frag = input_frag;

	/*
	 * Fail if (phys_mem_installed - cmem_mbreq) < cmem_mbleft.
	 */
	phys_mem_pages = sysconf(_SC_PHYS_PAGES);
	page_size = sysconf(_SC_PAGESIZE);
	phys_mem_installed = (phys_mem_pages * page_size) / ONE_MBYTE;
	if ((cmem_mbreq + cmem_mbleft) > phys_mem_installed) {
		fprintf(stderr, "Error: The request exceeds the system "
			"limit.\n");
		fprintf(stderr, "\tPhysical memory installed: %ldMB\n",
			phys_mem_installed);
		fprintf(stderr, "\tContiguous memory requested: %ldMB\n",
			cmem_mbreq);
		fprintf(stderr, "\tMemory for system use requested: "
			"%ldMB\n", cmem_mbleft);
		cleanup();
		exit(7);
	}


	/*
	 * If after cmem reservation, the amount of memory left for
	 * system use will be less than the default value, give
	 * user a warning and continue..
	 */
	if (cmem_mbleft < DFLT_CMEM_MBLEFT) {
		fprintf(stderr, "WARNING: The amount of memory left "
			"for system use is less than %ldMB.\n",
			DFLT_CMEM_MBLEFT);
	}


	/*
	 * Make the tmp_file complete by appending the
	 * configuration line to it.
	 */
	sprintf(s, "name=\"sx_cmem\" parent=\"pseudo\" "
		"cmem_mbreq=%d cmem_frag=%d cmem_mbleft=%d;\n",
		cmem_mbreq, cmem_frag, cmem_mbleft);
	fputs(s, outfp);

	/*
	 * Flush the write buffer to the tmp_file.
	 */
	fclose(outfp);

	/*
	 * Overwrite the config file with the resultant tmp_file.
	 */

	sprintf(cp_str, "/usr/bin/cp %s %s", tmp_file, confname);
	status = system(cp_str);
	if (status == -1) {
		fprintf(stderr, "system: Could not issue a shell "
			"command. \n");
		cleanup();
		exit(-1);
	}

	if (WIFEXITED(status) && (WEXITSTATUS(status) != 0)) {
		fprintf(stderr, "Could not copy %s to %s.\n",
			tmp_file, confname);
		cleanup();
		exit(-1);
	}

	/*
	 * If this is the first time the command is used,
	 * the sx_cmem driver has not been loaded and the link
	 * /dev/sx_cmem has not been made yet. Create
	 * /reconfigure file to force a reconfiguration boot on
	 * next boot. Just so users won't need to reboot with -r.
	 */
	if (stat(DEVNAME, &buf) == -1) {
		if (creat(RECONFIGURE, S_IRWXU) == -1) {
			fprintf(stderr, "Can not create %s.\n",
				RECONFIGURE);
			cleanup();
			exit(-1);
		}
	}


	/*
	 * Tell user that a reboot is necessary for the changes to
	 * take effect.
	 */
	printf("The system must be rebooted for the changes "
		"to take effect.\n");

	cleanup();
	exit(0);
}
