#! /usr/bin/sh
#
# ident	"@(#)printmgr.sh	1.3	99/09/14 SMI"
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
PMDIR="/usr/sadm/admin/printmgr"
CLSDIR="${PMDIR}/classes"
L10NDIR="/usr/share/lib/locale"

CLASSPATH="${L10NDIR}:${CLSDIR}/pmclient.jar:${CLSDIR}/pmserver.jar"
LD_LIBRARY_PATH="${PMDIR}/lib"

export CLASSPATH LD_LIBRARY_PATH

exec /usr/java1.2/bin/java com.sun.admin.pm.client.pmTop $1
