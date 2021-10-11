#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)sa2.sh	1.4	98/03/26 SMI"       /* SVr4.0 1.4 */
#	sa2.sh 1.4 of 5/8/89
DATE=`/usr/bin/date +%d`
RPT=/var/adm/sa/sar$DATE
DFILE=/var/adm/sa/sa$DATE
ENDIR=/usr/bin
cd $ENDIR
$ENDIR/sar $* -f $DFILE > $RPT
/usr/bin/find /var/adm/sa \( -name 'sar*' -o -name 'sa*' \) -mtime +7 -exec /usr/bin/rm {} \;
