#include <sys/types.h>
#include <utmpx.h>
#include <pwd.h>
#include <shadow.h>
#include <netinet/in.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

audit_login_save_flags(rflag, hflag)
	int rflag, hflag;
{
	return (0);
}

audit_login_save_host(host)
	char *host;
{
	return (0);
}

audit_login_save_ttyn(ttyn)
	char *ttyn;
{
	return (0);
}

audit_login_save_port()
{
	return (0);
}

audit_login_success()
{
	return (0);
}

audit_login_save_machine()
{
	return (0);
}

audit_login_save_pw(pwd)
	struct passwd *pwd;
{
	return (0);
}

audit_login_maxtrys()
{
	return (0);
}

audit_login_not_console()
{
	return (0);
}

audit_login_bad_pw()
{
	return (0);
}

audit_login_bad_dialup()
{
	return (0);
}

audit_cron_session(nam, uid)
	char *nam;
	uid_t uid;
{
	return (0);
}

audit_inetd_service(service_name, in_addr, iport)
	char *service_name;
	struct in_addr *in_addr;
	u_short iport;
{
	return (0);
}

audit_inetd_config()
{
	return (0);
}
audit_mountd_setup()
{
	return (0);
}

audit_mountd_mount(clname, path, success)
	char *clname;
	char *path;
	int success;
{
	return (0);
}

audit_mountd_umount(clname, path)
	char *clname;
	char *path;
{
	return (0);
}
audit_rexd_setup()
{
	return (0);
}

audit_rexd_fail(msg, hostname, user, cmdbuf)
	char *msg;
	char *hostname;
	char *user;
	char *cmdbuf;
{
	return (0);
}

audit_rexd_success(hostname, user, cmdbuf)
	char *hostname;
	char *user;
	char *cmdbuf;
{
	return (0);
}
audit_rexecd_setup()
{
	return (0);
}

audit_rexecd_fail(msg, hostname, user, cmdbuf)
	char *msg;
	char *hostname;
	char *user;
	char *cmdbuf;
{
	return (0);
}

audit_rexecd_success(hostname, user, cmdbuf)
	char *hostname;
	char *user;
	char *cmdbuf;
{
	return (0);
}
/*
 * Stubs in rshd for BSM.
 */

audit_rshd_setup()
{
	return (0);
}

audit_rshd_fail(msg, hostname, remuser, locuser, cmdbuf)
	char *msg;
	char *hostname;
	char *remuser;
	char *locuser;
	char *cmdbuf;
{
	return (0);
}

audit_rshd_success(hostname, remuser, locuser, cmdbuf)
	char *hostname;
	char *remuser;
	char *locuser;
	char *cmdbuf;
{
	return (0);
}
