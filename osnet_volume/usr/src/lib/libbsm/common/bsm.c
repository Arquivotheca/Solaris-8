#ifndef lint
static char	sccsid[] = "@(#)bsm.c 1.20 99/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/syscall.h>
#include <sys/types.h>
#include <bsm/audit.h>
#include <sys/socket.h>
#include <sys/param.h>

const char *bsm_dom = TEXT_DOMAIN;

int
#ifdef __STDC__
auditsvc(fd, limit)
#else
auditsvc(fd, limit)
	int	fd;
	int	limit;
#endif
{
	return (syscall(SYS_auditsys, BSM_AUDITSVC, fd, limit));
}


int
#ifdef __STDC__
audit(char *record, int length)
#else
audit(record, length)
	char	*record;
	int	length;
#endif
{
	return (syscall(SYS_auditsys, BSM_AUDIT, record, length));
}


int
#ifdef __STDC__
audituser(char *record)
#else
audituser(record)
	char	*record;
#endif
{
	return (syscall(SYS_auditsys, BSM_AUDITUSER, record));
}


int
#ifdef __STDC__
getauid(au_id_t *auid)
#else
getauid(auid)
	au_id_t *auid;
#endif
{
	return (syscall(SYS_auditsys, BSM_GETAUID, auid));
}


int
#ifdef __STDC__
setauid(au_id_t *auid)
#else
setauid(auid)
	au_id_t *auid;
#endif
{
	return (syscall(SYS_auditsys, BSM_SETAUID, auid));
}


int
#ifdef __STDC__
getuseraudit(au_id_t uid, au_mask_t *mask)
#else
getuseraudit(uid, mask)
	au_id_t uid;
	au_mask_t *mask;
#endif
{
	return (syscall(SYS_auditsys, BSM_GETUSERAUDIT, uid, mask));
}


int
#ifdef __STDC__
setuseraudit(au_id_t uid, au_mask_t *mask)
#else
setuseraudit(uid, mask)
	au_id_t uid;
	au_mask_t *mask;
#endif
{
	return (syscall(SYS_auditsys, BSM_SETUSERAUDIT, uid, mask));
}


int
#ifdef __STDC__
getaudit(auditinfo_t *ai)
#else
getaudit(ai)
	auditinfo_t *ai;
#endif
{
	return (syscall(SYS_auditsys, BSM_GETAUDIT, ai));
}

int
#ifdef __STDC__
getaudit_addr(auditinfo_addr_t *ai, int len)
#else
getaudit_addr(ai, len)
	auditinfo_addr_t *ai;
	int	     len;
#endif
{
	return (syscall(SYS_auditsys, BSM_GETAUDIT_ADDR, ai, len));
}


int
#ifdef __STDC__
setaudit(auditinfo_t *ai)
#else
setaudit(ai)
	auditinfo_t *ai;
#endif
{
	return (syscall(SYS_auditsys, BSM_SETAUDIT, ai));
}


int
#ifdef __STDC__
setaudit_addr(auditinfo_t *ai, int len)
#else
setaudit_addr(ai, len)
	auditinfo_t *ai;
	int          len;
#endif
{
	return (syscall(SYS_auditsys, BSM_SETAUDIT_ADDR, ai, len));
}


int
#ifdef __STDC__
getkernstate(au_mask_t *mask)
#else
getkernstate(mask)
	au_mask_t *mask;
#endif
{
	return (syscall(SYS_auditsys, BSM_GETKERNSTATE, mask));
}


int
#ifdef __STDC__
setkernstate(au_mask_t *mask)
#else
setkernstate(mask)
	au_mask_t *mask;
#endif
{
	return (syscall(SYS_auditsys, BSM_SETKERNSTATE, mask));
}


int
#ifdef __STDC__
auditon(int cmd, caddr_t data, int length)
#else
auditon(cmd, data, length)
	int	cmd;
	caddr_t data;
	int	length;
#endif
{
	return (syscall(SYS_auditsys, BSM_AUDITCTL, cmd, data, length));
}


int
#ifdef __STDC__
auditstat(au_stat_t *stat)
#else
auditstat(stat)
	au_stat_t *stat;
#endif
{
	return (syscall(SYS_auditsys, BSM_AUDITSTAT, stat));
}
