#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#
# Copyright (c) 1994, 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)mk.rc2.d.sh	1.34	99/07/07 SMI"
#

STARTLST=" 01MOUNTFSYS 05RMTMPFILES \
20sysetup 70uucp 71ldap.client 71rpc 73cachefs.daemon 73nfs.client 74autofs \
74syslog 76nscd 75cron 75savecore 82mkdtab 88utmpd 92volmgt \
93cacheos.finish 99audit"

STOPLST="28nfs.server"

INSDIR=${ROOT}/etc/rc2.d

if [ ! -d ${INSDIR} ] 
then 
	mkdir ${INSDIR} 
	eval ${CH}chmod 755 ${INSDIR}
	eval ${CH}chgrp sys ${INSDIR}
	eval ${CH}chown root ${INSDIR}
fi 
for f in ${STOPLST}
do 
	name=`echo $f | sed -e 's/^..//'`
	rm -f ${INSDIR}/K$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/K$f
done
for f in ${STARTLST}
do 
	name=`echo $f | sed -e 's/^..//'`
	rm -f ${INSDIR}/S$f
	ln ${ROOT}/etc/init.d/${name} ${INSDIR}/S$f
done
