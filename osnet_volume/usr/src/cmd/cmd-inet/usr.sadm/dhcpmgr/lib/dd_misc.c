/*
 * Copyright (c) 1998-1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)dd_misc.c	1.12	99/10/12 SMI"

#include <dhcdata.h>
#include <dd_impl.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <libintl.h>
#include <procfs.h>
#include <rpcsvc/nis.h>
#include <malloc.h>
#include <ctype.h>

#include "dd_misc.h"

#define	STARTUP_DIR	"/etc/init.d"
#define	STARTUP_FILE	STARTUP_DIR"/dhcp"
#define	STARTUP_CMD	STARTUP_FILE" start"
#define	SHUTDOWN_CMD	STARTUP_FILE" stop"

static char *startup_links[] = {
	"/etc/rc0.d/K34dhcp", "/etc/rc1.d/K34dhcp",
	"/etc/rc2.d/K34dhcp", "/etc/rc3.d/S34dhcp",
	"/etc/rcS.d/K34dhcp" };
#define	NUM_LINKS	(sizeof (startup_links) / sizeof (startup_links[0]))

#define	PROCFS_DIR	"/proc"

static char *nameservice_list[TBL_NUM_NAMESERVICES] = { "Files", "NIS+" };

/*
 * Free a data store list created by dd_data_stores().
 */
void
dd_free_data_stores(struct data_store **dsl)
{
	int i = 0;

	if (dsl != NULL) {
		for (i = 0; dsl[i] != NULL; ++i) {
			free(dsl[i]->name);
		}
		free(dsl);
	}
}

/*
 * Returns the list of possible data stores for DHCP data.  Files will
 * always be returned, NIS+ as well if the system is configured as a
 * client.  List returned is terminated with a NULL.
 */
struct data_store **
dd_data_stores()
{
	int i;
	struct data_store **dsl = NULL;
	struct data_store **tmpdsl;
	int dslcnt = 0;
	nis_result *nres;

	for (i = 0; i < TBL_NUM_NAMESERVICES; ++i) {
		switch (i) {

		case TBL_NS_NISPLUS:
			/*
			 * Check if we can access NIS+ by looking up the
			 * org_dir directory which we would need to put the
			 * tables in.
			 */
			nres = nis_lookup("org_dir", EXPAND_NAME+FOLLOW_LINKS);
			if ((NIS_RES_STATUS(nres) != NIS_SUCCESS) &&
			    (NIS_RES_STATUS(nres) != NIS_S_SUCCESS)) {
				break;
			}
			/*FALLTHRU*/
		case TBL_NS_UFS:
			/* Files are always possible */
			/*FALLTHRU*/
		default:
			tmpdsl = realloc(dsl,
			    (dslcnt + 1) * sizeof (struct data_store *));
			if (tmpdsl == NULL) {
				break;
			}
			dsl = tmpdsl;
			dsl[dslcnt] = malloc(sizeof (struct data_store));
			if (dsl[dslcnt] == NULL) {
				break;
			}
			dsl[dslcnt]->code = i;
			dsl[dslcnt]->name = strdup(nameservice_list[i]);
			if (dsl[dslcnt]->name == NULL) {
				break;
			}
			++dslcnt;
			break;
		}
	}
	/* Null-terminate the list */
	tmpdsl = realloc(dsl, (dslcnt + 1) * sizeof (struct data_store *));
	if (tmpdsl == NULL) {
		/*
		 * Couldn't null-terminate; return NULL to protect callers
		 * from walking off the end of the list.
		 */
		dd_free_data_stores(dsl);
		return (NULL);
	}
	dsl = tmpdsl;
	dsl[dslcnt] = NULL;

	return (dsl);
}

/*
 * Create links from the rc* directories to the script in /etc/init.d
 */
int
dd_create_links()
{
	int i;
	int serrno;

	for (i = 0; i < NUM_LINKS; ++i) {
		if (link(STARTUP_FILE, startup_links[i]) != 0) {
			if (errno != EEXIST) {
				serrno = errno;
				while (--i >= 0) {
					unlink(startup_links[i]);
				}
				return (serrno);
			}
		}
	}
	return (0);
}

/*
 * Check the links to be sure they exist.
 */
int
dd_check_links()
{
	int i;
	struct stat stbuf;

	for (i = 0; i < NUM_LINKS; ++i) {
		if (stat(startup_links[i], &stbuf) != 0) {
			return (0);
		}
	}
	return (1);
}

/*
 * Delete the links in the rc* directories which point to the init.d script
 */
int
dd_remove_links()
{
	int i;
	int serrno;

	for (i = 0; i < NUM_LINKS; ++i) {
		if (unlink(startup_links[i]) != 0) {
			if (errno != ENOENT) {
				/*
				 * Something really went wrong so let's
				 * try to put things back the way they were.
				 */
				serrno = errno;
				while (--i >= 0) {
					link(STARTUP_FILE, startup_links[i]);
				}
				return (serrno);
			}
		}
	}
	return (0);
}

/*
 * Start up the daemon.  Just executes /etc/init.d/dhcp start.
 */
int
dd_startup(boolean_t start)
{
	int ret;

	if (start) {
		ret = system(STARTUP_CMD);
	} else {
		ret = system(SHUTDOWN_CMD);
	}
	if (ret != 0) {
		return (errno);
	} else {
		return (0);
	}
}

/*
 * Send a signal to a process whose command name is as specified
 */
int
dd_signal(char *fname, int sig)
{
	pid_t pid;

	pid = dd_getpid(fname);
	if (pid == (pid_t)-1) {
		return (-1);
	}

	if (kill(pid, sig) != 0) {
		return (errno);
	} else {
		return (0);
	}
}

/*
 * Return a process's pid
 */
pid_t
dd_getpid(char *fname)
{
	DIR *dirptr;
	dirent_t *direntptr;
	psinfo_t psinfo;
	int proc_fd;
	char buf[MAXPATHLEN];
	pid_t retval = (pid_t)-1;

	/*
	 * Read entries in /proc, each one is in turn a directory
	 * containing files relating to the process's state.  We read
	 * the psinfo file to get the command name.
	 */
	dirptr = opendir(PROCFS_DIR);
	if (dirptr == (DIR *) NULL) {
		return (retval);
	}
	while ((direntptr = readdir(dirptr)) != NULL) {
		sprintf(buf, PROCFS_DIR"/%s/psinfo", direntptr->d_name);
		if ((proc_fd = open(buf, O_RDONLY)) < 0) {
			continue;	/* skip this one */
		}
		if (read(proc_fd, &psinfo, sizeof (psinfo)) > 0) {
			if (strncmp(psinfo.pr_fname, fname, PRFNSZ) == 0) {
				retval = psinfo.pr_pid;
				close(proc_fd);
				break;
			}
		}
		close(proc_fd);
	}
	closedir(dirptr);
	return (retval);
}


/*
 * Get list of physical, non-loopback interfaces for the system.  Those are
 * the ones in.dhcpd will support.
 */
struct ip_interface **
dd_get_interfaces()
{
	int s;
	struct ifconf ifc;
	int num_ifs;
	int i;
	struct ifreq *ifr;
	struct ip_interface **ret = NULL;
	struct ip_interface **tmpret;
	int retcnt = 0;
	struct sockaddr_in *sin;

	/*
	 * Open socket, needed for doing the ioctls.  Then get number of
	 * interfaces so we know how much memory to allocate, then get
	 * all the interface configurations.
	 */
	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ioctl(s, SIOCGIFNUM, &num_ifs) < 0) {
		(void) close(s);
		return (NULL);
	}
	ifc.ifc_len = num_ifs * sizeof (struct ifreq);
	ifc.ifc_buf = malloc(ifc.ifc_len);
	if (ifc.ifc_buf == NULL) {
		(void) close(s);
		return (NULL);
	}
	if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
		free(ifc.ifc_buf);
		(void) close(s);
		return (NULL);
	}

	/*
	 * For each interface, stuff its name, address and netmask into the
	 * structure that we return.  Filter out loopback and virtual
	 * interfaces as they are of no interest for DHCP.
	 */
	for (i = 0, ifr = ifc.ifc_req; i < num_ifs; ++i, ++ifr) {
		if (strchr(ifr->ifr_name, ':') != NULL) {
			continue;	/* Ignore a virtual interface */
		}
		if (ioctl(s, SIOCGIFFLAGS, ifr) < 0) {
			continue;	/* Can't get flags? Ignore it. */
		}

		if ((ifr->ifr_flags & IFF_LOOPBACK) ||
		    !(ifr->ifr_flags & IFF_UP)) {
			continue;	/* Ignore if loopback or down */
		}
		/* Get more space to store this in */
		tmpret = realloc(ret,
		    (retcnt+1)*sizeof (struct ip_interface *));
		if (tmpret == NULL) {
			while (retcnt-- > 0)
				free(ret[retcnt]);
			free(ret);
			free(ifc.ifc_buf);
			(void) close(s);
			return (NULL);
		}
		ret = tmpret;
		ret[retcnt] = malloc(sizeof (struct ip_interface));
		if (ret[retcnt] == NULL) {
			while (retcnt-- > 0)
				free(ret[retcnt]);
			free(ret);
			free(ifc.ifc_buf);
			(void) close(s);
			return (NULL);
		}
		strcpy(ret[retcnt]->name, ifr->ifr_name);
		if (ioctl(s, SIOCGIFADDR, ifr) < 0) {
			(void) close(s);
			while (retcnt-- > 0) {
				free(ret[retcnt]);
			}
			free(ret);
			free(ifc.ifc_buf);
			return (NULL);
		}
		/*LINTED - alignment*/
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ret[retcnt]->addr = sin->sin_addr;

		if (ioctl(s, SIOCGIFNETMASK, ifr) < 0) {
			(void) close(s);
			while (retcnt-- > 0) {
				free(ret[retcnt]);
			}
			free(ret);
			free(ifc.ifc_buf);
			return (NULL);
		}
		/*LINTED - alignment*/
		sin = (struct sockaddr_in *)&ifr->ifr_addr;
		ret[retcnt]->mask = sin->sin_addr;
		++retcnt;
	}

	/* Null-terminate the list */
	if (retcnt > 0) {
		tmpret = realloc(ret,
		    (retcnt+1)*sizeof (struct ip_interface *));
		if (tmpret == NULL) {
			while (retcnt-- > 0)
				free(ret[retcnt]);
			free(ret);
			free(ifc.ifc_buf);
			(void) close(s);
			return (NULL);
		}
		ret = tmpret;
		ret[retcnt] = NULL;
	}
	(void) close(s);
	free(ifc.ifc_buf);
	return (ret);
}
