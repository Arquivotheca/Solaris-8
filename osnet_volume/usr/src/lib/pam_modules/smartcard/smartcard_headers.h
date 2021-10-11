/*
 * Copyright (c) 1999, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)smartcard_headers.h 1.4     99/05/18 SMI"

#ifdef __cplusplus
extern "C" {
#endif


#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <security/pam_impl.h>
#include <syslog.h>
#include <pwd.h>
#include <shadow.h>
#include <lastlog.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libintl.h>
#include <signal.h>
#include <thread.h>
#include <synch.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <crypt.h>
#include <assert.h>

/*
 * OCF header files
 */
#include <smartcard.h>

/*
 * Various useful files and string constants
 */
#define	SHELL		"/usr/bin/sh"
#define	DEFSHELL	"/bin/sh"
#define	LASTLOG		"/var/adm/lastlog"
#define	PASSWORD_LEN	8
#define	PIN_LEN		8

/*
 * PAM_MSG macro for return of internationalized text
 */

#define	PAM_MSG(pamh, number, string)\
	(char *) __pam_get_i18n_msg(pamh, "pam_unix", 1, number, string)
