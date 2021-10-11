/*
 * Copyright (c) 1999 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Utils.xs	1.1	99/08/16 SMI"

/*
 * Utils.xs contains XS wrappers for utility functions needed initially by
 * Sun::Solaris::Kstat, but that should prove generally useful as well.
 */

/* Solaris includes */
#include <libgen.h>
#include <libintl.h>

/* Perl XS includes */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/*
 * The XS code exported to perl is below here.  Note that the XS preprocessor
 * has its own commenting syntax, so all comments from this point on are in
 * that form.
 */

MODULE = Sun::Solaris::Utils PACKAGE = Sun::Solaris::Utils
PROTOTYPES: ENABLE

 #
 # See gmatch(3GEN)
 #

int
gmatch(str, pattern)
	char *str;
	char *pattern;

 #
 # See gettext(3C)
 #

char *
gettext(msgid)
	char *msgid

 #
 # See dcgettext(3C)
 #

char *
dcgettext(domainname, msgid, category)
	char *domainname
	char *msgid
	int  category

 #
 # See dgettext(3C)
 #

char *
dgettext(domainname, msgid)
	char *domainname
	char *msgid

 #
 # See textdomain(3C)
 #

char *
textdomain(domain)
	char *domain

 #
 # See bindtextdomain(3C)
 #

char *
bindtextdomain(domain, dirname)
	char *domain
	char *dirname
