#!/sbin/sh
#
#	Copyright (c) 1996-1999 by Sun Microsystems, Inc.
#	  All rights reserved.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All rights reserved.
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#ident	"@(#)mk.rcS.d.sh	1.23	99/07/07 SMI"

sparc_STARTLST=""
i386_STARTLST="45initboot 15initpcihpc"
MACH_STARTLST=`eval echo "\\$${MACH}_STARTLST"`

COMMON_STARTLST="\
10initpcmcia \
30network.sh \
30rootusr.sh \
33keymap.sh \
35cacheos.sh \
40standardmounts.sh \
41cachefs.root \
42coreadm \
50devfsadm \
70buildmnttab.sh"

STOPLST="\
28nfs.server \
33audit \
35volmgt \
36utmpd \
40cron \
40nscd \
40syslog \
41autofs \
41ldap.client \
41rpc"

INSDIR=${ROOT}/etc/rcS.d

if [ ! -d ${INSDIR} ]
then
	mkdir ${INSDIR}
	eval ${CH}chmod 755 ${INSDIR}
	eval ${CH}chgrp sys ${INSDIR}
	eval ${CH}chown root ${INSDIR}
fi
for f in ${STOPLST}
do
	name=`echo $f | sed -e 's/^..//' | sed -e 's/\.sh$//`
	rm -f ${INSDIR}/K$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/K$f
done
for f in ${COMMON_STARTLST} ${MACH_STARTLST}
do
	name=`echo $f | sed -e 's/^..//' | sed -e 's/\.sh$//`
	rm -f ${INSDIR}/S$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/S$f
done
