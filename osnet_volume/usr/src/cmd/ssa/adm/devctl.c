/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)devctl.c 1.7     98/02/24 SMI"

/*
 * hotplugging device control routines for disk administration
 */


#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<fcntl.h>
#include	<rpc/types.h>
#include	<sys/stat.h>
#include	<sys/devctl.h>
#include	<libdevice.h>
#include	<sys/wait.h>
#include	<sys/param.h>

#include	"common.h"


/*
 * from main comp unit
 */
extern char	*whoami;			/* program name */

/*
 * from our lib
 */
extern char	*get_physical_name(char *);
extern void	destroy_data(char *);
extern char	*alloc_string(char *);


/*
 * forward declarations
 */
static bool_t	okay_to_proceed(char *);
static int	run_external_commands(void);



/*
 * various commands we run
 */
#define	DRVCONFIG_PATH		"/usr/sbin/drvconfig"
#define	DISKS_PATH		"/usr/sbin/disks"
#define	TAPES_PATH		"/usr/sbin/tapes"
#define	DEVLINKS_PATH		"/usr/sbin/devlinks"




void
print_dev_state(char *devname, int state)
{
	(void) printf("\t%s: ", devname);
	if (state & DEVICE_ONLINE) {
		(void) printf("Online");
		if (state & DEVICE_BUSY) {
			(void) printf(" Busy");
		}
		if (state & DEVICE_DOWN) {
			(void) printf(" Down");
		}
	} else {
		if (state & DEVICE_OFFLINE) {
			(void) printf("Offline");
			if (state & DEVICE_DOWN) {
				(void) printf(" Down");
			}
		}
	}
	(void) printf("\n");
}


void
print_bus_state(char *devname, int state)
{
	(void) printf("\t%s: ", devname);
	if (state == BUS_QUIESCED) {
		(void) printf("Quiesced");
	} else if (state == BUS_ACTIVE) {
		(void) printf("Active");
	} else if (state == BUS_SHUTDOWN) {
		(void) printf("Shutdown");
	}
	(void) printf("\n");
}


/*
 * handle a hotplug request for insertion of a single device or a
 *	single bus:
 *
 *	- take device offline
 *	- attempt to quiesce the bus (not always needed/supported)
 *	- wait for user confirmation of insertion
 *	- attempt to unquiesce the bus
 *	- online device
 *	- run drvconfig(1M), disks(1M), tapes(1M), then devlinks(1M)
 *
 * XXX: what about running pseudo(), devlinks(), etc ???
 *
 * return 0 for success
 *
 * XXX NOTE: the "path" in this case must be of another device already
 * on the bus the be added to, or else the /devices name of the ":devctl"
 * node, minus the ":devctl" postfix
 */
int
dev_handle_insert(char *path)
{
	int		exit_code = -1;
	bool_t		run_xcmds = TRUE;
	devctl_hdl_t	dcp = NULL;
	char		*pp = NULL;			/* physical path */



	/* convert supplied name to physical name */
	if ((pp = get_physical_name(path)) == NULL) {
		(void) fprintf(stderr, "%s: Invalid path name (%s)\n", whoami,
		    path);
		goto dun;
	}

	/* acquire device */
	if ((dcp = devctl_device_acquire(pp, DC_EXCL)) == NULL) {
		(void) fprintf(stderr, "%s: can't acquire \"%s\": %s\n",
		    whoami, pp, strerror(errno));
		goto dun;
	}

	/* take device offline */
	if (devctl_device_offline(dcp) != 0) {
		(void) fprintf(stderr, "%s: can't take offline: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

	/* try to quiesce the bus */
	if (devctl_bus_quiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't quiesce \"%s\": %s\n", whoami,
			    path, strerror(errno));
		}
	}

	/* tell user what to do, then wait for them to do it */
	if (!okay_to_proceed(
	    "Bus is ready for the insertion of device(s)\n"
	    "Insert device(s) and reconfigure bus as needed\n"
	    "Press RETURN when ready to continue\n")) {
		run_xcmds = FALSE;
	}

	/* attempt to unquiesce the bus */
	if (devctl_bus_unquiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't unquiesce \"%s\": %s\n",
			    whoami, path, strerror(errno));
		}
	}

	/* online device */
	if (devctl_device_online(dcp) != 0) {
		(void) fprintf(stderr,
		    "%s: can't put device online: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

	/*
	 * if an error has occurred prior to here then skip running
	 * external programs
	 */
	if (run_xcmds) {
		/*
		 * run external commands that will handle removal of this disk
		 */
		exit_code = run_external_commands();
	}

dun:
	/* all done */
	if (dcp != NULL) {
		devctl_release(dcp);
	}
	if (pp != NULL) {
		destroy_data(pp);
	}
	return (exit_code);
}


/*
 * handle a hotplug request for removal of a single device:
 *
 *	- take device offline
 *	- attempt to quiesce the bus (might not be supported)
 *	- wait for user confirmation of removal
 *	- attempt to unquiesce the bus
 *	- online device
 *	- run drvconfig(1M), disks(1M), tapes(1M), then devlinks(1M)
 *
 * XXX: what about running pseudo(), devlinks(), etc ???
 *
 * return 0 for success
 */
int
dev_handle_remove(char *path)
{
	int		exit_code = -1;
	bool_t		run_xcmds = TRUE;
	devctl_hdl_t	dcp = NULL;
	char		*pp = NULL;			/* physical path */



	/* convert supplied name to physical name */
	if ((pp = get_physical_name(path)) == NULL) {
		(void) fprintf(stderr, "%s: Invalid path name (%s)\n", whoami,
		    path);
		goto dun;
	}

	/* acquire device */
	if ((dcp = devctl_device_acquire(pp, DC_EXCL)) == NULL) {
		(void) fprintf(stderr, "%s: can't acquire \"%s\": %s\n",
		    whoami, pp, strerror(errno));
		goto dun;
	}

	/* take device offline */
	if (devctl_device_offline(dcp) != 0) {
		(void) fprintf(stderr, "%s: can't take offline: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

	/* try to quiesce the bus */
	if (devctl_bus_quiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't quiesce \"%s\": %s\n", whoami,
			    path, strerror(errno));
		}
	}

	/* tell user what to do, then wait for them to do it */
	if (!okay_to_proceed(
	    "Bus is ready for the removal of device\n"
	    "Remove device and reconfigure bus as needed\n"
	    "Press RETURN when ready to continue\n")) {
		run_xcmds = FALSE;
	}

	/*
	 * XXX: should we online if the user decided not to proceed ???
	 *	and how about if they *do* proceed ???
	 */

	/* attempt to unquiesce the bus */
	if (devctl_bus_unquiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't unquiesce \"%s\"\n", whoami,
			    path);
		}
	}

	/* online device */
	if (devctl_device_online(dcp) != 0) {
		(void) fprintf(stderr,
		    "%s: can't put device online: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

	/*
	 * if an error has occurred prior to here then skip running
	 * external programs
	 */
	if (run_xcmds) {
		/*
		 * run external commands that will handle removal of this disk
		 */
		exit_code = run_external_commands();
	}

dun:
	/* all done */
	if (dcp != NULL) {
		devctl_release(dcp);
	}
	if (pp != NULL) {
		destroy_data(pp);
	}
	return (exit_code);
}


/*
 * handle a hotplug request for replacement:
 *
 *	- take device offline
 *	- attempt to quiesce the bus (might not be supported)
 *	- wait for user confirmation of replacement
 *	- attempt to unquiesce the bus
 *	- online device
 *
 * NOTE: no need to run external programs when just replacing a disk
 *
 * return 0 for success
 */
int
dev_handle_replace(char *path)
{
	int		exit_code = -1;
	devctl_hdl_t	dcp = NULL;
	char		*pp = NULL;			/* physical path */



	/* convert supplied name to physical name */
	if ((pp = get_physical_name(path)) == NULL) {
		(void) fprintf(stderr, "%s: Invalid path name (%s)\n", whoami,
		    path);
		goto dun;
	}

	/* acquire device */
	if ((dcp = devctl_device_acquire(pp, DC_EXCL)) == NULL) {
		(void) fprintf(stderr, "%s: can't acquire \"%s\": %s\n",
		    whoami, pp, strerror(errno));
		goto dun;
	}

	/* take device offline */
	if (devctl_device_offline(dcp) != 0) {
		(void) fprintf(stderr, "%s: can't take offline: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

	/* try to quiesce the bus */
	if (devctl_bus_quiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't quiesce \"%s\": %s\n", whoami,
			    path, strerror(errno));
		}
	}

	/*
	 * tell user what to do, then wait for them to do it
	 *
	 * note that the same action should be taken whether or not
	 * user wants to proceed, because undoing is the same as
	 * continuing
	 */
	(void) okay_to_proceed(
	    "Bus is ready for the replacement of device\n"
	    "Replace device and reconfigure bus as needed\n"
	    "Press RETURN when ready to continue\n");

	/* attempt to unquiesce the bus */
	if (devctl_bus_unquiesce(dcp) != 0) {
		if (errno != ENOTSUP) {
			(void) fprintf(stderr,
			    "%s: warning: can't unquiesce \"%s\"\n", whoami,
			    path);
		}
	}

	/* online device */
	if (devctl_device_online(dcp) != 0) {
		(void) fprintf(stderr,
		    "%s: can't put device online: \"%s\": %s\n",
		    whoami, path, strerror(errno));
		goto dun;
	}

dun:
	/* all done */
	if (dcp != NULL) {
		devctl_release(dcp);
	}
	if (pp != NULL) {
		destroy_data(pp);
	}
	return (exit_code);
}


/*
 * helper routines
 */

/*
 * prompt user then get response (a RETURN), returning TRUE for "yes"
 *
 * XXX: we *really* should allow user to say no, e.g. by pressing Q or N
 *	(or something)
 */
static bool_t
okay_to_proceed(char *prompt)
{
	bool_t		res = FALSE;
	int		c;


	(void) printf(prompt);
	(void) fflush(stdout);


	while ((c = getchar()) != EOF) {
		if ((c == '\n') || (c == '\r')) {
			res = TRUE;
			break;
		}
	}

	return (res);
}


/*
 * run external commands for handling hotplugging:
 *
 *	- run drvconfig(1M)
 *	- run disks(1M)
 *	- run tapes(1M)
 *	- run devlinks(1M)
 *
 * if any command fails then print a context-sensitive error message
 * and bug out
 *
 * return "exit code" value
 */
static int
run_external_commands(void)
{
	static int	run_cmd(char *path);
	int		exit_val;


	if ((exit_val = run_cmd(DRVCONFIG_PATH)) != 0) {
		(void) fprintf(stderr,
"Manually run \"drvconfig\", \"disks\", \"tapes\", and \"devlinks\"\n");
		goto dun;
	}

	if ((exit_val = run_cmd(DISKS_PATH)) != 0) {
		(void) fprintf(stderr,
		    "Manually run \"disks\", \"tapes\", and \"devlinks\"\n");
		goto dun;
	}

	if ((exit_val = run_cmd(TAPES_PATH)) != 0) {
		(void) fprintf(stderr,
		    "Manually run \"tapes\", and \"devlinks\"\n");
		goto dun;
	}

	if ((exit_val = run_cmd(DEVLINKS_PATH)) != 0) {
		(void) fprintf(stderr, "Manually run \"devlinks\"\n");
		goto dun;
	}

dun:
	return (exit_val);
}


/*
 * run the specified command (with no arguments), returning its exit value
 *
 * redirect stdin and stderr from/to the bit bucket
 */
static int
run_cmd(char *path)
{
	pid_t		cpid;
	int		stat_val;
	int		child_exit_val = -1;


#ifdef	DEBUG
	P_DPRINTF("running \"%s\"\n", path);
#endif
	if ((cpid = fork()) == (pid_t)-1) {
		(void) fprintf(stderr, "%s: problem: can't fork!\n", whoami);
		exit(-1);
	}

	if (cpid == 0) {
		/* the child */
#ifndef	DEBUG
		int		fd;

		/* redirect stdin and stderr to /dev/null */
		if ((fd = open("/dev/null", O_RDWR)) >= 0) {
			(void) dup2(fd, fileno(stderr));
			(void) dup2(fd, fileno(stdin));
		}
#endif

		(void) execl(path, NULL);

#ifdef	DEBUG
		(void) fprintf(stderr, "error: exec of \"%s\" failed: %s\n",
		    path, strerror(errno));
#endif

		/* oh oh -- shouldn't get here! */
		_exit(-1);
	}

	/* the parent -- wait for that darned child */
	if (waitpid(cpid, &stat_val, 0) == cpid) {
#ifdef	DEBUG
		P_DPRINTF("child stat_val = %#x\n", stat_val);
#endif
		if (WIFEXITED(stat_val)) {
#ifdef	DEBUG
			P_DPRINTF("child exit val: %d\n",
			    WEXITSTATUS(stat_val));
#endif
			if (WEXITSTATUS(stat_val) == 0) {
				child_exit_val = 0;
			}
		}
	}

	if (child_exit_val != 0) {
		(void) fprintf(stderr, "%s: error running \"%s\": %s\n",
		    whoami, path, strerror(errno));
	}

	return (child_exit_val);
}
