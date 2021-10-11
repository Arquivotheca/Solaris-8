/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#ident	"@(#)mntinfo.c	1.21	98/07/27 SMI"

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <signal.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/systeminfo.h>
#include <pkgstrct.h>
#include <pkginfo.h>
#include <locale.h>
#include <libintl.h>
#include <pkglib.h>
#include "install.h"
#include "libinst.h"
#include "libadm.h"

#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/vfstab.h>

extern char **environ;

static int match_mount;		/* This holds the mount of interest. */

int	fs_tab_used  = 0;
int	fs_tab_alloc = 0;
static int	fs_list = -1;

struct	fstable	**fs_tab = NULL;

#define	PKGDBROOT	"/var/sadm"
#define	MOUNT		"/sbin/mount"
#define	UMOUNT		"/sbin/umount"

#define	WRN_FREOPEN		"WARNING: freopen(%s, \"%s\", %s) failed, " \
				"errno=%d"
#define	WRN_BAD_FORK		"WARNING: bad fork(), errno=%d"
#define	WRN_BAD_WAIT		"WARNING: wait for %d failed, pid=%d errno=%d"

#define	ERR_NOTABLE		"unable to open %s table %s"
#define	ERR_MALLOC		"malloc(%s) failed errno=%d"
#define	ERR_FELLOUT		"fsys(): fell out of loop looking for <%s>"
#define	ERR_FSTAB_MALLOC	"malloc(fs_tab) failed errno=%d"
#define	ERR_FSTAB_REALLOC	"realloc(fs_tab) failed errno=%d"
#define	ERR_NOROOT		"get_mntinfo() identified <%s> as root file " \
				"system instead of <%s> errno %d."
#define	ERR_NOMOUNTS		"get_mntinfo() could find no filesystems"

#define	WRN_FSTAB_MOUNT		"WARNING: unable to mount client's " \
				"filesystem at %s - errno=%d."
#define	WRN_FSTAB_UMOUNT	"WARNING: unable to unmount client's " \
				"filesystem at %s - errno=%d."

#define	setmntent	fopen
#define	endmntent	fclose
#define	MOUNT_TABLE	MNTTAB

/* returned by already_mounted() */
#define	MNT_NOT		0
#define	MNT_EXACT	1
#define	MNT_AVAIL	2

/* used with is_remote_src() */
#define	NOT_REMOTE	0
#define	REAL_REMOTE	1
#define	SELF_SERVE	2

/* Due to /etc/mnttab files containing entries for multiple nfs hosts
 * HOST_NM_LN needs to be accommodating. The recommended value in the sysinfo
 * man page of 257 needs to be expanded. See bugid 4076513.
 * 1024 chars is defined in the mnttab.h header as the max size of an entry.
*/

#define	HOST_NM_LN	MNT_LINE_MAX

/* These cachefs definitions should be in mntent.h. Maybe some day. */
#define	MNTTYPE_CFS		"cachefs"
#define	MNTOPT_BACKFSTYPE	"backfstype"
#define	MNTTYPE_AUTO		"autofs"

/*
 * Utilities for getting filesystem information from the mount table.
 *
 * Note: vanilla SVr4 code (pkginstall/dockspace.c) used the output from
 * popen() on the "/etc/mount" command.  However, we need to get more
 * information about mounted filesystems, so we use the C interfaces to
 * the mount table, which also happens to be much faster than running
 * another process.  Since several of the pkg commands need access to the
 * the code has been placed here, to be included in the libinst library.
 */

#define	ALLOC_CHUNK	30

/*
 * fs_tab_ent_comp -	compare fstable entries first by length in reverse
 *			order, then alphabetically.
 */
static int
fs_tab_ent_comp(const void *e1, const void *e2)
{
	struct fstable	*fs1 = *((struct fstable **) e1);
	struct fstable	*fs2 = *((struct fstable **) e2);

	if (fs1->namlen == fs2->namlen)
		return (strcmp(fs1->name, fs2->name));
	else
		return (fs2->namlen - fs1->namlen);
}

/*
 * This determines if the source of the mount is from another host. If it's
 * from this host, then it might be writable. This returns NOT_REMOTE if it's
 * pure local, REAL_REMOTE if it's being served from another host and
 * SELF_SERVE if it's being served by the current host.
 */
static int
is_remote_src(char *source)
{
	static char host_name[HOST_NM_LN];
	char source_host[HOST_NM_LN], *src_ptr, *src_host_ptr;
	static int hn_len;

	if (hn_len == 0) {
		/* Find out what host this is. */
		sysinfo(SI_HOSTNAME, host_name, HOST_NM_LN);
		hn_len = strlen(host_name);
	}

	if (source[0] == '/')
		return (NOT_REMOTE);	/* No server name, so it's local. */

	if (strchr(source, ':') == NULL)
		return (NOT_REMOTE);	/* it's a floppy disk or something */

	src_ptr = source;
	src_host_ptr = source_host;

	/* Scan to the end of the hostname (find the ":"). */
	while (*src_ptr != ':')
		*src_host_ptr++ = *src_ptr++;
	*src_host_ptr = '\0';

	if (strncmp(source, host_name, hn_len) == 0 &&
	    *(source+hn_len) == ':' || is_local_host(source_host))
		return (SELF_SERVE);	/* Exporting from itself, it's local. */

	return (REAL_REMOTE);
}

/*
 * This determines if an apparently writeable filesystem is really writeable
 * or if it's been shared over the network with root-restrictive options.
 */
static int
really_write(char *mountpt)
{
	char testfile[PATH_MAX];
	int fd, retval = 0;
	struct stat status;

	sprintf(testfile, "%s/testXXXXXX", mountpt);

	if (mktemp(testfile) == NULL)
		return (0);	/* may as well be read-only */
	else if ((fd = creat(testfile, 0777)) == -1)
		return (0);	/* can't write */
	else if (fstat(fd, &status) == -1)
		retval = 0;	/* may as well be read-only */
	else if (status.st_uid != 0)
		retval = 0;	/* too many restrictions */
	else
		retval = 1;

	close(fd);
	unlink(testfile);

	return (retval);
}

/* This returns the hostname portion of a remote path. */
char *
get_server_host(short n)
{
	static char hostname[HOST_NM_LN], *host_end;

	if (n >= 0 && n < fs_tab_used) {
		strcpy(hostname, fs_tab[n]->remote_name);
		if ((host_end = strchr(hostname, ':')) == NULL) {
			if ((strcmp(fs_tab[n]->fstype, MNTTYPE_AUTO)) == NULL)
				return ("automounter");
			else
				return (fs_tab[n]->fstype);
		} else {
			*host_end = '\0';
			return (hostname);
		}
	}

	return ("unknown source");
}

/*
 * This pulls the path out of a hostpath which may be of the form host:path
 * where path is an absolute path. NOTE: If path turns out to be relative,
 * this returns NULL.
 */
static char *
path_part(char *hostpath)
{
	char *host_end;

	if ((host_end = strchr(hostpath, ':')) == NULL && hostpath[0] == '/')
		return (hostpath);	/* It's already legit. */

	if (*(host_end+1) == '/')
		return (host_end+1);	/* Here's the path part. */

	return (NULL);
}

/*
 * This scans the filesystems already mounted to see if this remote mount is
 * already in place on the server. This scans the fs_tab for a remote_name
 * exactly matching the client's. It stores the current entry number
 * corresponding to this mount in the static match_mount.
 *
 * Returns:
 *	MNT_NOT		Couldn't find it.
 *	MNT_EXACT	This has actually been manually mounted for us
 *	MNT_AVAIL	This is mounted for the server, but needs to be
 *			loopback mounted from the client's perspective.
 */
static int
already_mounted(struct vfstab *vfs, int is_local_host, char *client_path,
    char *host_path)
{
	int i;

	match_mount = -1;

	for (i = 0; i < fs_tab_used; i++) {
		/*
		 * Determine if this has been manually mounted exactly as we
		 * require. Begin by finding a mount on our current
		 * mountpoint.
		 */
		if (strcmp(fs_tab[i]->name, client_path) == 0) {
			/*
			 * Now see if it is really the same mount. This isn't
			 * smart enough to find mounts on top of mounts, but
			 * assuming there is no conspiracy to fool this
			 * function, it will be good enough.
			 */
			if (is_local_host &&
			    strcmp(fs_tab[i]->remote_name, host_path) == 0) {
				match_mount = i;
				return (MNT_EXACT);
			}
		}

		/* Determine if this mount is available to the server. */
		if (strcmp(fs_tab[i]->remote_name, vfs->vfs_special) == 0) {
			match_mount = i;
			return (MNT_AVAIL);
		}
	}
	return (MNT_NOT);
}

/*
 * This function unmounts all of the loopback mounts created for the client.
 * If no client stuff is mounted, this is completely benign, it finds that
 * nothing is mounted up and returns. It returns "1" for unmounted everything
 * OK and "0" for failure.
 */
int
unmount_client()
{
	int n, pid, pid_return, exit_no, status;
	int retcode = 1;
	int errcode;

	for (n = 0; n < fs_tab_used-1; n++) {
		/* If the filesystem is mounted and this utility did it ... */
		if (fs_tab[n]->cl_mounted && fs_tab[n]->srvr_map) {
			pid = fork();
			if (pid < 0) {
				logerr(gettext(WRN_BAD_FORK), errno);
				retcode = 0;
			} else if (pid) {
				/* parent process */
				pid_return = waitpid(pid, &status, 0);

				if (pid_return != pid) {
					logerr(gettext(WRN_BAD_WAIT),
					    pid, pid_return, errno);
					retcode = 0;
				}

				/*
				 * If the child was stopped or killed by a
				 * signal or exied with any code but 0, we
				 * assume the mount has failed.
				 */
				if (!WIFEXITED(status) ||
				    (errcode = WEXITSTATUS(status))) {
					retcode = 0;
					logerr(gettext(WRN_FSTAB_UMOUNT),
					    fs_tab[n]->name, errcode);
				} else {
					fs_tab[n]->cl_mounted = 0;
				}
			} else {
				/* child process */
				char *arg[3] = {
					UMOUNT, NULL, NULL
				};

				arg[1] = strdup(fs_tab[n]->name);

				/*
				 * Redirect output to /dev/null because the
				 * umount error message may be confusing to
				 * the user.
				 */
				if (freopen("/dev/null", "a", stderr) == NULL) {
					logerr(gettext(WRN_FREOPEN),
					    "/dev/null", "a", "stderr", errno);
				}

				exit_no = execve(arg[0], arg, environ);
				_exit(exit_no);
			}
		}
	}

	return (retcode);
}

/*
 * This function creates the necessary loopback mounts to emulate the client
 * configuration with respect to the server. If this is being run on a
 * standalone or the installation is actually to the local system, this call
 * is benign since srvr_map won't be set anywhere. It returns "1" for mounted
 * everything OK and "0" for failure.
 */
int
mount_client()
{
	int n, pid, pid_return, exit_no, status;
	int retcode = 1;
	int errcode;

	for (n = fs_tab_used-1; n >= 0; n--) {
		/*
		 * If the filesystem is mounted (meaning available) and the
		 * apparent filesystem can be mapped to a local filesystem
		 * AND the local filesystem is not the same as the target
		 * filesystem, mount it.
		 */
		if (fs_tab[n]->mounted && fs_tab[n]->srvr_map) {
			pid = fork();
			if (pid < 0) {
				logerr(gettext(WRN_BAD_FORK), errno);
				retcode = 0;
			} else if (pid) {
				/* parent process */
				pid_return = waitpid(pid, &status, 0);

				if (pid_return != pid) {
					logerr(gettext(WRN_BAD_WAIT),
					    pid, pid_return, errno);
					retcode = 0;
				}

				/*
				 * If the child was stopped or killed by a
				 * signal or exied with any code but 0, we
				 * assume the mount has failed.
				 */
				if (!WIFEXITED(status) ||
				    (errcode = WEXITSTATUS(status))) {
					retcode = 0;
					fs_tab[n]->mnt_failed = 1;
					logerr(gettext(WRN_FSTAB_MOUNT),
					    fs_tab[n]->name, errcode);
				} else {
					fs_tab[n]->cl_mounted = 1;
				}
			} else {
				/* child process */
				char *arg[6] = {
					MOUNT, "-F", "lofs", NULL, NULL, NULL
				};

				arg[3] = strdup(fs_tab[n]->remote_name);
				arg[4] = strdup(fs_tab[n]->name);

				/*
				 * Redirect output to /dev/null because the
				 * mount error message may be confusing to
				 * the user.
				 */
				if (freopen("/dev/null", "a", stderr) == NULL) {
					logerr(gettext(WRN_FREOPEN),
					    "/dev/null", "a", "stderr", errno);
				}

				exit_no = execve(arg[0], arg, environ);
				_exit(exit_no);
			}
		}
	}
	return (retcode);
}

/*
 * This function maps path, on a loopback filesystem, back to the real server
 * filesystem. fsys_value is the fs_tab[] entry to which the loopback'd path is
 * mapped. This returns a pointer to a static area. If the result is needed
 * for further processing, it should be strdup()'d or something.
 */
char *
server_map(char *path, short fsys_value)
{
	static char server_construction[PATH_MAX];

	if (fsys_value >= 0 && fsys_value < fs_tab_used)
		sprintf(server_construction, "%s%s",
		    fs_tab[fsys_value]->remote_name,
		    path+strlen(fs_tab[fsys_value]->name));
	else
		strcpy(server_construction, path);

	return (server_construction);
}

/* This function sets up the standard parts of the fs_tab. */
static struct fstable *
fs_tab_init(char *mountp, char *fstype)
{
	struct fstable *nfte;

	/* Create the array if necessary. */
	if (fs_list == -1) {
		fs_list = ar_create(ALLOC_CHUNK,
		    (unsigned) sizeof (struct fstable),
		    "filesystem mount data");
		if (fs_list == -1) {
			progerr(gettext(ERR_MALLOC), errno);
			return (NULL);
		}
	}

	/*
	 * Allocate an fstable entry for this mnttab entry.
	 */
	if ((nfte = *(struct fstable **)ar_next_avail(fs_list))
	    == NULL) {
		progerr(gettext(ERR_MALLOC), "nfte", errno);
		return (NULL);
	}

	/*
	 * Get the length of the 'mount point' name.
	 */
	nfte->namlen = strlen(mountp);
	/*
	 * Allocate space for the 'mount point' name.
	 */
	if ((nfte->name = malloc(nfte->namlen+1)) == NULL) {
		progerr(gettext(ERR_MALLOC), "name", errno);
		return (NULL);
	}
	(void) strcpy(nfte->name, mountp);

	if ((nfte->fstype = malloc(strlen(fstype)+1)) == NULL) {
		progerr(gettext(ERR_MALLOC), "fstype", errno);
		return (NULL);
	}
	(void) strcpy(nfte->fstype, fstype);

	fs_tab_used++;

	return (nfte);
}

/* This function frees all memory associated with the filesystem table. */
void
fs_tab_free(void)
{
	int n;

	for (n = 0; n < fs_tab_used; n++) {
		free(fs_tab[n]->fstype);
		free(fs_tab[n]->name);
		free(fs_tab[n]->remote_name);
	}

	ar_free(fs_list);
}

/* This function scans a string of mount options for a specific keyword. */
static int
hasopt(char *options, char *keyword)
{
	char vfs_options[VFS_LINE_MAX], *optptr;

	if (!options)
		strcpy(vfs_options, "ro");
	else
		strcpy(vfs_options, options);

	while (optptr = strrchr(vfs_options, ',')) {
		*optptr++ = '\0';

		if (strcmp(optptr, keyword) == 0)
			return (1);
	}

	/* Now deal with the remainder. */
	if (strcmp(vfs_options, keyword) == 0)
		return (1);

	return (0);
}

/*
 * This function constructs a new filesystem table (fs_tab[]) entry based on
 * an /etc/mnttab entry. When it returns, the new entry has been inserted
 * into fs_tab[].
 */
static int
construct_mt(struct mnttab *mt)
{
	struct	fstable	*nfte;

	/*
	 * Initialize fstable structure and make the standard entries.
	 */
	if ((nfte = fs_tab_init(mt->mnt_mountp, mt->mnt_fstype)) == NULL)
		return (1);

	/* See if this is served from another host. */
	if (is_remote_src(mt->mnt_special) == REAL_REMOTE ||
	    strcmp(mt->mnt_fstype, MNTTYPE_AUTO) == 0)
		nfte->remote = 1;
	else
		nfte->remote = 0;

	/* It's mounted now (by definition), so we don't have to remap it. */
	nfte->srvr_map = 0;
	nfte->mounted = 1;

	nfte->remote_name = strdup(mt->mnt_special);

	/*
	 * This checks the mount commands which establish the most
	 * basic level of access. Later further tests may be
	 * necessary to fully qualify this. We set this bit
	 * preliminarily because we have access to the mount data
	 * now.
	 */
	nfte->writeable = 0;	/* Assume read-only. */
	if (hasmntopt(mt, MNTOPT_RO) == NULL) {
		nfte->writeable = 1;
		if (!(nfte->remote))
			/*
			 * There's no network involved, so this
			 * assessment is confirmed.
			 */
			nfte->write_tested = 1;
	} else
		/* read-only is read-only */
		nfte->write_tested = 1;

	/* Is this coming to us from a server? */
	if (nfte->remote && !(nfte->writeable))
		nfte->served = 1;

	return (0);
}

/*
 * This function modifies an existing fs_tab[] entry. It was found mounted up
 * exactly the way we would have mounted it in mount_client() only at the
 * time we didn't know it was for the client. Now we do, so we're setting the
 * various permissions to conform to the client view.
 */
static void
mod_existing(struct vfstab *vfsent, int fstab_entry, int is_remote)
{
	/*
	 * Establish whether the client will see this as served.
	 */
	if (is_remote && hasopt(vfsent->vfs_mntopts, MNTOPT_RO))
		fs_tab[fstab_entry]->served = 1;

	fs_tab[fstab_entry]->cl_mounted = 1;
}

/*
 * This function constructs a new fs_tab[] entry based on
 * an /etc/vfstab entry. When it returns, the new entry has been inserted
 * into fstab[].
 */
static int
construct_vfs(struct vfstab *vfsent, char *client_path, char *link_name,
    int is_remote, int mnt_stat)
{
	int use_link;
	struct	fstable	*nfte;

	if ((nfte = fs_tab_init(client_path, vfsent->vfs_fstype)) == NULL)
		return (1);

	nfte->remote = (is_remote == REAL_REMOTE);

	/*
	 * The file system mounted on the client may or may not be writeable.
	 * So we hand it over to fsys() to evaluate. This will have the same
	 * read/write attributes as the corresponding mounted filesystem.
	 */
	use_link = 0;
	if (nfte->remote) {
		/*
		 * Deal here with mount points actually on a system remote
		 * from the server.
		 */
		if (mnt_stat == MNT_NOT) {
			/*
			 * This filesystem isn't in the current mount table
			 * meaning it isn't mounted, the current host can't
			 * write to it and there's no point to mapping it for
			 * the server.
			 */
			link_name = NULL;
			nfte->mounted = 0;
			nfte->srvr_map = 0;
			nfte->writeable = 0;
		} else {	/* It's MNT_AVAIL. */
			/*
			 * This filesystem is associated with a current
			 * mountpoint. Since it's mounted, it needs to be
			 * remapped and it is writable if the real mounted
			 * filesystem is writeable.
			 */
			use_link = 1;
			link_name = strdup(fs_tab[match_mount]->name);
			nfte->mounted = 1;
			nfte->srvr_map = 1;
			nfte->writeable = fs_tab[match_mount]->writeable;
			nfte->write_tested = fs_tab[match_mount]->write_tested;
		}
	} else {	/* local filesystem */
		use_link = 1;
		nfte->mounted = 1;
		nfte->srvr_map = 1;
		nfte->writeable = fs_tab[fsys(link_name)]->writeable;
		nfte->write_tested = 1;
	}

	/*
	 * Now we establish whether the client will see this as served.
	 */
	if (is_remote && hasopt(vfsent->vfs_mntopts, MNTOPT_RO))
		nfte->served = 1;

	if (use_link) {
		nfte->remote_name = link_name;
	} else {
		nfte->remote_name = strdup(vfsent->vfs_special);
	}

	return (0);
}

/*
 * get_mntinfo - get the mount table, now dynamically allocated. Returns 0 if
 * no problem and 1 if there's a fatal error.
 */
int
get_mntinfo(int map_client, char *vfstab_file)
{
	static 	char 	*rn = "/";
	FILE		*pp;
	struct	mnttab	mtbuf;
	struct	mnttab	*mt = &mtbuf;
	char		*install_root;
	int 		is_remote;

	/*
	 * Open the mount table for the current host and establish a global
	 * table that holds data about current mount status.
	 */
	if ((pp = setmntent(MOUNT_TABLE, "r")) == NULL) {
		progerr(gettext(ERR_NOTABLE), "mount", MOUNT_TABLE);
		return (1);
	}

	/*
	 * First, review the mounted filesystems on the managing host. This
	 * may also be the target host but we haven't decided that for sure
	 * yet.
	 */
	while (!getmntent(pp, mt))
		if (construct_mt(mt))
			return (1);

	endmntent(pp);

	/* Point fs_tab[] at the head of the array. */
	if ((fs_tab = (struct fstable **)ar_get_head(fs_list)) == NULL) {
		progerr(gettext(ERR_NOTABLE), "mount", MOUNT_TABLE);
		return (1);
	}

	/*
	 * Now, we see if this installation is to a client. If it is, we scan
	 * the client's vfstab to determine what filesystems are
	 * inappropriate to write to. This simply adds the vfstab entries
	 * representing what will be remote file systems for the client.
	 * Everything that isn't remote to the client is already accounted
	 * for in the fs_tab[] so far. If the remote filesystem is really on
	 * this server, we will write through to the server from this client.
	 */
	install_root = get_inst_root();
	if (install_root && strcmp(install_root, "/") != 0 && map_client) {
		/* OK, this is a legitimate remote client. */
		struct	vfstab	vfsbuf;
		struct	vfstab	*vfs = &vfsbuf;
		char VFS_TABLE[PATH_MAX];

		/*
		 * Since we use the fsys() function later, and it depends on
		 * an ordered list, we have to sort the list here.
		 */
		qsort(fs_tab, fs_tab_used,
		    sizeof (struct fstable *), fs_tab_ent_comp);

		/*
		 * Here's where the vfstab for the target is. If we can get
		 * to it, we'll scan it for what the client will see as
		 * remote filesystems, otherwise, we'll just skip this.
		 */
		if (vfstab_file)
			sprintf(VFS_TABLE, "%s", vfstab_file);
		else
			sprintf(VFS_TABLE, "%s%s", install_root, VFSTAB);

		if (access(VFS_TABLE, R_OK) == 0) {
			char *link_name;

			/*
			 * Open the vfs table for the target host.
			 */
			if ((pp = setmntent(VFS_TABLE, "r")) == NULL) {
				progerr(gettext(ERR_NOTABLE), "vfs", VFS_TABLE);
				return (1);
			}

			/* Do this for each entry in the vfstab. */
			while (!getvfsent(pp, vfs)) {
				char client_mountp[PATH_MAX];
				int mnt_stat;

				/*
				 * We put it into the fs table if it's
				 * remote mounted (even from this server) or
				 * loopback mounted from the client's point
				 * of view.
				 */
				if (!(is_remote =
				    is_remote_src(vfs->vfs_special)) &&
				    strcmp(vfs->vfs_fstype, MNTTYPE_LOFS) !=
				    0)
					continue;	/* not interesting */

				/*
				 * Construct client_mountp by prepending the
				 * install_root to the 'mount point' name.
				 */
				if (strcmp(vfs->vfs_mountp, "/") == 0)
					strcpy(client_mountp, install_root);
				else
					sprintf(client_mountp, "%s%s",
					    install_root, vfs->vfs_mountp);

				/*
				 * We also skip the entry if the vfs_special
				 * path and the client_path are the same.
				 * There's no need to mount it, it's just a
				 * cachefs optimization that mounts a
				 * directory over itself from this server.
				 */
				if ((is_remote == SELF_SERVE) &&
				    strcmp(path_part(vfs->vfs_special),
				    client_mountp) == 0)
					continue;

				/* Determine if this is already mounted. */
				link_name = strdup(path_part(vfs->vfs_special));
				mnt_stat = already_mounted(vfs,
				    (is_remote != REAL_REMOTE), client_mountp,
				    link_name);

				if (mnt_stat == MNT_EXACT) {
					mod_existing(vfs, match_mount,
					    is_remote);
				} else {	/* MNT_NOT */
					if (construct_vfs(vfs, client_mountp,
					    link_name, is_remote, mnt_stat))
						return (1);
				}
			}
			endmntent(pp);
		}	/* end of if(access()) */
	}	/* end of if(install_root) */

	/* This next one may look stupid, but it can really happen. */
	if (fs_tab_used <= 0) {
		progerr(gettext(ERR_NOMOUNTS));
		return (1);
	}

	/*
	 * Point fs_tab[] at the head of the array again (since it may have
	 * moved).
	 */
	if ((fs_tab = (struct fstable **)ar_get_head(fs_list)) == NULL) {
		progerr(gettext(ERR_NOTABLE), "mount", MOUNT_TABLE);
		return (1);
	}

	/*
	 * Now that we have the complete list of mounted (or virtually
	 * mounted) filesystems, we sort the mountpoints in reverse order
	 * based on the length of the 'mount point' name.
	 */
	qsort(fs_tab, fs_tab_used, sizeof (struct fstable *), fs_tab_ent_comp);
	if (strcmp(fs_tab[fs_tab_used-1]->name, rn) != 0) {
		progerr(gettext(ERR_NOROOT), fs_tab[fs_tab_used-1]->name,
		    rn, errno);
		return (1);
	} else {
		return (0);
	}
}

/*
 * This function supports dryrun mode by allowing the filesystem table to be
 * directly loaded from the continuation file.
 */
int
load_fsentry(struct fstable *fs_entry, char *name, char *fstype,
    char *remote_name)
{
	struct fstable *nfte;

	if ((nfte = fs_tab_init(name, fstype)) == NULL)
		return (1);

	/* Grab the name and fstype from the new structure. */
	fs_entry->name = nfte->name;
	fs_entry->fstype = nfte->fstype;

	/* Copy the basic structure into place. */
	memcpy(nfte, fs_entry, sizeof (struct fstable));

	/*
	 * Allocate space for the 'special' name.
	 */
	if ((nfte->remote_name = malloc(strlen(remote_name)+1)) == NULL) {
		progerr(gettext(ERR_MALLOC), "remote_name", errno);
		return (1);
	}

	(void) strcpy(nfte->remote_name, remote_name);

	/* Point fs_tab[] at the head of the array. */
	if ((fs_tab = (struct fstable **)ar_get_head(fs_list)) == NULL) {
		progerr(gettext(ERR_NOTABLE), "mount", MOUNT_TABLE);
		return (1);
	}

	return (0);
}

/*
 * Given a path, return the table index of the filesystem the file apparently
 * resides on. This doesn't put any time into resolving filesystems that
 * refer to other filesystems. It just returns the entry containing this
 * path.
 */
short
fsys(char *path)
{
	register int i;
	char	real_path[PATH_MAX];
	char	*path2use = NULL;
	char	*cp = NULL;
	int	pathlen;

	path2use = path;

	real_path[0] = '\0';

	(void) realpath(path, real_path);

	if (real_path[0]) {
		cp = strstr(path, real_path);
		if (cp != path || cp == NULL)
			path2use = real_path;
	}

	pathlen = strlen(path2use);

	/*
	 * The following algorithm scans the list of attached file systems
	 * for the one containing path. At this point the file names in
	 * fs_tab[] are sorted by decreasing length to facilitate the scan.
	 * The first for() scans past all the file system names too short to
	 * contain path. The second for() does the actual string comparison.
	 * It tests first to assure that the comparison is against a complete
	 * token by assuring that the end of the filesystem name aligns with
	 * the end of a token in path2use (ie: '/' or NULL) then it does a
	 * string compare. -- JST
	 */
	for (i = 0; i < fs_tab_used; i++)
		if (fs_tab[i] == NULL)
			continue;
		else if (fs_tab[i]->namlen <= pathlen)
			break;
	for (; i < fs_tab_used; i++) {
		int fs_namelen;
		char term_char;

		if (fs_tab[i] == NULL)
			continue;

		fs_namelen = fs_tab[i]->namlen;
		term_char = path2use[fs_namelen];

		/*
		 * If we're putting the file "/a/kernel" into the filesystem
		 * "/a", then fs_namelen == 2 and term_char == '/'. If, we're
		 * putting "/etc/termcap" into "/", fs_namelen == 1 and
		 * term_char (unfortunately) == 'e'. In the case of
		 * fs_namelen == 1, we check to make sure the filesystem is
		 * "/" and if it is, we have a guaranteed fit, otherwise we
		 * do the string compare. -- JST
		 */
		if ((fs_namelen == 1 && *(fs_tab[i]->name) == '/') ||
		    ((term_char == '/' || term_char == NULL) &&
		    strncmp(fs_tab[i]->name, path2use, fs_namelen) == 0))
			return (i);
	}

	/*
	 * It only gets here if the root filesystem is fundamentally corrupt.
	 * (This can happen!)
	 */
	progerr(gettext(ERR_FELLOUT), path2use);

	return (-1);
}

/*
 * This function returns the entry in the fs_tab[] corresponding to the
 * actual filesystem of record. It won't return a loopback filesystem entry,
 * it will return the filesystem that the loopback filesystem is mounted
 * over.
 */
short
resolved_fsys(char *path)
{
	int i = -1;
	char path2use[PATH_MAX];

	strcpy(path2use, path);

	/* If this isn't a "real" filesystem, resolve the map. */
	do {
		strcpy(path2use, server_map(path2use, i));
		i = fsys(path2use);
	} while (fs_tab[i]->srvr_map);

	return (i);
}

/*
 * This function returns the srvr_map status based upon the fs_tab entry
 * number. This tells us if the server path constructed from the package
 * install root is really the target filesystem.
 */
int
use_srvr_map_n(short n)
{
	return ((int)fs_tab[n]->srvr_map);
}

/*
 * This function returns the mount status based upon the fs_tab entry
 * number. This tells us if there is any hope of gaining access
 * to this file system.
 */
int
is_mounted_n(short n)
{
	return ((int)fs_tab[n]->mounted);
}

/*
 * is_fs_writeable_n - given an fstab index, return 1
 *	if it's writeable, 0 if read-only.
 */
int
is_fs_writeable_n(short n)
{
	/*
	 * If the write access permissions haven't been confirmed, do that
	 * now. Note that the only reason we need to do the special check is
	 * in the case of an NFS mount (remote) because we can't determine if
	 * root has access in any other way.
	 */
	if (fs_tab[n]->remote && fs_tab[n]->mounted &&
	    !fs_tab[n]->write_tested) {
		if (fs_tab[n]->writeable && !really_write(fs_tab[n]->name))
			fs_tab[n]->writeable = 0;	/* not really */

		fs_tab[n]->write_tested = 1;	/* confirmed */
	}

	return ((int)fs_tab[n]->writeable);
}

/*
 * is_remote_fs_n - given an fstab index, return 1
 *	if it's a remote filesystem, 0 if local.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
int
is_remote_fs_n(short n)
{
	return ((int)fs_tab[n]->remote);
}

/* index-driven is_served() */
int
is_served_n(short n)
{
	return ((int)fs_tab[n]->served);
}

/*
 * This returns the number of blocks available on the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
u_long
get_blk_free_n(short n)
{
	return (fs_tab[n]->bfree);
}

/*
 * This returns the number of blocks being used on the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
u_long
get_blk_used_n(short n)
{
	return (fs_tab[n]->bused);
}

/*
 * This returns the number of inodes available on the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
u_long
get_inode_free_n(short n)
{
	return (fs_tab[n]->ffree);
}

/*
 * This returns the number of inodes being used on the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
u_long
get_inode_used_n(short n)
{
	return (fs_tab[n]->fused);
}

/*
 * Sets the number of blocks being used on the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
void
set_blk_used_n(short n, ulong value)
{
	fs_tab[n]->bused = value;
}

/* Get the filesystem block size. */
u_long
get_blk_size_n(short n)
{
	return (fs_tab[n]->bsize);
}

/* Get the filesystem fragment size. */
u_long
get_frag_size_n(short n)
{
	return (fs_tab[n]->bsize);
}

/*
 * This returns the name of the indicated filesystem.
 */
char *
get_fs_name_n(short n)
{
	if (n >= fs_tab_used)
		return (NULL);
	else
		return (fs_tab[n]->name);
}

/*
 * This returns the remote name of the indicated filesystem.
 *
 *	Note: Upon entry, a valid fsys() is required.
 */
char *
get_source_name_n(short n)
{
	return (fs_tab[n]->remote_name);
}

/*
 * This function returns the srvr_map status based upon the path.
 */
int
use_srvr_map(char *path, short *fsys_value)
{
	if (*fsys_value == BADFSYS)
		*fsys_value = fsys(path);

	return (use_srvr_map_n(*fsys_value));
}

/*
 * This function returns the mount status based upon the path.
 */
int
is_mounted(char *path, short *fsys_value)
{
	if (*fsys_value == BADFSYS)
		*fsys_value = fsys(path);

	return (is_mounted_n(*fsys_value));
}

/*
 * is_fs_writeable - given a cfent entry, return 1
 *	if it's writeable, 0 if read-only.
 *
 *	Note: Upon exit, a valid fsys() is guaranteed. This is
 *	an interface requirement.
 */
int
is_fs_writeable(char *path, short *fsys_value)
{
	if (*fsys_value == BADFSYS)
		*fsys_value = fsys(path);

	return (is_fs_writeable_n(*fsys_value));
}

/*
 * is_remote_fs - given a cfent entry, return 1
 *	if it's a remote filesystem, 0 if local.
 *
 *	Also Note: Upon exit, a valid fsys() is guaranteed. This is
 *	an interface requirement.
 */
int
is_remote_fs(char *path, short *fsys_value)
{
	if (*fsys_value == BADFSYS)
		*fsys_value = fsys(path);

	return (is_remote_fs_n(*fsys_value));
}

/*
 * This function returns the served status of the filesystem. Served means a
 * client is getting this file from a server and it is not writeable by the
 * client. It has nothing to do with whether or not this particular operation
 * (eg: pkgadd or pkgrm) will be writing to it.
 */
int
is_served(char *path, short *fsys_value)
{
	if (*fsys_value == BADFSYS)
		*fsys_value = fsys(path);

	return (is_served_n(*fsys_value));
}

/*
 * get_remote_path - given a filesystem table index, return the
 *	path of the filesystem on the remote system.  Otherwise,
 *	return NULL if it's a local filesystem.
 */
char *
get_remote_path(short n)
{
	char	*p;

	if (!is_remote_fs_n(n))
		return (NULL); 	/* local */
	p = strchr(fs_tab[n]->remote_name, ':');
	if (!p)
		p = fs_tab[n]->remote_name; 	/* Loopback */
	else
		p++; 	/* remote */
	return (p);
}

/*
 * get_mount_point - given a filesystem table index, return the
 *	path of the mount point.  Otherwise,
 *	return NULL if it's a local filesystem.
 */
char *
get_mount_point(short n)
{
	if (!is_remote_fs_n(n))
		return (NULL); 	/* local */
	return (fs_tab[n]->name);
}

struct fstable *
get_fs_entry(short n)
{
	if (n >= fs_tab_used)
		return (NULL);
	else
		return (fs_tab[n]);
}
