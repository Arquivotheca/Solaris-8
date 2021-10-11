#!/bin/sh
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)mk.rc2.d.sh	1.2	98/06/16 SMI"

RCLINKS="\
$ROOT/etc/rc2.d/S75flashprom"

OLD_RCLINKS="\
$ROOT/etc/rc2.d/S71flashprom"

echo rm -f $OLD_RCLINKS
rm -f $OLD_RCLINKS

for LINK in $RCLINKS; do
	RCDIR=`dirname $LINK`

	if [ ! -d $RCDIR ]; then
		echo mkdir -m 0755 $RCDIR
		mkdir -m 0755 $RCDIR || exit 1
		echo chown root:sys $RCDIR
		chown root:sys $RCDIR || exit 1
	fi

	echo ln $ROOT/etc/init.d/flashprom $LINK
	rm -f $LINK
	ln $ROOT/etc/init.d/flashprom $LINK || exit 1
done

exit 0
