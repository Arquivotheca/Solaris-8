#! /sbin/sh
#
# ident	"@(#)cvc.sh	1.15	98/05/05 SMI"
#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# Startup script for Network Console
#

case "$1" in
'start')
	starfire="SUNW,Ultra-Enterprise-10000"
	if [ ${_INIT_UTS_PLATFORM:-`/sbin/uname -i`} = "${starfire}" -a \
	    -x /platform/${starfire}/lib/cvcd ]; then
		/platform/${starfire}/lib/cvcd
	fi
	;;

'stop')
	/usr/bin/pkill -9 -x -u 0 cvcd
	;;

*)
	echo "Usage: $0 { start | stop }"
	exit 1
	;;
esac
exit 0
