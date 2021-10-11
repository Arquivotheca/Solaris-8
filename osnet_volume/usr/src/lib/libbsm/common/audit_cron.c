#ifndef lint
static char sccsid[] = "@(#)audit_cron.c 1.17 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <sys/systeminfo.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include <unistd.h>
#include <wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <locale.h>
#include "generic.h"

#define	F_AUID	"%u\n"
#define	F_SMASK	"%x\n"
#define	F_FMASK	"%x\n"
#define	F_PORT	"%x\n"
#define	F_TYPE	"%x\n"
#define	F_MACH	"%x %x %x %x\n"
#define	F_ASID	"%u\n"

#define	AU_SUFFIX	".au"

#define	ANC_BAD_FILE	-1
#define	ANC_BAD_FORMAT	-2

#define	AUDIT_CRON_TEXTBUF	256
static char	textbuf[AUDIT_CRON_TEXTBUF];

int
audit_cron_mode()
{
	return (!cannot_audit(0));
}

static int
audit_cron_getinfo(char *fname, struct auditinfo_addr *info)
{
	int		fd;
	int		save_err;
	struct stat	st;


	if ((fd = open(fname, O_RDONLY)) == -1)
		return (ANC_BAD_FILE);

	if (fstat(fd, &st) == -1) {
		save_err = errno;
		goto audit_getinfo_clean;
	}

	if (read(fd, textbuf, st.st_size) != st.st_size) {
		save_err = errno;
		goto audit_getinfo_clean;
	}

	if (sscanf(textbuf,
			F_AUID
			F_SMASK
			F_FMASK
			F_PORT
			F_TYPE
			F_MACH
			F_ASID,
				&(info->ai_auid),
				&(info->ai_mask.am_success),
				&(info->ai_mask.am_failure),
				&(info->ai_termid.at_port),
				&(info->ai_termid.at_type),
				&(info->ai_termid.at_addr[0]),
				&(info->ai_termid.at_addr[1]),
				&(info->ai_termid.at_addr[2]),
				&(info->ai_termid.at_addr[3]),
				&(info->ai_asid)) != 10) {
		close(fd);
		return (ANC_BAD_FORMAT);
	}

	close(fd);
	return (0);

audit_getinfo_clean:
	close(fd);
	errno = save_err;
	return (ANC_BAD_FILE);
}

int
audit_cron_setinfo(char *fname, struct auditinfo_addr *info)
{
	int		fd, len, r;
	int		save_err;

	r = chmod(fname, 0200);
	if (r == -1 && errno != ENOENT)
		return (-1);

	if ((fd = open(fname, O_CREAT|O_WRONLY, 0200)) == -1)
		return (-1);

	len = sprintf(textbuf,
			F_AUID
			F_SMASK
			F_FMASK
			F_PORT
			F_TYPE
			F_MACH
			F_ASID,
				info->ai_auid,
				info->ai_mask.am_success,
				info->ai_mask.am_failure,
				info->ai_termid.at_port,
				info->ai_termid.at_type,
				info->ai_termid.at_addr[0],
				info->ai_termid.at_addr[1],
				info->ai_termid.at_addr[2],
				info->ai_termid.at_addr[3],
				info->ai_asid);

	if (write(fd, textbuf, len) != len)
		goto audit_setinfo_clean;

	if (fchmod(fd, 0400) == -1)
		goto audit_setinfo_clean;

	close(fd);
	return (0);

audit_setinfo_clean:
	save_err = errno;
	close(fd);
	unlink(fname);
	errno = save_err;
	return (-1);
}

char *
audit_cron_make_anc_name(char *fname)
{
	char *anc_name;

	anc_name = (char *)malloc(strlen(fname) + 4);
	if (anc_name == NULL)
		return (NULL);

	strcpy(anc_name, fname);
	strcat(anc_name, AU_SUFFIX);
	return (anc_name);
}

int
audit_cron_is_anc_name(char *name)
{
	int	pos;

	pos = strlen(name) - strlen(AU_SUFFIX);
	if (pos <= 0)
		return (0);

	if (strcmp(name + pos, AU_SUFFIX) == 0)
		return (1);

	return (0);
}

static void
audit_cron_session_failure(char *name, int type, char *err_str)
{
	char		*mess;

	if (type == 0)
		mess = dgettext(bsm_dom,
		"at-job session for user %s failed: ancillary file: %s");
	else
		mess = dgettext(bsm_dom,
		"crontab job session for user %s failed: ancillary file: %s");

	(void) sprintf(textbuf, mess, name, err_str);

	aug_save_event(AUE_cron_invoke);
	aug_save_sorf(4);
	aug_save_text(textbuf);
	aug_audit();
}


int
audit_cron_session(
		char *name,
		char *path,
		uid_t uid,
		gid_t gid,
		char *at_jobname)
{
	struct auditinfo_addr	info;
	au_mask_t		mask;
	char			*anc_file, *fname;
	int			r = 0;
	char			full_path[PATH_MAX];

	if (cannot_audit(0)) {
		return (0);
	}

	/* get auditinfo from ancillary file */
	if (at_jobname == NULL) {
		/*
		 *	this is a cron-event, so we can get
		 *	filename from "name" arg
		 */
		fname = name;
		if (path != NULL) {
			if (strlen(path) + strlen(fname) + 2 > PATH_MAX) {
				errno = ENAMETOOLONG;
				r = -1;
			}
			strcat(strcat(strcpy(full_path, path), "/"), fname);
			fname = full_path;
		}
	} else {
		/* this is an at-event, use "at_jobname" */
		fname = at_jobname;
	}

	if (r == 0) {
		anc_file = audit_cron_make_anc_name(fname);
		if (anc_file == NULL) {
			r = -1;
		} else {
			r = audit_cron_getinfo(anc_file, &info);
		}
	}

	if (r != 0) {
		char *err_str;

		if (r == ANC_BAD_FORMAT)
			err_str = dgettext(bsm_dom, "bad format");
		else
			err_str = strerror(errno);

		audit_cron_session_failure(name,
					at_jobname == NULL,
					err_str);
		if (anc_file != NULL)
			free(anc_file);
		return (r);
	}

	free(anc_file);
	aug_init();

	/* get current audit masks */
	if (au_user_mask(name, &mask) == 0) {
		info.ai_mask.am_success  |= mask.am_success;
		info.ai_mask.am_failure  |= mask.am_failure;
	}

	/* save audit attributes for further use in current process */
	aug_save_auid(info.ai_auid);
	aug_save_asid(info.ai_asid);
	aug_save_tid_ex(info.ai_termid.at_port, info.ai_termid.at_addr,
		info.ai_termid.at_type);
	aug_save_pid(getpid());
	aug_save_uid(uid);
	aug_save_gid(gid);
	aug_save_euid(uid);
	aug_save_egid(gid);

	/* set mixed audit masks */
	return (setaudit_addr(&info, sizeof(info)));
}

/*
 * audit_cron_new_job - create audit record with an information
 *			about new job started by cron.
 *	args:
 *	cmd  - command being run by cron daemon.
 *	type - type of job (0 - at-job, 1 - crontab job).
 *	event - not used. pointer to cron event structure.
 */
/*ARGSUSED*/
void
audit_cron_new_job(char *cmd, int type, void *event)
{
	if (cannot_audit(0))
		return;

	if (type == 0) {
	    (void) sprintf(textbuf, dgettext(bsm_dom, "at-job"));
	} else if (type == 1) {
	    (void) sprintf(textbuf, dgettext(bsm_dom, "batch-job"));
	} else if (type == 2) {
	    (void) sprintf(textbuf, dgettext(bsm_dom, "crontab-job"));
	} else if ((type > 2) && (type <= 25)) {	/* 25 from cron.h */
	    (void) sprintf(textbuf,
		    dgettext(bsm_dom, "queue-job (%c)"), (type+'a'));
	} else {
	    (void) sprintf(textbuf,
		    dgettext(bsm_dom, "unknown job type (%d)"), type);
	}

	aug_save_event(AUE_cron_invoke);
	aug_save_sorf(0);
	aug_save_text(textbuf);
	aug_save_text1(cmd);
	aug_audit();
}

void
audit_cron_bad_user(char *name)
{
	if (cannot_audit(0))
		return;

	(void) sprintf(textbuf,
			dgettext(bsm_dom, "bad user %s"), name);

	aug_save_event(AUE_cron_invoke);
	aug_save_sorf(2);
	aug_save_text(textbuf);
	aug_audit();
}

void
audit_cron_user_acct_expired(char *name)
{
	if (cannot_audit(0))
		return;

	(void) sprintf(textbuf,
			dgettext(bsm_dom,
				"user %s account expired"), name);

	aug_save_event(AUE_cron_invoke);
	aug_save_sorf(3);
	aug_save_text(textbuf);
	aug_audit();
}

int
audit_cron_create_anc_file(char *name, char *path, char *uname, uid_t uid)
{
	au_mask_t	msk;
	auditinfo_addr_t ai;
	int		pid;
	char		*anc_name;
	char		full_path[PATH_MAX];

	if (cannot_audit(0))
		return (0);

	if (name == NULL)
		return (0);

	if (path != NULL) {
		if (strlen(path) + strlen(name) + 2 > PATH_MAX)
			return (-1);
		strcat(strcat(strcpy(full_path, path), "/"), name);
		name = full_path;
	}
	anc_name = audit_cron_make_anc_name(name);

	if (access(anc_name, F_OK) != 0) {
		if (au_user_mask(uname, &msk) != 0) {
			free(anc_name);
			return (-1);
		}

		ai.ai_mask = msk;
		ai.ai_auid = uid;
		ai.ai_termid.at_port = 0;
		ai.ai_termid.at_type = AU_IPv4;
		ai.ai_termid.at_addr[0] = 0;
		ai.ai_termid.at_addr[1] = 0;
		ai.ai_termid.at_addr[2] = 0;
		ai.ai_termid.at_addr[3] = 0;
		/* generate new pid to use it as asid */
		pid = vfork();
		if (pid == -1) {
			free(anc_name);
			return (-1);
		}
		if (pid == 0)
			exit(0);
		else {
		/*
		 * we need to clear status of children for
		 * wait() call in "cron"
		 */
			int lock;

			waitpid(pid, &lock, 0);
		}
		ai.ai_asid = pid;
		if (audit_cron_setinfo(anc_name, &ai) != 0) {
			free(anc_name);
			return (-1);
		}
	}

	free(anc_name);
	return (0);
}

int
audit_cron_delete_anc_file(char *name, char *path)
{
	char	*anc_name;
	char	full_path[PATH_MAX];
	int	r;

	if (name == NULL)
		return (0);

	if (path != NULL) {
		if (strlen(path) + strlen(name) + 2 > PATH_MAX)
			return (-1);
		strcat(strcat(strcpy(full_path, path), "/"), name);
		name = full_path;
	}
	anc_name = audit_cron_make_anc_name(name);
	r = unlink(anc_name);
	free(anc_name);
	return (r);
}
