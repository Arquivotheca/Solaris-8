#!/sbin/sh
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T.
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)acct.sh	1.3	97/12/08 SMI"

state="$1"

if [ -z "$_INIT_RUN_NPREV" -o -z "$_INIT_PREV_LEVEL" ]; then
	set -- `/usr/bin/who -r`
	_INIT_RUN_NPREV="$8"
	_INIT_PREV_LEVEL="$9"
fi

[ $_INIT_RUN_NPREV != 0 ] && exit 0

case "$state" in
'start')
	[ $_INIT_PREV_LEVEL = 2 -o $_INIT_PREV_LEVEL = 3 ] && exit 0
	echo 'Starting process accounting'
	/usr/lib/acct/startup
	;;

'stop')
	echo 'Stopping process accounting'
	/usr/lib/acct/shutacct
	;;

*)
	echo "Usage: $0 { start | stop }"
	exit 1
	;;
esac
exit 0
