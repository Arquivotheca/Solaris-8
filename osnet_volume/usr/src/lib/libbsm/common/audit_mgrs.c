#ifndef lint
static char sccsid[] = "@(#)audit_mgrs.c 1.2 98/05/06 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include "generic.h"

#ifdef C2_DEBUG
#define	dprintf(x) { printf x; }
#else
#define	dprintf(x)
#endif

/* constant for user account enable/disable state change */

#define AC_STATE_UNCHANGED      -99

/* Constants used for password type interpretation in BSM auditing */

#define PWD_NONE_CODE           0
#define PWD_CLEARED_CODE        1
#define PWD_LOCKED_CODE         2
#define PWD_NORMAL_CODE         3
#define PWD_UNCHANGED_CODE      4

#define PWD_NONE_TEXT           "No password active"
#define PWD_CLEARED_TEXT        "Cleared until first login"
#define PWD_LOCKED_TEXT         "Account is locked"
#define PWD_NORMAL_TEXT         "Normal password active"

static int  save_afunc();

static char *saved_uid_p;
static char *saved_username_p;
static char *saved_gid_p;
static char *saved_groups_p;
static char *saved_shell_p;
static char *saved_min_p;
static char *saved_max_p;
static char *saved_inactive_p;
static char *saved_expire_p;
static char *saved_warn_p;
static char *saved_home_path_p;
static char *saved_home_server_p;
static char *saved_home_mode_p;
static int   saved_passwd_type_code;

static int taudit_user_dde_event_setup(au_event_t , char *);
static int audit_user_generic(int);
static int audit_users_modified_by_group_generic(char *, char *, int);

/*
 * Save user information to audit log as text tokens
 */

static int
save_afunc(int ad)
{
	char *local_passwd_type_string;

	/* Work out the password type display string */

	switch (saved_passwd_type_code) {
	case PWD_CLEARED_CODE:
		local_passwd_type_string = PWD_CLEARED_TEXT;
		break;
	case PWD_LOCKED_CODE:
		local_passwd_type_string = PWD_LOCKED_TEXT;
		break;
	case PWD_NORMAL_CODE:
		local_passwd_type_string = PWD_NORMAL_TEXT;
		break;
	case PWD_NONE_CODE:
		local_passwd_type_string = PWD_NONE_TEXT;
		break;
	case PWD_UNCHANGED_CODE:
		local_passwd_type_string = NULL;
		break;
	default: 
		/* Never reached, but if it is report as if none */
		/* to flag a potential hole in security */
		local_passwd_type_string = PWD_NONE_TEXT;
		break;
	}

	if (saved_uid_p != NULL)
        {
		au_write(ad, au_to_text(saved_uid_p));
	}
	if (saved_username_p != NULL)
        {
		au_write(ad, au_to_text(saved_username_p));
	}
	if (saved_gid_p != NULL)
        {
		au_write(ad, au_to_text(saved_gid_p));
	}
	if (saved_groups_p != NULL)
        {
		au_write(ad, au_to_text(saved_groups_p));
	}
	if (saved_shell_p != NULL)
        {
		au_write(ad, au_to_text(saved_shell_p));
	}
	if (local_passwd_type_string != NULL)
        {
		au_write(ad, au_to_text(local_passwd_type_string));
	}
	if (saved_min_p != NULL)
        {
		au_write(ad, au_to_text(saved_min_p));
	}
	if (saved_max_p != NULL)
        {
		au_write(ad, au_to_text(saved_max_p));
	}
	if (saved_inactive_p != NULL)
        {
		au_write(ad, au_to_text(saved_inactive_p));
	}
	if (saved_expire_p != NULL)
        {
		au_write(ad, au_to_text(saved_expire_p));
	}
	if (saved_warn_p != NULL)
        {
		au_write(ad, au_to_text(saved_warn_p));
	}
	if (saved_home_path_p != NULL)
        {
		au_write(ad, au_to_text(saved_home_path_p));
	}
	if (saved_home_server_p != NULL)
        {
		au_write(ad, au_to_text(saved_home_server_p));
	}
	if (saved_home_mode_p != NULL)
        {
		au_write(ad, au_to_text(saved_home_mode_p));
	}

	return (0);
}

/* 
 * Set up data for audit of user Delete/Disable or Enable Event 
 */

int
audit_user_dde_event_setup(char *uid_p)
{
	return (taudit_user_dde_event_setup(AUE_delete_user, uid_p));
}

static int
taudit_user_dde_event_setup(au_event_t id, char *uid_p)
{
	dprintf(("taudit_user_dde_event_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_init();

	aug_save_event(id);
        aug_save_text(uid_p);

	aug_save_me();

	return (0);
}

/* 
 * Audit successful or failed user create
 */

int
audit_user_create_event(char *uid_p,
			    char *username_p,
			    char *gid_p,
			    char *groups_p,
			    char *shell_p,
			    char *min_p,
			    char *max_p,
			    char *inactive_p,
		   	    char *expire_p,
		   	    char *warn_p,
		   	    char *home_path_p,
		   	    char *home_server_p,
		   	    char *home_mode_p,
      			    int  passwd_type_code,
			    int  ac_disabled,
                            int  status)

{
	dprintf(("audit_user_create_event()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	saved_uid_p 		= uid_p;
	saved_username_p 	= username_p;
	saved_gid_p 		= gid_p;
	saved_groups_p 		= groups_p;
	saved_shell_p 		= shell_p;
	saved_min_p 		= min_p;
	saved_max_p 		= max_p;
	saved_inactive_p 	= inactive_p;
	saved_expire_p 		= expire_p;
	saved_warn_p 		= warn_p;
	saved_home_path_p 	= home_path_p;
	saved_home_server_p 	= home_server_p;
	saved_home_mode_p 	= home_mode_p;
	saved_passwd_type_code	= passwd_type_code;

	aug_init();

	aug_save_event(AUE_create_user);

	aug_save_me();

        aug_save_afunc(save_afunc);

	if (status != 0)
        {
		audit_user_generic(-1);
        }
        else
        {
		audit_user_generic(0);
        }

        if (ac_disabled != AC_STATE_UNCHANGED)
        {
                if (ac_disabled)
                {
                        taudit_user_dde_event_setup(AUE_disable_user, saved_uid_p
);
                }
                else
                {
                        taudit_user_dde_event_setup(AUE_enable_user, saved_uid_p)
;
                }

                if (status != 0)
                {
                        audit_user_generic(-1);
                }
                else
                {
                        audit_user_generic(0);
                }

        }

	return (0);
}

/* 
 * Audit user modification
 */

int
audit_user_modify_event(char *uid_p,
			    char *username_p,
			    char *gid_p,
			    char *groups_p,
			    char *shell_p,
			    char *min_p,
			    char *max_p,
			    char *inactive_p,
		   	    char *expire_p,
		   	    char *warn_p,
		   	    char *home_path_p,
		   	    char *home_server_p,
      			    int  passwd_type_code,
			    int  ac_disabled,
                            int  status)

{
	dprintf(("audit_user_modify_event()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	saved_uid_p 		= uid_p;
	saved_username_p 	= username_p;
	saved_gid_p 		= gid_p;
	saved_groups_p 		= groups_p;
	saved_shell_p 		= shell_p;
	saved_min_p 		= min_p;
	saved_max_p 		= max_p;
	saved_inactive_p 	= inactive_p;
	saved_expire_p 		= expire_p;
	saved_warn_p 		= warn_p;
	saved_home_path_p 	= home_path_p;
	saved_home_server_p 	= home_server_p;
	saved_home_mode_p 	= NULL;
	saved_passwd_type_code	= passwd_type_code;

	aug_init();

	aug_save_event(AUE_modify_user);

	aug_save_me();

        aug_save_afunc(save_afunc);

	if (status != 0)
        {
		audit_user_generic(-1);
        }
        else
        {
		audit_user_generic(0);
        }

	if (ac_disabled != AC_STATE_UNCHANGED)
        {
		if (ac_disabled)
		{
			taudit_user_dde_event_setup(AUE_disable_user, saved_uid_p);
		}
		else
		{
			taudit_user_dde_event_setup(AUE_enable_user, saved_uid_p);
		}

		if (status != 0)
		{
			audit_user_generic(-1);
		}
		else
		{
			audit_user_generic(0);
		}

	}

	return (0);
}

int
audit_delete_user_fail()
{
	return (audit_user_generic(-1));
}

int
audit_delete_user_success()
{
	return (audit_user_generic(0));
}

static int
audit_user_generic(int sorf)
{
	dprintf(("audit_user_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	aug_audit();

	return (0);
}

int
audit_users_modified_by_group_success(char *unique_members, char *ID)
{
	return(audit_users_modified_by_group_generic(unique_members, ID, 0));
}

int
audit_users_modified_by_group_fail(char *members, char *ID)
{
	return(audit_users_modified_by_group_generic(members, ID, -1));
}

static int
audit_users_modified_by_group_generic(char *member_list, char *ID, int sorf)
{
	char *member_start;
   	char *member_finish;
   	int  member_len;
   	char *member;

	member_start = member_list;
	member_finish = member_list;

	while(member_finish != NULL)
	{
		member_finish=strchr(member_start,',');
		if (member_finish == NULL)
		{
			audit_user_modify_event(NULL,
						member_start,
				    		ID,
			    			NULL,
			    			NULL,
		  	    			NULL,
			    			NULL,
			  	  		NULL,
			    			NULL,
		   	    			NULL,
		   	    			NULL,
		   	   	 		NULL,
						PWD_UNCHANGED_CODE,
			    			AC_STATE_UNCHANGED,
                            			sorf);
		}
		else
		{
			member_len = member_finish - member_start;
			member=(char *)malloc(member_len + 1);

			if (member != NULL)
			{
				strncpy(member, member_start, member_len);
				member[member_len]='\0';

				audit_user_modify_event(NULL,
							member,
					    		ID,
					    		NULL,
					    		NULL,
		  	    				NULL,
			    				NULL,
			    				NULL,
			    				NULL,
		   	    				NULL,
		   	    				NULL,
		   	    				NULL,
							PWD_UNCHANGED_CODE,
			    				AC_STATE_UNCHANGED,
                            				sorf);

				free(member);
			}

			member_start = member_finish + 1;
		}

   	}
	return (0);
}

