#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)ckpacct.sh	1.5	97/02/26 SMI"	/* SVr4.0 1.9	*/
#       periodically check the size of /var/adm/pacct
#       if over $1 blocks (default: maxi 200, mini 500), execute 
#	turnacct switch

_adm=/var/adm
PATH=/usr/lib/acct:/usr/bin:/usr/sbin
trap "rm -f /var/adm/cklock*; exit 0" 0 1 2 3 9 15
export PATH

if [ -f uts -a uts ]
then
	_max=${1-200}
else
	_max=${1-500}
fi
_MIN_BLKS=500
cd /var/adm

#	set up lock files to prevent simultaneous checking

cp /dev/null cklock
chmod 400 cklock
ln cklock cklock1
if test $? -ne 0 ; then exit 1; fi

#	If there are less than $_MIN_BLKS free blocks left on the /var/adm
#	file system, turn off the accounting (unless things improve
#	the accounting wouldn't run anyway).  If something has
#	returned the file system space, restart accounting.  This
#	feature relies on the fact that ckpacct is kicked off by the
#	cron at least once per hour.


_blocks=`df $_adm | sed 's/.*://' | awk '{ print $1 }'`

if [ "$_blocks" -lt $_MIN_BLKS   -a  -f /tmp/acctoff ];then
	echo "ckpacct: $_adm still low on space ($_blocks blks); \c"
	echo "acctg still off"
	( echo "ckpacct: $_adm still low on space ($_blocks blks); \c"
	echo "acctg still off" ) | mailx root adm
	exit 1
elif [ "$_blocks" -lt $_MIN_BLKS ];then
	echo "ckpacct: $_adm too low on space ($_blocks blks); \c"
	echo "turning acctg off"
	( echo "ckpacct: $_adm too low on space ($_blocks blks); \c"
	echo "turning acctg off" ) | mailx root adm
	nulladm /tmp/acctoff
	turnacct off
	exit 1
elif [ -f /tmp/acctoff ];then
	echo "ckpacct: $_adm free space restored; turning acctg on"
	echo "ckpacct: $_adm free space restored; turning acctg on" | \
		mailx root adm
	rm /tmp/acctoff
	turnacct on
fi

_cursize="`du -s pacct | sed 's/	.*//'`"
if [ "${_max}" -lt "${_cursize}" ]; then
	turnacct switch
fi
