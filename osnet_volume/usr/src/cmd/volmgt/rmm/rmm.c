/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rmm.c	1.49	98/07/31 SMI"


#include	<stdio.h>
#include	<stdlib.h>
#include	<fcntl.h>
#include	<dirent.h>
#include	<string.h>
#include	<errno.h>
#include	<rmmount.h>
#include	<locale.h>
#include	<libintl.h>
#include	<sys/vtoc.h>
#include	<rpc/types.h>
#include	<sys/param.h>
#include	<sys/stat.h>
#include	<sys/wait.h>
#include	<sys/types.h>
#include	<sys/types.h>
#include	<unistd.h>
#include	<string.h>
#include	<regex.h>
#include	"rmm_int.h"


/*
 * This program (used with volume management) will figure out
 * what file system type you have and mount it up for you at
 * the pre-defined place.
 *
 * We set the nosuid flag for security, and we set it to be read-only,
 * if the device being mounted is read-only.
 *
 */

#define	FSCK_CMD		"/etc/fsck"
#define	MOUNT_CMD		"/etc/mount"
#define	UMOUNT_CMD		"/etc/umount"


struct ident_list **ident_list = NULL;
struct action_list **action_list = NULL;

char	*prog_name = NULL;
pid_t	prog_pid = 0;

#define	DEFAULT_CONFIG	"/etc/rmmount.conf"
#define	DEFAULT_DSODIR	"/usr/lib/rmmount"

char	*rmm_dsodir = DEFAULT_DSODIR;
char	*rmm_config = DEFAULT_CONFIG;
bool_t	rmm_debug = FALSE;

#define	SHARE_CMD	"/usr/sbin/share"
#define	UNSHARE_CMD	"/usr/sbin/unshare"


/* length option string for mount */
#define	RMM_OPTSTRLEN		128


/*
 * Production (i.e. non-DEBUG) mode is very, very, quiet.  The
 * -D flag will turn on printfs.
 */



int
main(int argc, char **argv)
{
	static void			usage(void);
	static void			find_fstypes(struct action_arg **);
	static int			exec_mounts(struct action_arg **);
	static int			exec_umounts(struct action_arg **);
	static void			exec_actions(struct action_arg **,
	    bool_t);
	static struct action_arg	**build_actargs(char *);
	extern	char			*optarg;
	int				c;
	char				*path = NULL;
	char				*vact;
	int				exval = 0;
	struct action_arg		**aa;
	char				*name = getenv("VOLUME_NAME");



	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif

	(void) textdomain(TEXT_DOMAIN);

	prog_name = argv[0];
	prog_pid = getpid();

	if (geteuid() != 0) {
		(void) fprintf(stderr,
		    gettext("%s(%ld) error: must be root to execute\n"),
		    prog_name, prog_pid);
		return (-1);
	}

	if (name == NULL) {
		dprintf("%s(%ld): VOLUME_NAME was null!!\n",
		    prog_name, prog_pid);
	}

	/* back to normal now... */
	while ((c = getopt(argc, argv, "d:c:D")) != EOF) {
		switch (c) {
		case 'D':
			rmm_debug = TRUE;
			break;
		case 'd':
			rmm_dsodir = (char *)optarg;
			break;
		case 'c':
			rmm_config = (char *)optarg;
			break;
		default:
			usage();
			/*NOTREACHED*/
		}
	}

#ifdef	DEBUG
	if (rmm_debug) {
		char	*volume_name = getenv("VOLUME_NAME");
		char	*volume_path = getenv("VOLUME_PATH");
		char	*volume_action = getenv("VOLUME_ACTION");
		char	*volume_mediatype = getenv("VOLUME_MEDIATYPE");
		char	*volume_symdev = getenv("VOLUME_SYMDEV");

		/* ensure we don't have any null env vars (name already ok) */
		if (volume_path == NULL) {
			dprintf("%s(%ld): VOLUME_PATH was null!!\n",
			    prog_name, prog_pid);
			return (-1);
		}
		if (volume_action == NULL) {
			dprintf("%s(%ld): VOLUME_ACTION was null!!\n",
			    prog_name, prog_pid);
			return (-1);
		}
		if (volume_mediatype == NULL) {
			dprintf("%s(%ld): VOLUME_MEDIATYPE was null!!\n",
			    prog_name, prog_pid);
			return (-1);
		}
		if (volume_symdev == NULL) {
			dprintf("%s(%ld): VOLUME_SYMDEV was null!!\n",
			    prog_name, prog_pid);
			return (-1);
		}

		dprintf("\nDEBUG: Env Vars:\n");
		dprintf("DEBUG:   VOLUME_NAME=%s\n", volume_name);
		dprintf("DEBUG:   VOLUME_PATH=%s\n", volume_path);
		dprintf("DEBUG:   VOLUME_ACTION=%s\n", volume_action);
		dprintf("DEBUG:   VOLUME_MEDIATYPE=%s\n", volume_mediatype);
		dprintf("DEBUG:   VOLUME_SYMDEV=%s\n", volume_symdev);
		dprintf("\n");
	}
#endif	/* DEBUG */

	/* for core files */
	(void) chdir(rmm_dsodir);

	if ((path = getenv("VOLUME_PATH")) == NULL) {
		dprintf("%s(%ld): VOLUME_PATH was null!!\n",
		    prog_name, prog_pid);
		return (-1);
	}

	/* build the action_arg structure. */
	if ((aa = build_actargs(path)) == NULL) {
		return (0);
	}

	if ((vact = getenv("VOLUME_ACTION")) == NULL) {
		dprintf("%s(%ld): VOLUME_ACTION unspecified\n",
		    prog_name, prog_pid);
		return (0);
	}
	/*
	 * read in our configuration file.
	 * Builds data structures used by find_fstypes
	 * and exec_actions,  and uses the configuration
	 * file as well as command line poop.
	 */
	config_read();

	if (strcmp(vact, "insert") == 0) {

		/* insert action */

		/* premount actions */
		exec_actions(aa, TRUE);
		/*
		 * If the media is unformatted, we don't try to figure out
		 * what fstype or try to mount it.
		 */
		if (strcmp(name, "unformatted") != 0) {
			/* Find the filesystem type of each entry in the aa. */
			find_fstypes(aa);

			/* try to mount the various file systems */
			exval = exec_mounts(aa);
		}
		/* execute user's (post mount) actions */
		if (exval == 0) {
			exec_actions(aa, FALSE);
		}

	} else if (strcmp(vact, "eject") == 0) {

		/* eject action */

		exec_actions(aa, TRUE);
		if (strcmp(name, "unformatted") != 0) {
			/* try to umount the various file systems */
			exval = exec_umounts(aa);
		} else {
			exval = 0;
		}
		/*
		 * Run the actions if things were unmounted properly.
		 */
		if (exval == 0) {
			exec_actions(aa, FALSE);
		}

	} else {
		dprintf("%s(%ld): unknown action type %s\n",
			prog_name, prog_pid, vact);
		exval = 0;
	}

	return (exval);
}


static void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "%s(%ld) usage: %s [-D] [-c config_file] [-d filesystem_dev]\n"),
	    prog_name, prog_pid, prog_name);

	exit(-1);
}


static struct action_arg **
build_actargs(char *path)
{
	struct stat		sb;
	DIR			*dirp;
	struct dirent64		*dp;
	char			*mtype;
	char 			namebuf[MAXPATHLEN];
	int			aaoff;
	struct action_arg	**aa;




	/*
	 * Stat the file and make sure it's there.
	 */
	if (stat(path, &sb) < 0) {
		dprintf("%s(%ld): fstat %s; %m\n", prog_name, prog_pid, path);
		return (NULL);
	}

	if ((mtype = getenv("VOLUME_MEDIATYPE")) == NULL) {
		dprintf("%s(%ld): VOLUME_MEDIATYPE unspecified\n",
			prog_name, prog_pid);
		return (NULL);
	}

	/* this is the case where the device has no partitions */
	if ((sb.st_mode & S_IFMT) == S_IFBLK) {
		/*
		 * Just two action_args required here.
		 */
		aa = (struct action_arg **)calloc(2,
			sizeof (struct action_arg *));
		aa[0] = (struct action_arg *)calloc(1,
			sizeof (struct action_arg));
		aa[1] = (struct action_arg *)calloc(1,
			sizeof (struct action_arg));
		aa[0]->aa_path = strdup(path);
		aa[0]->aa_rawpath = rawpath(path);
		aa[0]->aa_media = strdup(mtype);
		aa[0]->aa_mnt = TRUE;			/* do the mount */
	} else if ((sb.st_mode & S_IFMT) == S_IFDIR) {
		/* ok, so it's a directory (i.e. device w/partitions) */
		if ((dirp = opendir(path)) == NULL) {
			dprintf("%s(%ld): opendir failed on %s; %m\n",
				prog_name, prog_pid, path);
			return (NULL);
		}
		aaoff = 0;
		aa = (struct action_arg **)calloc(V_NUMPAR+1,
		    sizeof (struct action_arg *));
		while (dp = readdir64(dirp)) {
			/* ignore "." && ".." */
			if ((strcmp(dp->d_name, ".") == 0)||
			    (strcmp(dp->d_name, "..") == 0)) {
				continue;
			}

			(void) sprintf(namebuf, "%s/%s", path, dp->d_name);

			if (stat(namebuf, &sb) < 0) {
				continue;
			}

			/*
			 * If we're looking though a raw directory,
			 * get outta here.
			 */
			if ((sb.st_mode & S_IFMT) == S_IFCHR) {
				return (NULL);
			}

			if ((sb.st_mode & S_IFMT) != S_IFBLK) {
				continue;
			}
			aa[aaoff] = (struct action_arg *)calloc(1,
				sizeof (struct action_arg));
			aa[aaoff]->aa_path = strdup(namebuf);
			aa[aaoff]->aa_media = strdup(mtype);
			aa[aaoff]->aa_partname = strdup(dp->d_name);
			aa[aaoff]->aa_rawpath = rawpath(namebuf);
			aa[aaoff]->aa_mnt = TRUE;
			/*
			 * This should never be the case, but who
			 * knows.  Let's just be careful.
			 */
			if (aaoff == V_NUMPAR) {
				break;
			}

			aaoff++;
		}
		aa[aaoff] = (struct action_arg *)calloc(1,
			sizeof (struct action_arg));

		closedir(dirp);
	} else {
		dprintf("%s(%ld): %s is mode 0%o\n",
			prog_name, prog_pid, path, sb.st_mode);
		return (NULL);
	}
	return (aa);
}


static void
find_fstypes(struct action_arg **aa)
{
	extern int	audio_only(struct action_arg *);

	int		ai;
	int		fd;
	int		i, j;
	int		foundfs, foundmedia;
	int		clean;
	char		*mtype = getenv("VOLUME_MEDIATYPE");


	if (mtype == NULL) {
		dprintf("%s(%ld): VOLUME_MEDIATYPE unspecified\n",
			prog_name, prog_pid);
		exit(-1);
	}

	if (ident_list == NULL) {
		return;
	}

	/*
	 * If it's a cdrom and it only has audio on it, don't
	 * bother trying to figure out a file system type.
	 */
	if (strcmp(mtype, "cdrom") == 0) {
		if (audio_only(aa[0]) != FALSE) {
			return;
		}
	}

	/*
	 * We leave the file descriptor open on purpose so that
	 * the blocks that we've read in don't get invalidated
	 * on close, thus wasting i/o.  The mount (or attempted mount)
	 * command later on will have access to the blocks we have
	 * read as part of identification through the buffer cache.
	 * The only *real* difficulty here is that reading from the
	 * block device means that we always read 8k, even if we
	 * really don't need to.
	 */
	for (ai = 0; aa[ai]->aa_path; ai++) {
		/*
		 * if we're not supposed to mount it, just move along.
		 */
		if (aa[ai]->aa_mnt == FALSE) {
			continue;
		}

		if ((fd = open(aa[ai]->aa_path, O_RDONLY)) < 0) {
			dprintf("%s(%ld): %s; %m\n", prog_name, prog_pid,
				aa[ai]->aa_path);
			continue;
		}
		foundfs = FALSE;
		for (i = 0; ident_list[i]; i++) {
			/*
			 * Look through the list of media that this
			 * file system type can live on, and continue
			 * on if this isn't an appropriate function.
			 */
			foundmedia = FALSE;
			for (j = 0; ident_list[i]->i_media[j]; j++) {
				if (strcmp(aa[ai]->aa_media,
				    ident_list[i]->i_media[j]) == 0) {
					foundmedia = TRUE;
					break;
				}
			}
			if (foundmedia == FALSE) {
				continue;
			}
			if (ident_list[i]->i_ident == NULL) {
				/*
				 * Get the id function.
				 */
				if ((ident_list[i]->i_ident =
				    (bool_t (*)(int, char *, int *, int))
				    dso_load(ident_list[i]->i_dsoname,
				    "ident_fs", IDENT_VERS)) == NULL) {
					continue;
				}
			}
			/*
			 * Call it.
			 */
			if (((*ident_list[i]->i_ident)
			    (fd, aa[ai]->aa_rawpath, &clean, 0)) != FALSE) {
				foundfs = TRUE;
				break;
			}
		}
		if (foundfs) {
			aa[ai]->aa_type = strdup(ident_list[i]->i_type);
			aa[ai]->aa_clean = clean;
		}
		close(fd);
	}
}


/*
 * return 0 if all goes well, else return the number of problems
 */
static int
exec_mounts(struct action_arg **aa)
{
	static bool_t		hard_mount(struct action_arg *,
				    struct mount_args *, bool_t *);
	static void		share_mount(struct action_arg *,
				    struct mount_args *, bool_t);
	static void		clean_fs(struct action_arg *,
				    struct mount_args *);
	int			ai;
	int			mnt_ai = -1;
	char			symname[2 * MAXNAMELEN];
	char			symcontents[MAXNAMELEN];
#ifdef	OLD_SLICE_HANDLING_CODE
	char			*mountname;
	char			*s;
#endif
	char			*symdev = getenv("VOLUME_SYMDEV");
	char			*name = getenv("VOLUME_NAME");
	struct mount_args	*ma[3] = {NULL, NULL, NULL};
				/*
				 * The "mount arguments" are stored in an array
				 * indexed by the actual command they are
				 * applied to: "fsck", "mount", and "share"
				 */
	int			i, j;
	int			ret_val = 0;
	char			*mntpt;
	bool_t			rdonly = FALSE;




	for (ai = 0; aa[ai]->aa_path; ai++) {

		/*
		 * if a premount action told us not to mount
		 * it, don't do it.
		 */
		if (aa[ai]->aa_mnt == FALSE) {
			dprintf("%s(%ld): not supposed to mount %s\n",
			    prog_name, prog_pid, aa[ai]->aa_path);
			continue;
		}

		/*
		 * ok, let's do some real work here...
		 */
		dprintf("%s(%ld): %s is type %s\n", prog_name, prog_pid,
		    aa[ai]->aa_path, aa[ai]->aa_type?aa[ai]->aa_type:"data");

		if (aa[ai]->aa_type != NULL) {	/* assuming we have a type */

			/* no need to try to clean/mount if already mounted */
			if (mntpt = getmntpoint(aa[ai]->aa_path)) {
				/* already mounted on! */
#ifdef	DEBUG
				dprintf("DEBUG: "
					"%s already mounted on (%s dirty)\n",
					aa[ai]->aa_path,
					aa[ai]->aa_clean ? "NOT" : "IS");
#endif
				free(mntpt);
				ret_val++;
				continue;
			}
#ifdef	DEBUG
			dprintf("DEBUG: %s NOT already mounted\n",
				aa[ai]->aa_path);
#endif
			/*
			 * find the right mount arguments for this device
			 */
			for (j = 0; j < 3; j++) {
			    if (cmd_args[j] != NULL) {
				for (i = 0; cmd_args[j][i] != NULL; i++) {

					/* try to match name against RE */
#ifdef	DEBUG_RE
					dprintf("exec_mounts (%d,%d): "
						"regexec(\"%s\", \"%s\") ...\n",
						j, i, cmd_args[j][i]->ma_namere,
						name);
#endif
					if (regexec(&(cmd_args[j][i]->ma_re),
						    name, 0L, NULL, 0) == 0 &&
					    fs_supported(aa[ai]->aa_type,
							cmd_args[j][i])) {
						ma[j] = cmd_args[j][i];
#ifdef	DEBUG_RE
						dprintf("exec_mounts: "
						    "found a NAME match!\n");
#endif
						break;
					}

					/* try to match symname against RE */
#ifdef	DEBUG_RE
					dprintf("exec_mounts (%d,%d): "
						"regexec(\"%s\", \"%s\") ...\n",
						j, i, cmd_args[j][i]->ma_namere,
						symdev);
#endif
					if (regexec(&(cmd_args[j][i]->ma_re),
						    symdev, 0L, NULL, 0) == 0 &&
					    fs_supported(aa[ai]->aa_type,
							cmd_args[j][i])) {
						ma[j] = cmd_args[j][i];
#ifdef	DEBUG_RE
						dprintf("exec_mounts: "
						    "found a SYMNAME match!\n");
#endif
						break;
					}
				}
			    }
			}

#ifdef	DEBUG_MA
			if (ma[CMD_MOUNT] == NULL) {
				dprintf("exec_mounts: no mount args!\n");
			}
#endif

			/*
			 * If the file system is not clean, or if there
			 * is an explicit fsck option for this file system,
			 * then run fsck.
			 */
			if (aa[ai]->aa_clean == FALSE || ma[CMD_FSCK] != NULL) {
				clean_fs(aa[ai], ma[CMD_FSCK]);
			}

			if (!hard_mount(aa[ai], ma[CMD_MOUNT], &rdonly)) {
				ret_val++;
			}

			/* remember if we mount one of these guys */
			if (mnt_ai == -1) {
				if (aa[ai]->aa_mountpoint)
					mnt_ai = ai;
			}

			if (ma[CMD_SHARE] != NULL) {
				/*
				 * export the file system.
				 */
				share_mount(aa[ai], ma[CMD_SHARE], rdonly);
			}
		}
	}

	if (mnt_ai != -1) {
#ifdef OLD_SLICE_HANDLING_CODE
		/*
		 * XXX: did we used to do something here having to do with
		 * the slices mounted for a volume??? (lduncan)
		 */
		(void) sprintf(symname, "/%s/%s", aa[mnt_ai]->aa_media,
		    symdev);
		if (aa[0]->aa_partname) {
			mountname = strdup(aa[mnt_ai]->aa_mountpoint);
			if ((s = strrchr(mountname, '/')) != NULL) {
				*s = NULLC;
			}
			(void) unlink(symname);
			(void) symlink(mountname, symname);
		} else {
			(void) unlink(symname);
			(void) symlink(aa[mnt_ai]->aa_mountpoint, symname);
		}
#else	/* !OLD_SLICE_HANDLING_CODE */
		(void) sprintf(symcontents, "./%s", name);
		(void) sprintf(symname, "/%s/%s", aa[mnt_ai]->aa_media,
		    symdev);
		(void) unlink(symname);
		(void) symlink(symcontents, symname);
#endif	/* !OLD_SLICE_HANDLING_CODE */
	}

	return (ret_val);
}


/*
 * Mount the filesystem found at "path" of type "type".
 */
static bool_t
hard_mount(struct action_arg *aa, struct mount_args *ma, bool_t *rdonly)
{
	char		buf[BUFSIZ];
	char		*targ_dir;
	char		lopts[RMM_OPTSTRLEN],
			*options;	/* mount option string */
	char		mountpoint[MAXNAMLEN];
	struct stat 	sb;
	time_t		tloc;
	int		rval;
	int		pfd[2];
	pid_t		pid;
	int		n;
	mode_t		mpmode;
	bool_t		ret_val;


#ifdef DEBUG
	dprintf("hard_mount: entered with aa=%x, ma=%x, rdonly=%d\n",
		aa, ma, *rdonly);
#endif
	if (stat(aa->aa_path, &sb)) {
		dprintf("%s(%ld): %s; %m\n", prog_name, prog_pid, aa->aa_path);
		ret_val = FALSE;
		goto dun;
	}

	/*
	 * Here, we assume that the owners permissions are the
	 * most permissive and that if he can't "write" to the
	 * device that it should be mounted readonly.
	 */
	if (sb.st_mode & S_IWUSR) {
		/*
		 * If he wants it mounted readonly, give it to him.  The
		 * default is that if the device can be written, we mount
		 * it read/write.
		 */
		if (ma != NULL && (ma->ma_key & MA_READONLY)) {
			/* user has requested RO for this fs type */
			*rdonly = TRUE;
		}
	} else {
		*rdonly = TRUE;
	}

	/* "hsfs" file systems must be mounted readonly */
	if (strcmp(aa->aa_type, "hsfs") == 0) {
		*rdonly = TRUE;
	}

	/*
	 * If the file system isn't clean, we attempt a ro mount.
	 * We already tried the fsck.
	 */
	if (aa->aa_clean == FALSE) {
		*rdonly = TRUE;
	}

	targ_dir = getenv("VOLUME_NAME");

	if (targ_dir == NULL) {
		(void) fprintf(stderr,
		    gettext("%s(%ld) error: VOLUME_NAME not set for %s\n"),
		    prog_name, prog_pid, aa->aa_path);
		ret_val = FALSE;
		goto dun;
	}

	if (aa->aa_partname) {
		(void) sprintf(mountpoint, "/%s/%s/%s", aa->aa_media,
		    targ_dir, aa->aa_partname);
	} else {
		(void) sprintf(mountpoint, "/%s/%s", aa->aa_media, targ_dir);
	}

	/* make our mountpoint */
	(void) makepath(mountpoint, 0755);

	if (ma == NULL || ma->ma_options == NULL) {
		options = lopts;
		options[0] = NULLC;
	}
	else
		options = ma->ma_options;

	/* add in readonly option if necessary */
	if (*rdonly) {
		if (options[0] == NULLC)
			(void) strcat(options, "ro");
		else
			(void) strcat(options, ",ro");
	}

	(void) pipe(pfd);
	if ((pid = fork()) == 0) {

		(void) close(pfd[1]);
		(void) dup2(pfd[0], fileno(stdin));
		(void) dup2(pfd[0], fileno(stdout));
		(void) dup2(pfd[0], fileno(stderr));

		if (options[0] == NULLC) {
			dprintf("%s(%ld): %s -F %s %s %s\n", prog_name,
			    prog_pid, MOUNT_CMD, aa->aa_type, aa->aa_path,
			    mountpoint);
			(void) execl(MOUNT_CMD, MOUNT_CMD, "-F",
			    aa->aa_type, aa->aa_path, mountpoint, NULL);
		} else {
			dprintf("%s(%ld): %s -F %s -o %s %s %s\n", prog_name,
			    prog_pid, MOUNT_CMD, aa->aa_type, options,
			    aa->aa_path, mountpoint);
			(void) execl(MOUNT_CMD, MOUNT_CMD, "-F",
			    aa->aa_type, "-o", options, aa->aa_path,
			    mountpoint, NULL);
		}

		/* shouldn't get this far */

		(void) fprintf(stderr,
		    gettext("%s(%ld) error: exec of %s failed; %s\n"),
		    prog_name, prog_pid, MOUNT_CMD, strerror(errno));
		_exit(-1);
	}
	(void) close(pfd[0]);

	/* wait for the mount command to exit */
	if (waitpid(pid, &rval, 0) < 0) {
		dprintf("%s(%ld): waitpid() failed (errno %d)\n", prog_name,
		    prog_pid, errno);
		ret_val = FALSE;
		goto dun;
	}

	if ((WEXITSTATUS(rval) == 0) &&
	    (!WIFSIGNALED(rval))) {
		(void) time(&tloc);
		if (options[0] == NULLC) {
			dprintf("%s(%ld): \"%s\" mounted at %s\n",
			    prog_name, prog_pid, mountpoint, ctime(&tloc));
		} else {
			dprintf("%s(%ld): \"%s\" mounted (%s) at %s\n",
			    prog_name, prog_pid, mountpoint, options,
			    ctime(&tloc));
		}
		aa->aa_mnt = TRUE;
		aa->aa_mountpoint = strdup(mountpoint);
#ifdef	DEBUG
		dprintf(
		"\nDEBUG: Setting u.g of \"%s\" to %d.%d (me=%d.%d)\n\n",
		    mountpoint, sb.st_uid, sb.st_gid, getuid(), getgid());
#endif
		/*
		 * set owner and modes.
		 */
		(void) chown(mountpoint, sb.st_uid, sb.st_gid);

		mpmode = (sb.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO));
		/* read implies execute */
		if (mpmode & S_IRUSR) {
			mpmode |= S_IXUSR;
		}
		if (mpmode & S_IRGRP) {
			mpmode |= S_IXGRP;
		}
		if (mpmode & S_IROTH) {
			mpmode |= S_IXOTH;
		}
#ifdef	DEBUG
		dprintf("DEBUG: Setting mode of \"%s\" to %05o\n\n",
		    mountpoint, mpmode);
#endif
		(void) chmod(mountpoint, mpmode);

		ret_val = TRUE;

	} else {
		/* if there was an error, print out the mount message */
		(void) fprintf(stderr, gettext("%s(%ld) mount error:\n\t"),
		    prog_name, prog_pid);
		(void) fflush(stderr);
		while ((n = read(pfd[1], buf, BUFSIZ)) > 0) {
			(void) write(fileno(stderr), buf, n);
		}
		aa->aa_mnt = FALSE;
		(void) rmdir(mountpoint);			/* cleanup */
		ret_val = FALSE;
	}

dun:
	/* all done */
	if (pfd[1] >= 0) {
		(void) close(pfd[1]);
	}
	return (ret_val);
}


/*
 * export the filesystem
 */
static void
share_mount(struct action_arg *aa, struct mount_args *ma, bool_t rdonly)
{
	extern void	share_readonly(struct mount_args *);
	extern void	quote_clean(int, char **);
	extern void	makeargv(int *, char **, char *);
	pid_t  		pid;
	int		pfd[2];
	int		rval;
	int		ac;
	char		*av[MAX_ARGC];
	char		buf[BUFSIZ];
	int		n;




	if (aa->aa_mnt == FALSE) {
		return;
	}

	/* if it's a readonly thing, make sure the share args are right */
	if (rdonly || ma->ma_key & MA_READONLY) {
		share_readonly(ma);
	}

	/* build our command line into buf */
	(void) strcpy(buf, SHARE_CMD);
	(void) strcat(buf, " ");
	(void) strcat(buf, ma->ma_options);
	(void) strcat(buf, " ");
	(void) strcat(buf, aa->aa_mountpoint);

	makeargv(&ac, av, buf);
	quote_clean(ac, av);	/* clean up quotes from -d stuff... yech */

	(void) pipe(pfd);
	if ((pid = fork()) == 0) {

		(void) close(pfd[1]);
		(void) dup2(pfd[0], fileno(stdin));
		(void) dup2(pfd[0], fileno(stdout));
		(void) dup2(pfd[0], fileno(stderr));

		(void) execv(SHARE_CMD, av);

		(void) fprintf(stderr,
		    gettext("%s(%ld) error: exec of %s failed; %s\n"),
		    prog_name, prog_pid, SHARE_CMD, strerror(errno));
		_exit(-1);
	}
	(void) close(pfd[0]);

	/* wait for the share command to exit */
	if (waitpid(pid, &rval, 0) < 0) {
		dprintf("%s(%ld): waitpid() failed (errno %d)\n",
		    prog_name, prog_pid, errno);
		return;
	}

	if (WEXITSTATUS(rval) != 0) {
		/* if there was an error, print out the mount message */
		(void) fprintf(stderr, gettext("%s(%ld) share error:\n\t"),
		    prog_name, prog_pid);
		(void) fflush(stderr);
		while ((n = read(pfd[1], buf, BUFSIZ)) > 0) {
			(void) write(fileno(stderr), buf, n);
		}
	} else {
		(void) dprintf("%s(%ld): %s shared\n", prog_name, prog_pid,
		    aa->aa_mountpoint);
	}
}


/*
 * unexport the filesystem.
 */
/*ARGSUSED*/
static void
unshare_mount(struct action_arg *aa, struct mount_args *ma)
{
	extern void	makeargv(int *, char **, char *);
	pid_t  		pid;
	int		pfd[2];
	int		rval;
	int		ac;
	char		*av[MAX_ARGC];
	char		buf[BUFSIZ];
	int		n;
	char		mountpoint[MAXPATHLEN];
	char		*targ_dir = getenv("VOLUME_NAME");


	/*
	 * reconstruct the mount point and hope the media's still
	 * mounted there. :-(
	 */
	if (aa->aa_partname != NULL) {
		(void) sprintf(mountpoint, "/%s/%s/%s", aa->aa_media,
		    targ_dir, aa->aa_partname);
	} else {
		(void) sprintf(mountpoint, "/%s/%s", aa->aa_media, targ_dir);
	}

	/* build our command line into buf */
	(void) strcpy(buf, UNSHARE_CMD);
	(void) strcat(buf, " ");
	(void) strcat(buf, mountpoint);

	makeargv(&ac, av, buf);

	(void) pipe(pfd);
	if ((pid = fork()) == 0) {

		(void) close(pfd[1]);

		(void) dup2(pfd[0], fileno(stdin));
		(void) dup2(pfd[0], fileno(stdout));
		(void) dup2(pfd[0], fileno(stderr));

		(void) execv(UNSHARE_CMD, av);

		(void) fprintf(stderr,
		    gettext("%s(%ld) error: exec of %s failed; %s\n"),
		    prog_name, prog_pid, UNSHARE_CMD, strerror(errno));
		_exit(-1);
	}
	(void) close(pfd[0]);

	/* wait for the share command to exit */
	if (waitpid(pid, &rval, 0) < 0) {
		dprintf("%s(%ld): waitpid() failed (errno %d)\n",
		    prog_name, prog_pid, errno);
		return;
	}

	if (WEXITSTATUS(rval) != 0) {
		/* if there was an error, print out the message */
		(void) fprintf(stderr, gettext("%s(%ld) unshare error:\n\t"),
		    prog_name, prog_pid);
		(void) fflush(stderr);
		while ((n = read(pfd[1], buf, BUFSIZ)) > 0) {
			(void) write(fileno(stderr), buf, n);
		}
	}

}


static void
clean_fs(struct action_arg *aa, struct mount_args *ma)
{
	pid_t  		pid;
	int		rval;
	struct stat	sb;


	if (stat(aa->aa_path, &sb)) {
		dprintf("%s(%ld): %s; %m\n",
		    prog_name, prog_pid, aa->aa_path);
		return;
	}

	/*
	 * Here, we assume that the owners permissions are the
	 * most permissive.  If no "write" permission on
	 * device, it should be mounted readonly.
	 */
	if ((sb.st_mode & S_IWUSR) == 0) {
		dprintf("%s(%ld): %s is dirty but read-only (no fsck)\n",
		    prog_name, prog_pid, aa->aa_path);
		return;
	}

	if (aa->aa_clean == FALSE)
		(void) fprintf(stderr, gettext("%s(%ld) warning: %s is dirty, "
						"cleaning (please wait)\n"),
				prog_name, prog_pid, aa->aa_path);
	else
		(void) fprintf(stderr, gettext("%s(%ld) note: fsck of %s "
						"requested (please wait)\n"),
				prog_name, prog_pid, aa->aa_path);

	if ((pid = fork()) == 0) {
		int	fd;

		/* get rid of those nasty err messages */
		if ((fd = open("/dev/null", O_RDWR)) >= 0) {
			(void) dup2(fd, 0);
			(void) dup2(fd, 1);
			(void) dup2(fd, 2);
		}
		if (ma && ma->ma_options[0] != NULLC)
			(void) execl(FSCK_CMD, FSCK_CMD, "-F", aa->aa_type,
			    "-o", ma->ma_options, aa->aa_path, NULL);
		else
			(void) execl(FSCK_CMD, FSCK_CMD, "-F", aa->aa_type,
			    "-o", "p", aa->aa_path, NULL);
		(void) fprintf(stderr,
		    gettext("%s(%ld) error: exec of %s failed; %s\n"),
		    prog_name, prog_pid, FSCK_CMD, strerror(errno));
		_exit(-1);
	}

	/* wait for the fsck command to exit */
	if (waitpid(pid, &rval, 0) < 0) {
		(void) fprintf(stderr,
		    gettext("%s(%ld) warning: can't wait for pid %d (%s)\n"),
		    prog_name, prog_pid, pid, FSCK_CMD);
		    return;
	}

	if (WEXITSTATUS(rval) != 0) {
		(void) fprintf(stderr, gettext(
		    "%s(%ld) warning: fsck of \"%s\" failed, returning %d\n"),
		    prog_name, prog_pid, aa->aa_path, WEXITSTATUS(rval));
	} else {
		aa->aa_clean = TRUE;
	}
}


/*
 * This is fairly irritating.  The biggest problem is that if
 * there are several things to umount and one of them is busy,
 * there is no easy way to mount the other file systems back up
 * again.  So, the semantic that I implement here is that we
 * umount ALL umountable file systems, and return failure if
 * any file systems in "aa" cannot be umounted.
 */
static int
exec_umounts(struct action_arg **aa)
{
	static bool_t		umount_fork(char *);
	int			ai;
	int			i, j;
	bool_t			success = TRUE;
	char			*mountpoint;
	char			*oldmountpoint = NULL; /* save prev. mnt pt */
	char			*symdev = getenv("VOLUME_SYMDEV");
	char			*name = getenv("VOLUME_NAME");
	char			*s;
	struct mount_args	*ma = NULL;



	/* do once for each slice (or just once if only one slice) */
	for (ai = 0; aa[ai]->aa_path; ai++) {

		/*
		 * If it's not in the mount table, we assume it's
		 * not mounted.  Obviously, mnttab must be kept up
		 * to date in all cases for this to work properly
		 */
		if ((mountpoint = getmntpoint(aa[ai]->aa_path)) == NULL) {
			continue;
		}

		/*
		 * find the right mount arguments for this device
		 */
		if (cmd_args[CMD_SHARE] != NULL) {
			for (i = 0; cmd_args[CMD_SHARE][i] != NULL; i++) {

				/* try to match name against RE */
#ifdef	DEBUG_RE
				dprintf("exec_mounts: "
				    "regexec(\"%s\", \"%s\") ...\n",
				    cmd_args[CMD_SHARE][i]->ma_namere, name);
#endif
				if (regexec(&(cmd_args[CMD_SHARE][i]->ma_re),
					    name, 0L, NULL, 0) == 0) {
					ma = cmd_args[CMD_SHARE][i];
#ifdef	DEBUG_RE
					dprintf("exec_umounts: "
						"found a NAME match!\n");
#endif
					break;
				}

				/* try to match symname against RE */
#ifdef	DEBUG_RE
				dprintf("exec_mounts: "
				    "regexec(\"%s\", \"%s\") ...\n",
				    cmd_args[CMD_SHARE][i]->ma_namere, symdev);
#endif
				if (regexec(&(cmd_args[CMD_SHARE][i]->ma_re),
					    symdev, 0L, NULL, 0) == 0) {
					ma = cmd_args[CMD_SHARE][i];
#ifdef	DEBUG_RE
					dprintf("exec_umounts: "
						"found a SYMNAME match!\n");
#endif
					break;
				}
			}
		}

		/* unshare the mount before umounting */
		if (ma != NULL) {
			unshare_mount(aa[ai], ma);
		}

		/*
		 * do the actual umount
		 */
		if (umount_fork(mountpoint) == FALSE) {
			success = FALSE;
		}

		/* remove the mountpoint, if it's a partition */
		if (aa[ai]->aa_partname) {
			(void) rmdir(mountpoint);
		}

		/* save a good mountpoint */
		if (oldmountpoint == NULL) {
			oldmountpoint = strdup(mountpoint);
		}
		free(mountpoint);
		mountpoint = NULL;
	}

	/*
	 * clean up our directories and such if all went well
	 */
	if (success) {
		char		rmm_mountpoint[2 * MAXNAMELEN];

		/*
		 * if we have partitions, we'll need to remove the last
		 * component of the path
		 */
		if (aa[0]->aa_partname != NULL) {
			if ((oldmountpoint != NULL) &&
			    ((s = strrchr(oldmountpoint, '/')) != NULL)) {
				*s = NULLC;
			}
		}

		/*
		 * we only want to remove the directory (and symlink)
		 * if we're dealing with the directory we probably created
		 * when we were called to mount the media
		 * i.e. if the direcoty is "/floppy/NAME", then remove it
		 * but if it's /SOME/GENERAL/PATH then don't remove it *or*
		 * try to remove the symlink
		 */

		(void) sprintf(rmm_mountpoint, "/%s/%s", aa[0]->aa_media,
		    name);
		if ((oldmountpoint != NULL) &&
		    (strcmp(oldmountpoint, rmm_mountpoint) == 0)) {
			char    symname[2 * MAXNAMELEN];

			/* remove volmgt mount point */
			(void) rmdir(oldmountpoint);

			/* remove symlink (what harm if it does not exist?) */
			(void) sprintf(symname, "/%s/%s", aa[0]->aa_media,
			    symdev);
			(void) unlink(symname);
		}
	}

	if (oldmountpoint != NULL) {
		free(oldmountpoint);
	}

	return (success ? 0 : 1);
}


static bool_t
umount_fork(char *path)
{
	pid_t	pid;
	int	rval;


	if ((pid = fork()) == 0) {
		int	fd;

		/* get rid of those nasty err messages */
		if ((fd = open("/dev/null", O_RDWR)) >= 0) {
			(void) dup2(fd, fileno(stdin));
			(void) dup2(fd, fileno(stdout));
			(void) dup2(fd, fileno(stderr));
		}

		(void) execl(UMOUNT_CMD, UMOUNT_CMD, path, NULL);

		(void) fprintf(stderr,
		    gettext("%s(%ld) error: exec of %s failed; %s\n"),
		    prog_name, prog_pid, UMOUNT_CMD, strerror(errno));
		_exit(-1);
		/*NOTREACHED*/
	}

	/* wait for the umount command to exit */
	(void) waitpid(pid, &rval, 0);


	if (WEXITSTATUS(rval) != 0) {
		(void) fprintf(stderr, gettext(
		"%s(%ld) error: \"umount\" of \"%s\" failed, returning %d\n"),
		    prog_name, prog_pid, path, WEXITSTATUS(rval));
		return (FALSE);
	}
	return (TRUE);
}


static void
exec_actions(struct action_arg **aa, bool_t premount)
{
	bool_t		(*act_func)(struct action_arg **, int, char **);
	bool_t		rval;
	int		i;
	static int	no_more_actions = FALSE;



	if (action_list == NULL) {
		return;			/* nothing to exec */
	}

	if (aa[0]->aa_path == NULL) {
		return;			/* no patch to exec for */
	}

	if (no_more_actions) {
		return;			/* no more action! (set before) */
	}

	for (i = 0; action_list[i]; i++) {

		if (strcmp(aa[0]->aa_media, action_list[i]->a_media) != 0) {
			continue;
		}

		/*
		 * if we're doing premount actions, don't execute ones
		 * without the premount flag set
		 */
		if (premount && ((action_list[i]->a_flag & A_PREMOUNT) == 0)) {
			continue;
		}

		/*
		 * don't execute premount actions if we've already done
		 * the mount
		 */
		if (!premount && (action_list[i]->a_flag & A_PREMOUNT)) {
			continue;
		}

		/*
		 * get the action function
		 */
		if ((act_func =
		    (bool_t (*)(struct action_arg **, int, char **))dso_load(
		    action_list[i]->a_dsoname, "action", ACT_VERS)) == NULL) {
			continue;
		}

		/*
		 * call it
		 */
		rval = (*act_func)(aa, action_list[i]->a_argc,
		    action_list[i]->a_argv);

		/*
		 * TRUE == don't execute anymore actions (so stop looking)
		 */
		if (rval) {
			no_more_actions = TRUE;
			break;
		}
	}

	return;

}
