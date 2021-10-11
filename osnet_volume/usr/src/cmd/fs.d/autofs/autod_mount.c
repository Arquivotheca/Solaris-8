/*
 *	autod_mount.c
 *
 *	Copyright (c) 1988-1996,1999 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma	ident	"@(#)autod_mount.c 1.61     99/10/07 SMI"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <pwd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/tiuser.h>
#include <locale.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <nfs/nfs.h>
#include <thread.h>
#include <limits.h>
#include <assert.h>
#include <fcntl.h>

#include "automount.h"
#include "replica.h"

extern (*getmapent_fn)(char *, char *, char *, uid_t,
	bool_t, getmapent_fn_res *);

static int unmount_mntpnt(struct mnttab *);
static int fork_exec(char *, char *, char **, int);
static int lofs_match(struct extmnttab *mnt, struct umntrequest *ur);
static void remove_browse_options(char *);

int
do_mount1(mapname, key, subdir, mapopts, path, isdirect, alpp, cred)
	char *mapname;
	char *key;
	char *subdir;
	char *mapopts;
	char *path;
	uint_t isdirect;
	struct action_list **alpp;
	struct authunix_parms *cred;
{
	struct mapline ml;
	struct mapent *me, *mapents = NULL;
	getmapent_fn_res fnres;
	char mntpnt[MAXPATHLEN];
	char spec_mntpnt[MAXPATHLEN];
	int err = 0;
	char *private;	/* fs specific data. eg prevhost in case of nfs */
	int mount_ok = 0;
	int len;
	action_list *alp, *prev, *tmp;
	char root[MAXPATHLEN];
	int overlay = 1;
	char next_subdir[MAXPATHLEN];
	bool_t mount_access = TRUE;
	bool_t iswildcard;

retry:
	iswildcard = FALSE;
	if (strncmp(mapname, FNPREFIX, FNPREFIXLEN) == 0) {
		getmapent_fn(key, mapname, mapopts, cred->aup_uid,
		    !mount_access, &fnres);
		switch (fnres.type) {
		case FN_MAPENTS:
			mapents = fnres.m_or_l.mapents;
			break;
		case FN_SYMLINK:	/* should never happen */
			free(fnres.m_or_l.symlink);
			break;
		case FN_NONE:	/* fall through */
		default:
			err = ENOENT;
			break;
		}
	} else {
		char *stack[STACKSIZ];
		char **stkptr = stack;

		/* initialize the stack of open files for this thread */
		stack_op(INIT, NULL, stack, &stkptr);

		err = getmapent(key, mapname, &ml, stack, &stkptr, &iswildcard);
		if (err == 0) {
		    mapents = parse_entry(key, mapname, mapopts, &ml,
				subdir, isdirect, mount_access);
		}
	}

	if (trace > 1) {
		struct mapfs *mfs;
		trace_prt(1, "  do_mount1:\n");
		for (me = mapents; me; me = me->map_next) {
			trace_prt(1, "  (%s,%s)\t%s%s%s\n",
			me->map_fstype ? me->map_fstype : "",
			me->map_mounter ? me->map_mounter : "",
			path ? path : "",
			me->map_root  ? me->map_root : "",
			me->map_mntpnt ? me->map_mntpnt : "");
			trace_prt(0, "\t\t-%s\n",
			me->map_mntopts ? me->map_mntopts : "");

			for (mfs = me->map_fs; mfs; mfs = mfs->mfs_next)
				trace_prt(0, "\t\t%s:%s\tpenalty=%d\n",
					mfs->mfs_host ? mfs->mfs_host: "",
					mfs->mfs_dir ? mfs->mfs_dir : "",
					mfs->mfs_penalty);
		}
	}

	*alpp = NULL;

	/*
	 * Each mapent in the list describes a mount to be done.
	 * Normally there's just a single entry, though in the
	 * case of /net mounts there may be many entries, that
	 * must be mounted as a hierarchy.  For each mount the
	 * automountd must make sure the required mountpoint
	 * exists and invoke the appropriate mount command for
	 * the fstype.
	 */
	private = "";
	for (me = mapents; me && !err; me = me->map_next) {
		(void) sprintf(mntpnt, "%s%s%s", path, mapents->map_root,
				me->map_mntpnt);
		/*
		 * remove trailing /'s from mountpoint to avoid problems
		 * stating a directory with two or more trailing slashes.
		 * This will let us mount directories from machines
		 * which export with two or more slashes (apollo for instance).
		 */
		len = strlen(mntpnt) - 1;
		while (mntpnt[len] == '/')
			mntpnt[len--] = '\0';

		(void) strcpy(spec_mntpnt, mntpnt);

		if (strcmp(me->map_fstype, MNTTYPE_NFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err = mount_nfs(me, spec_mntpnt, private, overlay);
			/*
			 * We must retry if we don't have access to the
			 * root file system and there are other
			 * following mapents. The reason we can't
			 * continue because the rest of the mapent list
			 * depends on whether mount_access is TRUE or FALSE.
			 */
			if (err == NFSERR_ACCES && me->map_next != NULL) {
				/*
				 * don't expect mount_access to be
				 * FALSE here, but we do a check
				 * anyway.
				 */
				if (mount_access == TRUE) {
					mount_access = FALSE;
					err = 0;
					free_mapent(mapents);
					goto retry;
				}
			}
			mount_ok = !err;
		} else if (strcmp(me->map_fstype, MNTTYPE_AUTOFS) == 0) {
			alp = (action_list *)malloc(sizeof (action_list));
			if (alp == NULL) {
				syslog(LOG_ERR, "malloc of alp failed");
				continue;
			}
			memset(alp, 0, sizeof (action_list));

			if (isdirect)
				strcpy(root, path);
			else
				sprintf(root, "%s/%s", path, key);

			/*
			 * get the next subidr, but only if its a modified
			 * or faked autofs mount
			 */
			if (me->map_modified || me->map_faked) {
				sprintf(next_subdir, "%s%s", subdir,
					me->map_mntpnt);
			} else {
				next_subdir[0] = '\0';
			}

			if (trace > 2)
				trace_prt(1, "  root=%s\t next_subdir=%s\n",
						root, next_subdir);
			err = mount_autofs(me, spec_mntpnt, alp, root,
							next_subdir, key);
			if (err == 0) {
				/*
				 * append to action list
				 */
				mount_ok++;
				if (*alpp == NULL)
					*alpp = alp;
				else {
					for (tmp = *alpp; tmp != NULL;
					    tmp = tmp->next)
						prev = tmp;
					prev->next = alp;
				}
			} else {
				free(alp);
				mount_ok = 0;
			}
		} else if (strcmp(me->map_fstype, MNTTYPE_LOFS) == 0) {
			remove_browse_options(me->map_mntopts);
			err = loopbackmount(me->map_fs->mfs_dir, spec_mntpnt,
					    me->map_mntopts, overlay);
			mount_ok = !err;
		} else {
			remove_browse_options(me->map_mntopts);
			err = mount_generic(me->map_fs->mfs_dir,
					    me->map_fstype, me->map_mntopts,
					    spec_mntpnt, overlay);
			mount_ok = !err;
		}
	}
	if (mapents)
		free_mapent(mapents);

	/*
	 * If an error occurred,
	 * the filesystem doesn't exist, or could not be
	 * mounted.  Return EACCES to autofs indicating that
	 * the mountpoint can not be accessed if this is not
	 * a wildcard access.  If it is a wildcard access we
	 * return ENOENT since the lookup that triggered
	 * this mount request will fail and the entry will not
	 * be available.
	 */
	if (mount_ok) {
		/*
		 * No error occurred, return 0 to indicate success.
		 */
		err = 0;
	} else {
		/*
		 * The filesystem does not exist or could not be mounted.
		 * Return ENOENT if the lookup was triggered by a wildcard
		 * access.  Wildcard entries only exist if they can be
		 * mounted.  They can not be listed otherwise (through
		 * a readdir(2)).
		 * Return EACCES if the lookup was not triggered by a
		 * wildcard access.  Map entries that are explicitly defined
		 * in maps are visible via readdir(2), therefore we return
		 * EACCES to indicate that the entry exists, but the directory
		 * can not be opened.  This is the same behavior of a Unix
		 * directory that exists, but has its execute bit turned off.
		 * The directory is there, but the user does not have access
		 * to it.
		 */
		if (iswildcard)
			err = ENOENT;
		else
			err = EACCES;
	}
	return (err);
}

#define	ARGV_MAX	16
#define	VFS_PATH	"/usr/lib/fs"

int
mount_generic(special, fstype, opts, mntpnt, overlay)
	char *special, *fstype, *opts, *mntpnt;
	int overlay;
{
	struct mnttab m;
	struct stat stbuf;
	int i, res;
	char *newargv[ARGV_MAX];

	if (trace > 1) {
		trace_prt(1, "  mount: %s %s %s %s\n",
			special, mntpnt, fstype, opts);
	}

	if (stat(mntpnt, &stbuf) < 0) {
		syslog(LOG_ERR, "Couldn't stat %s: %m", mntpnt);
		return (ENOENT);
	}

	i = 2;

	if (overlay)
		newargv[i++] = "-O";

	/*
	 *  Use "quiet" option to suppress warnings about unsupported
	 *  mount options.
	 */
	newargv[i++] = "-q";

	if (opts && *opts) {
		m.mnt_mntopts = opts;
		if (hasmntopt(&m, MNTOPT_RO) != NULL)
			newargv[i++] = "-r";
		newargv[i++] = "-o";
		newargv[i++] = opts;
	}
	newargv[i++] = "--";
	newargv[i++] = special;
	newargv[i++] = mntpnt;
	newargv[i] = NULL;
	res = fork_exec(fstype, "mount", newargv, verbose);
	if (res == 0 && trace > 1) {
		if (stat(mntpnt, &stbuf) == 0) {
			trace_prt(1, "  mount of %s dev=%x rdev=%x OK\n",
				mntpnt, stbuf.st_dev, stbuf.st_rdev);
		} else {
			trace_prt(1, "  failed to stat %s\n", mntpnt);
		}
	}
	return (res);
}

static int
fork_exec(fstype, cmd, newargv, console)
	char *fstype;
	char *cmd;
	char **newargv;
	int console;
{
	char path[MAXPATHLEN];
	int i;
	int stat_loc;
	int fd = 0;
	struct stat stbuf;
	int res;
	int child_pid;

	/* build the full path name of the fstype dependent command */
	(void) sprintf(path, "%s/%s/%s", VFS_PATH, fstype, cmd);

	if (stat(path, &stbuf) != 0) {
		res = errno;
		return (res);
	}

	if (trace > 1) {
		trace_prt(1, "  fork_exec: %s ", path);
		for (i = 2; newargv[i]; i++)
			trace_prt(0, "%s ", newargv[i]);
		trace_prt(0, "\n");
	}


	newargv[1] = cmd;
	switch ((child_pid = fork1())) {
	case -1:
		syslog(LOG_ERR, "Cannot fork: %m");
		return (errno);
	case 0:
		/*
		 * Child
		 */
		(void) setsid();
		fd = open(console ? "/dev/console" : "/dev/null", O_WRONLY);
		if (fd != -1) {
			(void) dup2(fd, 1);
			(void) dup2(fd, 2);
			(void) close(fd);
		}

		(void) execv(path, &newargv[1]);
		if (errno == EACCES)
			syslog(LOG_ERR, "exec %s: %m", path);

		_exit(errno);
	default:
		/*
		 * Parent
		 */
		(void) waitpid(child_pid, &stat_loc, WUNTRACED);

		if (WIFEXITED(stat_loc)) {
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns exit status %d\n",
				    WEXITSTATUS(stat_loc));
			}

			return (WEXITSTATUS(stat_loc));
		} else
		if (WIFSIGNALED(stat_loc)) {
			if (trace > 1) {
				trace_prt(1,
				    "  fork_exec: returns signal status %d\n",
				    WTERMSIG(stat_loc));
			}
		} else {
			if (trace > 1)
				trace_prt(1,
				    "  fork_exec: returns unknown status\n");
		}

		return (1);
	}
}

int
do_unmount1(ur)
	umntrequest *ur;
{

	struct mntlist *mntl_head, *mntl;
	struct mntlist *match;
	int res = 0;

	mntl_head = fsgetmntlist();
	if (mntl_head == NULL)
		return (1);

	assert(ur->next == NULL);
	/*
	 * Find the entry with a matching device id.
	 * Loopback mounts have the same device id
	 * as the real filesystem, so the autofs
	 * gives us the rdev as well. The lofs
	 * assigns a unique rdev for each lofs mount
	 * so that they can be recognized by the
	 * automountd.
	 * We expect autofs to give the rdev iff lofs.
	 * Therefore a non-zero rdev implies lofs, and
	 * a match of both dev and rdev must occur.
	 */
	match = NULL;
	for (mntl = mntl_head; mntl; mntl = mntl->mntl_next) {
		if (ur->devid == get_devid(mntl->mntl_mnt)) {
			if (ur->rdevid) {
				if (lofs_match(mntl->mntl_mnt, ur)) {
					match = mntl;
					break;
				}
			} else {
				match = mntl;
				break;
			}
		}
	}

	if (match == NULL) {
		/*
		 * dev not found in /etc/mnttab
		 */
		if (verbose) {
			syslog(LOG_ERR, "dev %x, rdev %x  not in mnttab",
				ur->devid, ur->rdevid);
		}
		if (trace > 1) {
			trace_prt(1, "  do_unmount: %x = ? "
				"<----- mntpnt not found\n",
				ur->devid);
		}
		res = 1;
		goto done;
	}

	/*
	 * Special case for NFS mounts.
	 * Don't want to attempt unmounts from
	 * a dead server.  If any member of a
	 * hierarchy belongs to a dead server
	 * give up (try later).
	 */
	if (strcmp(match->mntl_mnt->mnt_fstype, MNTTYPE_NFS) == 0) {
		struct replica *list;
		int i, n;
		bool_t pubopt = FALSE;
		int nfs_port;
		int got_port;

		/*
		 * See if a port number was specified.  If one was
		 * specified that is too large to fit in 16 bits, truncate
		 * the high-order bits (for historical compatibility).  Use
		 * zero to indicate "no port specified".
		 */
		got_port = nopt((struct mnttab *)(match->mntl_mnt),
			MNTOPT_PORT, &nfs_port);
		if (!got_port)
			nfs_port = 0;
		nfs_port &= USHRT_MAX;

		if (hasmntopt((struct mnttab *)(match->mntl_mnt),
			MNTOPT_PUBLIC))
			pubopt = TRUE;

		list = parse_replica(match->mntl_mnt->mnt_special, &n);
		if (list == NULL) {
			if (n >= 0)
				syslog(LOG_ERR, "Memory allocation failed: %m");
			res = 1;
			goto done;
		}

		for (i = 0; i < n; i++) {
			if (pingnfs(list[i].host, 1, NULL, 0, nfs_port,
			    pubopt, list[i].path) != RPC_SUCCESS) {
				res = 1;
				free_replica(list, n);
				goto done;
			}
		}
		free_replica(list, n);
	}

	res = unmount_mntpnt((struct mnttab *)(match->mntl_mnt));

done:	fsfreemntlist(mntl_head);
	return (res);
}

static int
unmount_mntpnt(mnt)
	struct mnttab *mnt;
{
	char *fstype = mnt->mnt_fstype;
	char *mountp = mnt->mnt_mountp;
	char *newargv[ARGV_MAX];
	int res;

	if (strcmp(fstype, MNTTYPE_NFS) == 0) {
		res = nfsunmount(mnt);
	} else if (strcmp(fstype, MNTTYPE_LOFS) == 0) {
		if ((res = umount(mountp)) < 0)
			res = errno;
	} else {
		newargv[2] = mountp;
		newargv[3] = NULL;

		res = fork_exec(fstype, "umount", newargv, verbose);
		if (res == ENOENT) {
			/*
			 * filesystem specific unmount command not found
			 */
			if ((res = umount(mountp)) < 0)
				res = errno;
		}
	}

	if (trace > 1)
		trace_prt(1, "  unmount %s %s\n",
			mountp, res ? "failed" : "OK");
	return (res);
}

/*
 * stats the path referred to by mnt_mnt->mnt_mountp, checks its
 * st_rdev, and returns 1 if it matches "rdev", 0 otherwise.
 */
static int
lofs_match(struct extmnttab *mnt, struct umntrequest *ur)
{
	struct stat stbuf;
	int retcode;

	retcode = stat(mnt->mnt_mountp, &stbuf) == 0 &&
			stbuf.st_rdev == ur->rdevid;
	return (retcode);
}

/*
 * Removes the 'browse' and 'nobrowse' options from the option
 * string 'opts'.
 */
static void
remove_browse_options(char *opts)
{
	char *p, *pb;
	char buf[MAXOPTSLEN], new[MAXOPTSLEN];
	char *placeholder;

	new[0] = '\0';
	(void) strcpy(buf, opts);
	pb = buf;
	while (p = (char *)strtok_r(pb, ",", &placeholder)) {
		pb = NULL;
		if (strcmp(p, NOBROWSE) && strcmp(p, BROWSE)) {
			if (new[0] != '\0')
				(void) strcat(new, ",");
			(void) strcat(new, p);
		}
	}
	(void) strcpy(opts, new);
}
