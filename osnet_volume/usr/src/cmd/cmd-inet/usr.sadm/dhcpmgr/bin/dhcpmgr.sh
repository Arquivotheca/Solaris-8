#!/bin/sh
#
#ident	"@(#)dhcpmgr.sh	1.4	99/05/07 SMI"
#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
DMDIR=/usr/sadm/admin/dhcpmgr
L10NDIR=/usr/share/lib/locale

CLASSPATH=${L10NDIR}:${DMDIR}/dhcpmgr.jar
export CLASSPATH

# add /usr/dt/bin so sdtwebclient will be available for help
PATH=${PATH}:/usr/dt/bin
export PATH

exec /usr/java1.2/bin/java com.sun.dhcpmgr.client.DhcpmgrApplet
