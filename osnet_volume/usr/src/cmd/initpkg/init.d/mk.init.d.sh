#	Copyright (c) 1988 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)mk.init.d.sh	1.5	92/07/14 SMI"	SVr4.0 1.2
#	From:	SVr4.0	initpkg:init.d/:mk.init.d.sh	1.2

INSDIR=${ROOT}/etc/init.d
if u3b2 || sun
then
	if [ ! -d ${INSDIR} ] 
	then 
		mkdir ${INSDIR} 
		if [ $? != 0 ]
		then
			exit 1
		fi
		eval ${CH}chmod 755 ${INSDIR}
		eval ${CH}chgrp sys ${INSDIR}
		eval ${CH}chown root ${INSDIR}
	fi 
	for f in [a-zA-Z0-9]* 
	do
		if [ "$f" != "SCCS" ]
		then
			${INS} -f ${INSDIR} -m 744 -g sys -u root $f
		fi
	done
fi
