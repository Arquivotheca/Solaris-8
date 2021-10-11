#!/usr/bin/sh
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)dmesg.sh	1.1	98/09/30 SMI"

/usr/bin/echo
/usr/bin/date
/usr/bin/cat -s `/usr/bin/ls -tr1 /var/adm/messages.? 2>/dev/null` \
	/var/adm/messages | /usr/bin/tail -200
