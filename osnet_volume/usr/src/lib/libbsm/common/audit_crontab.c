#ifndef lint
static char sccsid[] = "@(#)audit_crontab.c 1.3 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <locale.h>
#include "generic.h"

#define	AUDIT_GET_DIFFS_NO_CRONTAB	1
#define	AUDIT_GET_DIFFS_CRONTAB		0
#define	AUDIT_GET_DIFFS_ERR		-1
#define	AUDIT_GET_DIFFS_NO_DIFFS	-2

extern char	*audit_cron_make_anc_name(char *);
extern int	audit_cron_setinfo(char *, auditinfo_addr_t *);

static int audit_crontab_get_diffs(char *cf, char *tmp_name, char **bufptr);

int
audit_crontab_modify(char *path, char *tmp_path, int sorf)
{
	int r, create = 0;
	char *diffs = NULL;

	if (cannot_audit(0)) {
		return (0);
	} else {
		au_event_t event;
		char *anc_name;
		auditinfo_addr_t ai;

		if (getaudit_addr(&ai, sizeof(ai))) {
			return (-1);
		}

		r = audit_crontab_get_diffs(path, tmp_path, &diffs);

		if (r == AUDIT_GET_DIFFS_NO_DIFFS) {
			return (0);
		}
		if (diffs != NULL && r != AUDIT_GET_DIFFS_ERR) {
			aug_save_text(diffs);
			free(diffs);
		}

		if (r == AUDIT_GET_DIFFS_NO_CRONTAB) {
			create = 1;
			if (diffs == NULL)
				aug_save_text("");
		}

		anc_name = audit_cron_make_anc_name(path);
		if (anc_name != NULL) {
			r = audit_cron_setinfo(anc_name, &ai);
			free(anc_name);
		} else
			r = -1;

		aug_init();
		aug_save_auid(ai.ai_auid);
		aug_save_euid(geteuid());
		aug_save_egid(getegid());
		aug_save_uid(getuid());
		aug_save_gid(getgid());
		aug_save_pid(getpid());
		aug_save_asid(ai.ai_asid);
		aug_save_tid_ex(ai.ai_termid.at_port, ai.ai_termid.at_addr,
			ai.ai_termid.at_type);


		aug_save_path(path);
		event = (create) ? AUE_crontab_create : AUE_crontab_mod;
		aug_save_event(event);
		aug_save_sorf(sorf);

		if (aug_audit() != 0)
			return (-1);
		return (r);
	}
}

int
audit_crontab_delete(char *path, int sorf)
{
	int r = 0;

	if (cannot_audit(0)) {
		return (0);
	} else {
		char *anc_name;
		anc_name = audit_cron_make_anc_name(path);
		if (anc_name != NULL) {
			r = unlink(anc_name);
			free(anc_name);
		} else
			r = -1;

		aug_init();
		aug_save_me();

		aug_save_path(path);
		aug_save_event(AUE_crontab_delete);
		aug_save_sorf(sorf);
		if (aug_audit() != 0)
			return (-1);
		return (r);
	}
}

/*
 * gets differences between old and new crontab files.
 * arguments:
 * cf        - name of crontab file
 * tmp_name  - name of new crontab file
 * bufptr    - pointer to an array of characters with
 *             either an error message or an output of "diff" command.
 *
 * results:
 * AUDIT_GET_DIFFS_ERR       - errors;
 *			file not exists (do not free *bufptr in this case)
 * AUDIT_GET_DIFFS_NO_DIFFS  - errors;
 *			file exists (do not free *bufptr in this case)
 * AUDIT_GET_DIFFS_CRONTAB      - OK, old crontab file exists.
 * AUDIT_GET_DIFFS_NO_CRONTAB   - OK. there is no crontab file.
 */
static int
audit_crontab_get_diffs(char *cf, char *tmp_name, char **bufptr)
{
	struct stat st, st_tmp;
	uid_t	euid;
	int	len, r = AUDIT_GET_DIFFS_CRONTAB;
	char	*buf = NULL, err_buf[128];
#ifdef NOTYET
	char	*diff_com = "/usr/bin/diff";
	FILE	*fres;
	char	*pos;
	int	p[2], rr;
	pid_t	pid;
#endif

	memset(err_buf, 0, 128);
	euid = geteuid();
	if (seteuid(0) == -1) {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: seteuid: %s\n", strerror(errno));
		goto exit_diff;
	}
	if (stat(cf, &st) == -1) {
		if (errno == ENOENT) {
			r = AUDIT_GET_DIFFS_NO_CRONTAB;
		} else {
			r = AUDIT_GET_DIFFS_ERR;
			sprintf(err_buf, "crontab: %s: stat: %s\n",
				cf, strerror(errno));
			goto exit_diff;
		}
		len = 0;
	} else
		len = st.st_size;

	if (stat(tmp_name, &st_tmp) == -1) {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: %s: stat: %s\n",
			tmp_name, strerror(errno));
		goto exit_diff;
	}

	if (st_tmp.st_size == 0 && len == 0) {
	/* there is no difference */
		r = AUDIT_GET_DIFFS_NO_DIFFS;
		*bufptr = NULL;
		goto exit_diff;
	}
#ifdef NOTYET
	len += st_tmp.st_size;
	len <<= 1;
	if (len < 1024)
		len = 1024;
	buf = (char *) malloc(len);
	if (buf == NULL) {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: malloc: %s\n", strerror(errno));
		goto exit_diff;
	}

	memset(buf, 0, len);

	if (r == AUDIT_GET_DIFFS_NO_CRONTAB) {
		fres = fopen(tmp_name, "r");
		if (fres == NULL) {
			r = AUDIT_GET_DIFFS_ERR;
			sprintf(err_buf, "crontab: open: %s: %s\n",
				tmp_name, strerror(errno));
			goto exit_diff;
		}
		pos = buf;
		while (1) {
			rr = fread(pos, 1, 1024, fres);
			if (rr == 0) {
				if (feof(fres))
					break;
				else {
					r = AUDIT_GET_DIFFS_ERR;
					sprintf(err_buf,
						"crontab: read: %s: %s\n",
						tmp_name, strerror(errno));
					fclose(fres);
					goto exit_diff;
				}
			}
			pos += rr;
		}
		fclose(fres);
		*pos = 0;
		goto exit_diff;
	}

	if (pipe(p) < 0) {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: pipe: %s\n", strerror(errno));
		goto exit_diff;
	}

	if ((pid = vfork()) == 0) {
		(void) close(p[0]);
		(void) close(1);
		(void) fcntl(p[1], F_DUPFD, 1);
		(void) close(p[1]);
		(void) execl(diff_com, "diff", cf, tmp_name, NULL);
		if (errno == 1)
			errno = 255;
		_exit(errno);
	}

	if (pid == -1) {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: vfork: %s\n", strerror(errno));
		goto diff_close_pipe;
	}

	(void) close(p[1]);
	if ((fres = fdopen(p[0], "r")) != NULL) {
		pos = buf;
		while (1) {
			rr = fread(pos, 1, 1024, fres);
			if (rr == 0) {
				if (feof(fres))
					break;
				else {
					r = AUDIT_GET_DIFFS_ERR;
					sprintf(err_buf,
					"crontab: read output of %s %s\n",
						diff_com, strerror(errno));
					break;
				}
			}
			pos += rr;
		}
		(void) fclose(fres);
		rr = 1;
		while (waitpid(pid, &rr, _WNOCHLD) < 0) {
			if (errno != EINTR) {
				r = AUDIT_GET_DIFFS_ERR;
				sprintf(err_buf,
				"crontab: waitpid: %s\n", strerror(errno));
				break;
			}
		}
		if (rr > 1 && r != AUDIT_GET_DIFFS_ERR) {
			r = AUDIT_GET_DIFFS_ERR;
			sprintf(err_buf,
				"crontab: execl: %s\n", strerror(errno));
		} else if (rr == 0) {
			r = AUDIT_GET_DIFFS_NO_DIFFS;
		}
		*pos = 0;
	} else {
		r = AUDIT_GET_DIFFS_ERR;
		sprintf(err_buf, "crontab: fdopen[%d]: %s\n",
				p[0], strerror(errno));
	}

diff_close_pipe:
	(void) close(p[0]);
	(void) close(p[1]);
#endif /* NOTYET */

exit_diff:
	/* return information on create or update crontab */
	seteuid(euid);
	switch (r) {
	case AUDIT_GET_DIFFS_ERR:
		if (buf != NULL)
			free(buf);
		*bufptr = err_buf;
		break;
	case AUDIT_GET_DIFFS_NO_DIFFS:
		if (buf != NULL)
			free(buf);
		*bufptr = NULL;
		break;
	case AUDIT_GET_DIFFS_CRONTAB:
		if (buf != NULL) {
			if (strlen(buf) != 0) {
				*bufptr = buf;
			} else {
				r = AUDIT_GET_DIFFS_NO_DIFFS;
				*bufptr = NULL;
			}
		}
		break;
	case AUDIT_GET_DIFFS_NO_CRONTAB:
		if (buf != NULL) {
			if (strlen(buf) != 0) {
				*bufptr = buf;
			} else {
				*bufptr = NULL;
				free(buf);
			}
		}
		break;
	}

	return (r);
}
