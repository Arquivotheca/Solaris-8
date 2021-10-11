/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modunload.c	1.12	98/07/29 SMI"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <stdio.h>
#include <sys/modctl.h>

/*
 * Unload a loaded module.
 */
main(argc, argv, envp)
	int argc;
	char *argv[];
	char *envp[];
{
	int child;
	int status;
	int id;
	char *ap;
	char *execfile = NULL;
	int opt;
	extern int optind;
	extern char *optarg;

	extern errno;

	if (argc < 3)
		usage();

	while ((opt = getopt(argc, argv, "i:e:")) != -1) {
		switch (opt) {
		case 'i':
			if (sscanf(optarg, "%d", &id) != 1)
				fatal("Invalid id %s\n", optarg);
			break;
		case 'e':
			execfile = optarg;
		}
	}

	if (execfile) {
		child = fork();
		if (child == -1)
			error("can't fork %s", execfile);
		else if (child == 0)
			exec_userfile(execfile, id, envp);
		else {
			wait(&status);
			if (status != 0) {
				printf("%s returned error %d.\n",
				    execfile, status);
				exit(status >> 8);
			}
		}
	}

	/*
	 * Unload the module.
	 */
	if (modctl(MODUNLOAD, id) < 0) {
	    if (errno == EPERM)
		fatal("You must be superuser to unload a module\n");
	    else if (id != 0)
		error("can't unload the module");
	}

	exit(0);			/* success */
}

/*
 * exec the user file.
 */
exec_userfile(execfile, id, envp)
	char *execfile;
	int id;
	char **envp;
{
	struct modinfo modinfo;

	char modid[8];
	char mod0[8];

	modinfo.mi_id = modinfo.mi_nextid = id;
	modinfo.mi_info = MI_INFO_ONE;
	if (modctl(MODINFO, id, &modinfo) < 0)
		error("can't get module information");

	sprintf(modid, "%d", id);
	sprintf(mod0, "%d", modinfo.mi_msinfo[0].msi_p0);

	execle(execfile, execfile, modid, mod0, NULL, envp);

	error("couldn't exec %s\n", execfile);
}

usage()
{
	fatal("usage:  modunload -i <module_id> [-e <exec_file>]\n");
}
