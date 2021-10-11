#!/bin/sh
#
# Copyright (c) 1993, 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident	"@(#)sxcmem.sh	1.6	95/01/04 SMI"

# Startup script for reserving physically contiguous memory for the SX
# accelerator on the SPARCstation-10,SX and SPARCstation-20.

# Note that since these platforms are part of the standard sun4m platform
# group, the conf file will be found on all sun4m platforms.

case "$1" in
'start')
	if [ -f /platform/`/sbin/uname -i`/kernel/drv/sx_cmem.conf ]
	then
		(< /dev/sx_cmem) > /dev/null 2>&1
	fi
	;;
'stop')
	;;
*)
	echo "Usage: $0 start"
	;;
esac

exit 0
