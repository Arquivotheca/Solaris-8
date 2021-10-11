#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

#ifdef C2_DEBUG
#define dprintf(x) {printf x;}
#else
#define dprintf(x)
#endif

/* ARGSUSED */
void
audit_su_init_info(username, ttyn)
	char *username;
	char *ttyn;
{
}

/* ARGSUSED */
void
audit_su_init_user(username)
	char *username;
{}

/* ARGSUSED */
void
audit_su_init_ttyn(ttyn)
	char *ttyn;
{}

/* ARGSUSED */
void
audit_su_init_expired(username)
	char *username;
{}

void
audit_su_init_ai()
{}

void
audit_su_init_id()
{}

void
audit_su_reset_ai()
{}

void
audit_su_success()
{}

void
audit_su_bad_username()
{}

void
audit_su_bad_authentication()
{}

/* ARGSUSED */
void
audit_su_bad_uid(uid)
	uid_t uid;
{}

void
audit_su_unknown_failure()
{}

/* ARGSUSED */
audit_su(s, r)
	char *s;
	int r;
{}
