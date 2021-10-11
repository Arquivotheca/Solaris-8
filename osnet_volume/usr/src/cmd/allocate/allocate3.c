/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#if !defined(lint) && defined(SCCSIDS)
static char	*bsm_sccsid = "@(#)allocate3.c 1.10 99/06/28 SMI; SunOS BSM";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>
#include <sys/file.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/procfs.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>

#include <bsm/devices.h>
#include "allocate.h"
#include <bsm/audit_uevents.h>

#include <auth_attr.h>
#include <auth_list.h>

#define	EXIT(number) { \
	if (optflg & FORCE) \
		error = number; \
	else \
		return (number); \
}

#define	DEV_ALLOCATED(sbuf)	((sbuf).st_uid != alloc_uid || \
				((sbuf).st_mode & ~S_IFMT) == ALLOC_MODE)

#define	DEVICE_AUTH_SEPARATOR	","

extern void audit_allocate_list();
extern void audit_allocate_device();
extern void audit_deallocate_dev();

extern int	errno;
static int	alloc_uid = -1;
static int	alloc_gid = -1;

isso()
{
	struct passwd *pw;

	pw = getpwuid(getuid());
	if (pw == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Can't get user info for uid=%d\n", getuid());
#endif
		return (0);
	}

	if (chkauthattr(DEVICE_REVOKE_AUTH, pw->pw_name))
		return (1);
	return (0);
}


allocate_uid()
{
	struct passwd *passwd;

	if ((passwd = getpwnam(ALLOC_USER)) == NULL)  {
#ifdef DEBUG
		perror("Unable to get the allocate uid\n");
#endif
		return (-1);
	}
	alloc_uid = passwd->pw_uid;
	alloc_gid = passwd->pw_gid;
	return (alloc_uid);
}

check_devs(list)
char	*list;
{
	char	*file;

	file = strtok(list, " ");
	while (file != NULL) {

		if (access(file, F_OK) == -1) {
#ifdef DEBUG
			fprintf(stderr, "Unable to access file %s\n", file);
#endif
			return (-1);
		}
		file = strtok(NULL, " ");
	}
	return (0);
}


print_dev(dev_list)
devmap_t *dev_list;
{
	char	*file;

	printf(gettext("device: %s "), dev_list->dmap_devname);
	printf(gettext("type: %s "), dev_list->dmap_devtype);
	printf(gettext("files: "));

	file = strtok(dev_list->dmap_devlist, " ");
	while (file != NULL) {
		printf("%s ", file);
		file = strtok(NULL, " ");
	}
	printf("\n");
	return (0);
}




list_device(optflg, uid, device)
int	optflg;
int	uid;
char	*device;

{
	devalloc_t *dev_ent;
	devmap_t *dev_list;
	char	file_name[1024];
	struct	stat stat_buf;
	char	*list;

	if ((dev_ent = getdanam(device)) == NULL) {
		if ((dev_list = getdmapdev(device)) == NULL) {
#ifdef DEBUG
			fprintf(stderr, "Unable to find %s ", device);
			fprintf("in the allocate database\n");
#endif
			return (NODMAPENT);
		} else if ((dev_ent = getdanam(dev_list->dmap_devname)) ==
		    NULL) {
#ifdef DEBUG
			fprintf(stderr, "Unable to find %s ", device);
			fprintf(stderr, "in the allocate database\n");
#endif
			return (NODAENT);
		}
	} else if ((dev_list = getdmapnam(device)) == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Unable to find %s in the allocate database\n",
			device);
#endif
		return (NODMAPENT);
	}

	strcpy(file_name, DAC_DIR);
	strcat(file_name, "/");
	strcat(file_name, dev_ent->da_devname);

	if (stat(file_name, &stat_buf)) {
#ifdef DEBUG
		fprintf(stderr, "Unable to stat %s\n", file_name);
		perror("Error:");
#endif
		return (DACACC);
	}

	if ((optflg & FREE) && DEV_ALLOCATED(stat_buf))
		return (ALLOC);

	if ((optflg & LIST) && DEV_ALLOCATED(stat_buf) &&
	    (stat_buf.st_uid != uid))
		return (ALLOC_OTHER);

	if ((optflg & CURRENT) && (stat_buf.st_uid != uid))
		return (NALLOC);

	if ((stat_buf.st_mode & ~S_IFMT) == ALLOC_ERR_MODE)
		return (ALLOCERR);

	if ((list = (char *)malloc((unsigned)strlen(dev_list->dmap_devlist)
	    *sizeof (char) + 1)) == NULL)
		return (SYSERROR);

	strcpy(list, dev_list->dmap_devlist);
	if (check_devs(list) == -1) {
		free(list);
		return (DSPMISS);
	}

	print_dev(dev_list);

	free(list);
	return (0);
}


list_devices(optflg, uid, device)
int	optflg;
int	uid;
char	*device;
{
	DIR   * dev_dir;
	struct dirent *dac_file;
	int	error = 0, ret_code = 1;

	if (optflg & USER) {
		if (!isso())
			return (NOTROOT);
	} else if ((uid = getuid()) == (uid_t)-1)
		return (SYSERROR);

	if (allocate_uid() == (uid_t)-1)
		return (SYSERROR);

	setdaent();

	if (device) {
		error = list_device(optflg, uid, device);
		return (error);
	}


	if ((dev_dir = opendir(DAC_DIR)) == NULL) {

#ifdef DEBUG
		perror("Can't open DAC_DIR\n");
#endif
		return (DACACC);
	}

	while ((dac_file = readdir(dev_dir)) != NULL) {
		if ((strcmp(dac_file->d_name, ".") == 0) ||
		    (strcmp(dac_file->d_name, "..") == 0)) {
			continue;
		} else {
			error = list_device(optflg, uid, dac_file->d_name);
			ret_code = ret_code ? error : ret_code;
		}
	}
	closedir(dev_dir);
	enddaent();
	return (ret_code);
}

mk_dac(file, uid)
char	*file;
int	uid;
{
	if (chown(file, uid, getgid()) == -1) {
#ifdef DEBUG
		perror("mk_dac, unable to chown\n");
#endif
		return (CHOWN_PERR);
	}

	if (chmod(file, ALLOC_MODE) == -1) {
#ifdef DEBUG
		perror("mk_dac, unable to chmod\n");
#endif
		return (CHMOD_PERR);
	}

	return (0);
}


unmk_dac(file)
char	*file;
{
	int	error = 0;

	if (chown(file, alloc_uid, alloc_gid) == -1) {
#ifdef DEBUG
		perror("set_alloc_err, unable to chown\n");
#endif
		error = CHOWN_PERR;
	}

	if (chmod(file, DEALLOC_MODE) == -1) {
#ifdef DEBUG
		perror("set_alloc_err, unable to chmod\n");
#endif
		error = CHMOD_PERR;
	}

	return (error);
}


set_alloc_err(file)
char	*file;
{
	if (chown(file, alloc_uid, alloc_gid) == -1) {
#ifdef DEBUG
		perror("set_alloc_err, unable to chown\n");
#endif
		return (CHOWN_PERR);
	}

	if (chmod(file, ALLOC_ERR_MODE) == -1) {
#ifdef DEBUG
		perror("set_alloc_err, unable to chmod\n");
#endif
		return (CHMOD_PERR);
	}

	return (0);
}


lock_dev(file)
char	*file;
{
	int	fd;

	if ((fd = open(file, O_RDONLY)) == -1) {
#ifdef DEBUG
		perror("lock_dev, cannot open DAC file\n");
#endif
		return (DACACC);
	}

	if (lockf(fd, F_TLOCK, 0) == -1) {
#ifdef DEBUG
		perror("lock_dev, cannot set lock\n");
#endif
		return (DACLCK);
	}

	close(fd);
	return (0);
}


unlock_dev(file)
char	*file;
{
	int	fd;

	if ((fd = open(file, O_RDONLY)) == -1) {
#ifdef DEBUG
		perror("lock_dev, cannot open DAC file\n");
#endif
		return (DACACC);
	}

	if (lockf(fd, F_ULOCK, 0) == -1) {
#ifdef DEBUG
		perror("lock_dev, cannot remove lock\n");
#endif
		return (DACLCK);
	}

	close(fd);
	return (0);
}


mk_alloc(list, uid)
char	*list;
int	uid;
{
	char	*file;
	int	gid;

	gid = getgid();
	audit_allocate_list(list);

	file = strtok(list, " ");
	while (file != NULL) {

#ifdef DEBUG
		fprintf(stderr, "Allocating %s\n", file);
#endif
		if (chown(file, uid, gid) == -1) {
#ifdef DEBUG
			perror("mk_alloc, unable to chown\n");
#endif
			return (CHOWN_PERR);
		}

		if (chmod(file, ALLOC_MODE) == -1) {
#ifdef DEBUG
			perror("mk_alloc, unable to chmod\n");
#endif
			return (CHMOD_PERR);
		}

		file = strtok(NULL, " ");
	}
	return (0);
}

/*
 * mk_revoke() is used instead of system("/usr/sbin/fuser -k file")
 * because "/usr/sbin/fuser -k file" kills all processes
 * working with the file, even "vold" (bug #4095152).
 */
static int
mk_revoke(char *file)
{
	char buf[64];
	int r = 0, p[2], fp, lock;
	FILE *ptr;
	prpsinfo_t info;
	pid_t pid, c_pid;

	strcpy(buf, "/proc/");

	/*
	 * vfork() and execl() just to make the same output
	 * as before fixing of bug #4095152.
	 * The problem is that the "fuser" command prints
	 * one part of output into stderr and another into stdout,
	 * but user sees them mixed. Of course, better to change "fuser"
	 * or to intercept and not to print its output.
	 */
	c_pid = vfork();
	if (c_pid == -1)
		return (-1);
	if (c_pid == 0) {
		execl("/usr/sbin/fuser", "fuser", file, NULL);
		_exit(1);
	}

	waitpid(c_pid, &lock, 0);
	if (WEXITSTATUS(lock) != 0)
		return (-1);

	if (pipe(p))
		return (-1);

	/* vfork() and execl() to catch output and to process it */
	c_pid = vfork();
	if (c_pid == -1)
		return (-1);

	if (c_pid == 0) {
		close(p[0]);
		close(1);
		fcntl(p[1], F_DUPFD, 1);
		close(p[1]);
		close(2);
		execl("/usr/sbin/fuser", "fuser", file, NULL);
		_exit(1);
	}

	close(p[1]);
	if ((ptr = fdopen(p[0], "r")) != NULL) {
		while (!feof(ptr))
		if (r = fscanf(ptr, "%d", &pid) > 0) {
			sprintf(buf + 6, "%d", pid);
			fp = open(buf, O_RDONLY);
			if (fp == -1) {
				r = -1;
				continue;
			}
			if (ioctl(fp, PIOCPSINFO, (char *)&info) == -1) {
				r = -1;
				close(fp);
				continue;
			}
			close(fp);
			if (strcmp(info.pr_fname, "vold") == NULL)
				continue;
			r = kill(pid, SIGKILL);
		} else if (r == -1)
			continue;
	} else
		r = -1;

	fclose(ptr);
	return (r);
}

mk_unalloc(list)
char	*list;
{
	char	*file;
	int	error = 0, r;
	char	base[256];
	int child, status;

	audit_allocate_list(list);

	child = vfork();
	switch (child) {
	case -1:
		return (-1);
	case 0:
		setuid(0);
		file = strtok(list, " ");
		while (file != NULL) {

#ifdef DEBUG
			fprintf(stderr, "Deallocating %s\n", file);
#endif

			if ((r = mk_revoke(file)) < 0) {
#ifdef DEBUG
				fprintf("mk_unalloc: unable to revoke %s\n",
					file);
				perror("");
#endif
				error = CNTFRC;
			}

			if (chown(file, alloc_uid, alloc_gid) == -1) {
#ifdef DEBUG
				perror("mk_unalloc, unable to chown\n");
#endif
				error = CHOWN_PERR;
			}

			if (chmod(file, DEALLOC_MODE) == -1) {
#ifdef DEBUG
				perror("mk_unalloc, unable to chmod\n");
#endif
				error = CHMOD_PERR;
			}

			file = strtok(NULL, " ");
		}
		exit(error);
	default:
		while (wait(&status) != child);
		if (WIFEXITED(status)) {
			return (WEXITSTATUS(status));
		}
		return (-1);
	}
}


exec_clean(optflg, name, path)
int	optflg;
char	*name, *path;
{
	char	mode[8], *cmd, *info_ascii = NULL;
	int	status;
	int	c;

	if (optflg & FORCE_ALL)
		sprintf(mode, "-i");
	else if (optflg & FORCE)
		sprintf(mode, "-f");
	else
		sprintf(mode, "-s");
	if ((cmd = strrchr(path, '/')) == NULL)
		cmd = path;
	c = vfork();
	switch (c) {
	case -1:
		return (-1);
	case 0:
		setuid(0);
		execl(path, cmd, mode, name, info_ascii, NULL);
#ifdef DEBUG
		fprintf(stderr, "Unable to execute clean up script %s\n",
			path);
		perror("");
#endif
		exit(CNTDEXEC);
	default:
		while (wait(&status) != c);
		if (WIFEXITED(status))
			return (WEXITSTATUS(status));
#ifdef DEBUG
		fprintf(stderr, "exit status %d\n", status);
#endif
		return (-1);
	}
}


allocate_dev(optflg, uid, dev_ent)
	int	optflg;
	uid_t	uid;
	devalloc_t *dev_ent;
{
	devmap_t *dev_list;
	char	file_name[1024];
	struct stat stat_buf;
	char	*list;
	int	error = 0;

	strcpy(file_name, DAC_DIR);
	strcat(file_name, "/");
	strcat(file_name, dev_ent->da_devname);
	audit_allocate_device(file_name);			/* BSM */

	if (stat(file_name, &stat_buf)) {
#ifdef DEBUG
		fprintf(stderr, "Unable to stat %s\n", file_name);
		perror("Error:");
#endif
		return (DACACC);
	}

	if (DEV_ALLOCATED(stat_buf)) {
		if (optflg & FORCE) {
			if (deallocate_dev(FORCE, dev_ent, uid)) {
#ifdef DEBUG
				fprintf(stderr,
					"Couldn't force deallocate device\n");
#endif
				return (CNTFRC);
			}
		} else if (stat_buf.st_uid == uid) {
			return (ALLOC);
		} else
			return (ALLOC_OTHER);
	}
	if ((stat_buf.st_mode & ~S_IFMT) == ALLOC_ERR_MODE)
		return (ALLOCERR);

	if (strcmp(dev_ent->da_devauth, "*") == 0) {
#ifdef DEBUG
		fprintf(stderr,
			"Device %s is not allocatable\n", dev_ent->da_devname);
#endif
		return (AUTHERR);
	}

	if (strcmp(dev_ent->da_devauth, "@")) {
		if (!is_authorized(dev_ent->da_devauth, uid)) {
#ifdef DEBUG
			fprintf(stderr, "User %d is unauthorized to allocate\n",
				uid);
#endif
			return (IMPORT_ERR);
		}
	}

	if ((dev_list = getdmapnam(dev_ent->da_devname)) == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Unable to find %s in device map database\n",
			dev_ent->da_devname);
#endif
		return (NODMAPENT);
	}

	if ((list = (char *)malloc((unsigned)strlen(dev_list->dmap_devlist)
	    *sizeof (char) + 1)) == NULL)
		return (SYSERROR);

	strcpy(list, dev_list->dmap_devlist);
	if (check_devs(list) == -1) {
		set_alloc_err(file_name);
		free(list);
		return (DSPMISS);
	}

	/* All checks passed, time to lock and allocate */
	if (lock_dev(file_name) == -1) {
		free(list);
		return (DACLCK);
	}

	if ((error = mk_dac(file_name, uid)) != 0) {
		set_alloc_err(file_name);
		unlock_dev(file_name);
		free(list);
		return (error);
	}

	strcpy(list, dev_list->dmap_devlist);
	if (mk_alloc(list, uid) != 0) {
		set_alloc_err(file_name);
		strcpy(list, dev_list->dmap_devlist);
		mk_unalloc(list);
		unlock_dev(file_name);
		free(list);
		return (DEVLST);
	}

	free(list);
	return (0);
}







allocate(optflg, uid, device, label)
int	optflg;
int	uid;
char	*device, *label;
{
	devalloc_t	*dev_ent;
	devmap_t	*dev_list;
	int	error = 0;

	if (optflg & (USER | FORCE) && !isso())
		return (NOTROOT);

	if (!(optflg & USER) && ((uid = getuid()) == -1))
		return (SYSERROR);

	if (allocate_uid() == (uid_t)-1)
		return (SYSERROR);

	setdaent();
	setdmapent();

	if (!(optflg & TYPE)) {
		if ((dev_ent = getdanam(device)) == NULL) {
			if ((dev_list = getdmapdev(device)) == NULL)
				return (NODMAPENT);
			else if ((dev_ent = getdanam(dev_list->dmap_devname))
			    == NULL)
				return (NODAENT);
		}
		error = allocate_dev(optflg, uid, dev_ent);

		return (error);
	}

	while ((dev_ent = getdatype(device)) != NULL) {
#ifdef DEBUG
		fprintf(stderr, "trying to allocate %s\n", dev_ent->da_devname);
#endif
		if (!allocate_dev(optflg, uid, dev_ent)) {
			return (0);
		}
	}
	enddaent();
	return (NO_DEVICE);
}


deallocate_dev(optflg, dev_ent, uid)
int	optflg;
devalloc_t   *dev_ent;
int	uid;
{
	devmap_t *dev_list;
	char	file_name[1024];
	struct stat stat_buf;
	char	*list;
	int	error = 0, err = 0;

	strcpy(file_name, DAC_DIR);
	strcat(file_name, "/");
	strcat(file_name, dev_ent->da_devname);

	audit_allocate_device(file_name); /* BSM */

	if (stat(file_name, &stat_buf)) {
#ifdef DEBUG
		fprintf(stderr, "Unable to stat %s\n", file_name);
		perror("Error:");
#endif
		return (DACACC);
	}

	if (!(optflg & FORCE) && stat_buf.st_uid != uid &&
	    DEV_ALLOCATED(stat_buf)) {
		return (NALLOCU);
	}

	if (!DEV_ALLOCATED(stat_buf)) {
		if ((stat_buf.st_mode & ~S_IFMT) == ALLOC_ERR_MODE) {
			if (!(optflg & FORCE))
				return (ALLOCERR);
		} else
			return (NALLOC);
	}

	if ((err = unmk_dac(file_name)) != 0) {
		set_alloc_err(file_name);
		EXIT(err);
	}

	if ((dev_list = getdmapnam(dev_ent->da_devname)) == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Unable to find %s ", dev_ent->da_devname);
		fprintf(stderr, "in the device map database\n");
#endif
		EXIT(NODMAPENT);
	} else {
		if ((list = (char *)malloc(strlen(dev_list->dmap_devlist) *
				sizeof (char) + 1)) == NULL)
			EXIT(SYSERROR)
		else {
			strcpy(list, dev_list->dmap_devlist);
			if (mk_unalloc(list) != 0) {
				set_alloc_err(file_name);
				free(list);
				list = NULL;
				EXIT(DEVLST);
			}
		}
	}

	if (!error || optflg & FORCE_ALL)
		if ((err = unlock_dev(file_name)) != 0) {
			set_alloc_err(file_name);
			EXIT(err);
		}

	if (list != NULL)
		free(list);
	if (exec_clean(optflg, dev_ent->da_devname, dev_ent->da_devexec))
		EXIT(CLEAN_ERR);
	return (error);
}





deallocate(optflg, device)
int	optflg;
char	*device;
{
	DIR   *dev_dir;
	struct dirent	*dac_file;
	devalloc_t	*dev_ent;
	devmap_t	*dev_list;
	int	error = 0;
	int	uid;
	char	*f_aud_event, *s_aud_event;

	if (optflg & (FORCE | FORCE_ALL) && !isso())
		return (NOTROOT);
	if (optflg & FORCE_ALL)
		optflg |= FORCE;

	if (allocate_uid() == -1)
		return (SYSERROR);

	if (!(optflg & USER) && ((uid = getuid()) == (uid_t)-1))
		return (SYSERROR);

	setdaent();
	setdmapent();

	if (!(optflg & FORCE_ALL)) {
		if ((dev_ent = getdanam(device)) == NULL) {
			if ((dev_list = getdmapdev(device)) == NULL)
				return (NODMAPENT);
			else if ((dev_ent = getdanam(dev_list->dmap_devname))
			    == NULL)
				return (NODAENT);
		}

		error = deallocate_dev(optflg, dev_ent, uid);
		return (error);
	}

	if ((dev_dir = opendir(DAC_DIR)) == NULL) {

#ifdef DEBUG
		perror("Can't open DAC_DIR\n");
#endif
		return (DACACC);
	}

	while ((dac_file = readdir(dev_dir)) != NULL) {
		if ((strcmp(dac_file->d_name, ".") == 0) ||
		    (strcmp(dac_file->d_name, "..") == 0)) {
			continue;
		} else {
			if ((dev_ent = getdanam(dac_file->d_name)) == NULL) {
				error = NODAENT;
				continue;
			}
			error = deallocate_dev(optflg, dev_ent, uid);
		}
	}
	closedir(dev_dir);
	enddaent();
	return (error);
}

/*
 * Checks if the specified user has any of the authorizations in the
 * list of authorizations
 */
int
is_authorized(char *auth_list, uid_t uid)
{
	char	*auth;
	struct passwd *pw;

	pw = getpwuid(uid);
	if (pw == NULL) {
#ifdef DEBUG
		fprintf(stderr, "Can't get user info for uid=%d\n", getuid());
#endif
		return (0);
	}

	auth = strtok(auth_list, DEVICE_AUTH_SEPARATOR);
	while (auth != NULL) {
		if (chkauthattr(auth, pw->pw_name))
			return (1);
		auth = strtok(NULL, DEVICE_AUTH_SEPARATOR);
	}
	return (0);
}
