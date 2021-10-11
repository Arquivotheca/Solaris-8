/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modload.c	1.15	98/11/09 SMI"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/modctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/*
 * Load a module.
 */
main(argc, argv, envp)
	int argc;
	char *argv[];
	char *envp[];
{
	char *execfile = NULL;		/* name of file to exec after loading */
	char *modpath = NULL;
	char *ap;
	int id;
	extern int optind;
	extern char *optarg;
	int opt;
	int use_path = 0;
	char path[1024];

	extern errno;

	if (argc < 2 || argc > 5) {
		usage();
	}

	while ((opt = getopt(argc, argv, "e:p")) != -1) {
		switch (opt) {
		case 'e':
			execfile = optarg;
			break;
		case 'p':
			use_path++;
			break;
		case '?':
			usage();
		}
	}
	modpath = argv[optind];

	if (modpath == NULL) {
		printf("modpath is null\n");
		usage();
	}
	if (!use_path && modpath[0] != '/') {
		if (getcwd(path, 1023 - strlen(modpath)) == NULL)
			fatal("Can't get current directory\n");
		strcat(path, "/");
		strcat(path, modpath);
	} else
		strcpy(path, modpath);

	/*
	 * Load the module.
	 */
	if (modctl(MODLOAD, use_path, path, &id) != 0) {
		if (errno == EPERM)
			fatal("You must be superuser to load a module\n");
		else
			error("can't load module");
	}

	/*
	 * Exec the user's file (if any)
	 */
	if (execfile)
		exec_userfile(execfile, id, envp);

	exit(0);			/* success */
}

/*
 * Exec the user's file
 */
exec_userfile(execfile, id, envp)
	char *execfile;
	int id;
	char **envp;
{
	struct modinfo modinfo;

	int child;
	int status;
	int waitret;
	char module_id[8];
	char mod0[8];
	char mod1[8];

	if ((child = fork()) == -1)
		error("can't fork %s", execfile);

	/*
	 * exec the user program.
	 */
	if (child == 0) {
		modinfo.mi_id = id;
		modinfo.mi_nextid = id;
		modinfo.mi_info = MI_INFO_ONE;
		if (modctl(MODINFO, id, &modinfo) < 0)
			error("can't get module status");

		sprintf(module_id, "%d", modinfo.mi_id);
		sprintf(mod0, "%d", modinfo.mi_msinfo[0].msi_p0);
		execle(execfile, execfile, module_id, mod0, NULL, envp);

		/* Shouldn't get here if execle was successful */

		error("couldn't exec %s", execfile);
	} else {
		do {
			/* wait for exec'd program to finish */
			waitret = wait(&status);
		} while ((waitret != child) && (waitret != -1));

		waitret = (waitret == -1) ? waitret : status;

		if ((waitret & 0377) != 0) {
			/* exited because of a signal */
			printf("'%s' terminated because of signal %d",
			    execfile, (waitret & 0177));
			if (waitret & 0200)
				printf(" and produced a core file\n");
			printf(".\n");
			exit(waitret >> 8);
		} else {
			/* simple termination */
			if (((waitret >> 8) & 0377) != 0) {
				printf("'%s' returned error %d.\n", execfile,
				    (waitret >> 8) & 0377);
				exit(waitret >> 8);
			}
		}
	}
}

usage()
{
	fatal("usage:  modload [-p] [-e <exec_file>] <filename>\n");
}
