#
#pragma ident	"@(#)Makefile	1.6	98/10/09 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#

include		../Makefile.com

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1

.KEEP_STATE:

include		../Makefile.targ
