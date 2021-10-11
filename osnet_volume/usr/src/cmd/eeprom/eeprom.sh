#!/usr/bin/sh
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved
#
#ident	"@(#)eeprom.sh	1.3	95/08/29 SMI"
#
# Execute the platform dependent eeprom(1M) program if it exists.
#
_PLATFORM=`/usr/bin/uname -i 2> /dev/null`
if [ $? -ne 0 ]
then
	_RELEASE=`/usr/bin/uname -sr 2> /dev/null`
	echo "$0 (script) not supported on $_RELEASE"
	exit 255
fi
if [ -x /usr/platform/$_PLATFORM/sbin/eeprom ]
then
	exec /usr/platform/$_PLATFORM/sbin/eeprom "$@"
else
	echo "eeprom(1M) not implemented on $_PLATFORM"
	exit 255
fi
