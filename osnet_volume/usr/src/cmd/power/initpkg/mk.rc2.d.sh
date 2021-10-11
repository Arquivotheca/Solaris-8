#!/bin/sh
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)mk.rc2.d.sh	1.5	97/12/08 SMI"

RCLINKS="\
$ROOT/etc/rc0.d/K37power \
$ROOT/etc/rcS.d/K37power \
$ROOT/etc/rc1.d/K37power \
$ROOT/etc/rc2.d/S85power"

for LINK in $RCLINKS; do
	RCDIR=`dirname $LINK`

	if [ ! -d $RCDIR ]; then
		echo mkdir -m 0755 $RCDIR
		mkdir -m 0755 $RCDIR || exit 1
		echo chown root:sys $RCDIR
		chown root:sys $RCDIR || exit 1
	fi

	echo ln $ROOT/etc/init.d/power $LINK
	rm -f $LINK
	ln $ROOT/etc/init.d/power $LINK || exit 1
done

exit 0
