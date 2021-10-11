#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Copyright (c) 1999 by Sun Microsystems, Inc.
#	All rights reserved.

#ident	"@(#)calendar.sh	1.11	99/05/21 SMI"	/* SVr4.0 1.8	*/
#	calendar.sh - calendar command, uses /usr/lib/calprog

PATH=/usr/bin
USAGE="usage: calendar [ - ]"

# mktmpdir - Create a private (mode 0700) temporary directory inside of /tmp
mktmpdir() {
	tmpdir=/tmp/calendar.$$
	/usr/bin/mkdir -m 700 $tmpdir || exit 1
}
mktmpdir
_tmp=$tmpdir/cal$$

cleanup()
{
	/usr/bin/rm -rf $tmpdir
}

# Trap on SIGHUP, SIGINT, SIGQUIT, SIGPIPE, SIGTERM
for i in 1 2 3 13 15
do
	# Ignore trap if already set by the shell.  NOTE: If /bin/sh
	# is made XCU4 compliant, updates will also be required to this
	# if statement because of XCU4 changes to the trap built-in.
	TRAP_IGNORE=`trap | egrep -c "^$i:\$"`
	if [ "$TRAP_IGNORE" != "1" ]
	then
		# Cleanup; reset default value; send signal to process.
		trap "cleanup; trap $i; kill -$i $$" $i
	fi
done

# POSIX.2 and XCU4 specify that if a utility accepts an operand,
# it also handle -- as a delimitor.
if [ "$1" = -- ]; then
	shift
fi

case $# in
0)	if [ -f calendar ]; then
		/usr/lib/calprog > ${_tmp}
		egrep -f ${_tmp} calendar
	else
		echo >&2 $0: `pwd`/calendar not found
		exit 1
	fi;;
*)	case $* in
	-)	if (rpcinfo -p | fgrep -s ypbind); then
			caldata="ypcat passwd.byname | grep /`uname -n`/"
		else
			caldata="cat /dev/null"
		fi
		/usr/lib/calprog > ${_tmp}
		eval $caldata | cat /etc/passwd - | \
		sed 's/\([^:]*\):.*:\(.*\):[^:]*$/_dir=\2 _user=\1/' | \
		while read _token; do
			eval ${_token}	# evaluates _dir= and _user=
			if [ -s ${_dir}/calendar ]; then
				egrep -f ${_tmp} ${_dir}/calendar 2>/dev/null \
					> $tmpdir/calendar.$$
				if [ -s $tmpdir/calendar.$$ ]; then
					mail ${_user} < $tmpdir/calendar.$$
				fi
			fi
		done;;
	*)	echo >&2 $0: illegal option -- $@
		echo >&2 $USAGE
		exit 1
	esac
esac
cleanup
exit 0
