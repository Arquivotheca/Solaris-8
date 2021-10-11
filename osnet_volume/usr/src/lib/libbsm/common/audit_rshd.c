/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
#ident	"@(#)audit_rshd.c	1.21	99/10/14 SMI"

#include <sys/types.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <netinet/in.h>
#include <locale.h>
#include <unistd.h>
#include <generic.h>

extern int	errno;

static au_event_t	rshd_event;	/* audit event number */
static uint32_t		rshd_addr[4];	/* peer address */
static uint32_t		rshd_type;	/* peer type */
static uint32_t		rshd_port;	/* port name */

static void generate_record();
static void setup_session();
static selected();

audit_rshd_setup()
{
	rshd_event = AUE_rshd;
	return (0);
}


audit_rshd_fail(msg, hostname, remuser, locuser, cmdbuf)
char	*msg;		/* message containing failure information */
char	*hostname;		/* hostname of machine requesting service */
char	*remuser;		/* username at machine requesting service */
char	*locuser;		/* username of local machine */
char	*cmdbuf;		/* command line to be executed locally */
{
	if (cannot_audit(0)) {
		return (0);
	}
	generate_record(hostname, remuser, locuser, cmdbuf, -1, msg);
	return (0);
}


audit_rshd_success(hostname, remuser, locuser, cmdbuf)
char	*hostname;		/* hostname of machine requesting service */
char	*remuser;		/* username at machine requesting service */
char	*locuser;		/* username at local machine */
char	*cmdbuf;		/* command line to be executed locally */
{
	if (cannot_audit(0)) {
		return (0);
	}
	generate_record(hostname, remuser, locuser, cmdbuf, 0, "");
	setup_session(hostname, remuser, locuser);
	return (0);
}


#include <pwd.h>

static void
generate_record(hostname, remuser, locuser, cmdbuf, sf_flag, msg)
char	*hostname;	/* hostname of machine requesting service */
char	*remuser;		/* username at machine requesting service */
char	*locuser;		/* username of local machine */
char	*cmdbuf;		/* command line to be executed locally */
int	sf_flag;		/* success (0) or failure (-1) flag */
char	*msg;		/* message containing failure information */
{
	int	rd;		/* audit record descriptor */
	char	buf[256];	/* temporary buffer */
	char	*tbuf;		/* temporary buffer */
	uid_t	uid;
	gid_t	gid;
	pid_t	pid;
	au_tid_addr_t	tid;
	struct passwd *pwd;
	uint32_t addr[4], type;
	int rc;
	struct auditinfo_addr info;

	if (cannot_audit(0)) {
		return;
	}

	pwd = getpwnam(locuser);
	if (pwd == NULL) {
		uid = -1;
		gid = -1;
	} else {
		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
	}

	if (!selected(uid, locuser, remuser, rshd_event, sf_flag))
		return;

	pid = getpid();

	/* see if terminal id already set */
	if (getaudit_addr(&info, sizeof(info)) < 0) {
		perror("getaudit");
	}
	rshd_port = info.ai_termid.at_port;

	rd = au_open();

	au_write(rd, au_to_subject_ex(uid, uid, gid, uid, gid, pid, pid,
		&info.ai_termid));

	if ((tbuf = (char *) malloc(strlen(cmdbuf)+64)) == (char *) 0) {
		au_close(rd, 0, 0);
		return;
	}
	(void) sprintf(tbuf, dgettext(bsm_dom, "cmd %s"), cmdbuf);
	au_write(rd, au_to_text(tbuf));
	(void) free(tbuf);

	if (strcmp(remuser, locuser) != 0) {
		(void) sprintf(buf, dgettext(bsm_dom,
			"remote user %s"), remuser);
		au_write(rd, au_to_text(buf));
	}

	if (sf_flag == -1) {
		(void) sprintf(buf, dgettext(bsm_dom,
			"local user %s"), locuser);
		au_write(rd, au_to_text(buf));
		au_write(rd, au_to_text(msg));
	}

#ifdef _LP64
	au_write(rd, au_to_return64(sf_flag, (int64_t)0));
#else
	au_write(rd, au_to_return32(sf_flag, (int32_t)0));
#endif

	if (au_close(rd, 1, rshd_event) < 0) {
		au_close(rd, 0, 0);
	}
}


static
selected(uid, locuser, remuser, event, sf)
uid_t uid;
char	*locuser, *remuser;
au_event_t	event;
int	sf;
{
	int	rc, sorf;
	char	naflags[512];
	struct au_mask mask;

	mask.am_success = mask.am_failure = 0;
	if (uid < 0) {
		rc = getacna(naflags, 256); /* get non-attrib flags */
		if (rc == 0)
			getauditflagsbin(naflags, &mask);
	} else {
		rc = au_user_mask(locuser, &mask);
	}

	if (sf == 0)
		sorf = AU_PRS_SUCCESS;
	else if (sf == -1)
		sorf = AU_PRS_FAILURE;
	else
		sorf = AU_PRS_BOTH;
	rc = au_preselect(event, &mask, sorf, AU_PRS_REREAD);
	return (rc);
}


static void
setup_session(hostname, remuser, locuser)
char	*hostname, *remuser, *locuser;
{
	int	rc;
	struct auditinfo_addr info;
	au_mask_t		mask;
	uid_t			uid;
	struct passwd *pwd;
	uint32_t addr[4], type;

	pwd = getpwnam(locuser);
	if (pwd == NULL)
		uid = -1;
	else
		uid = pwd->pw_uid;

	/* see if terminal id already set */
	if (getaudit_addr(&info, sizeof(info)) < 0) {
		perror("getaudit");
	}

	info.ai_auid = uid;
	info.ai_asid = getpid();

	mask.am_success = 0;
	mask.am_failure = 0;
	au_user_mask(locuser, &mask);

	info.ai_mask.am_success = mask.am_success;
	info.ai_mask.am_failure = mask.am_failure;

	rshd_port = info.ai_termid.at_port;
	rshd_type = info.ai_termid.at_type;
	rshd_addr[0] = info.ai_termid.at_addr[0];
	rshd_addr[1] = info.ai_termid.at_addr[1];
	rshd_addr[2] = info.ai_termid.at_addr[2];
	rshd_addr[3] = info.ai_termid.at_addr[3];

	rc = setaudit_addr(&info, sizeof(info));
	if (rc < 0) {
		perror("setaudit");
	}
}
