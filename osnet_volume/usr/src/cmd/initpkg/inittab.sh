#!/sbin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved
#
#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.
#
#	Copyright (c) 1997-1998 by Sun Microsystems, Inc.
#	All rights reserved.	
#
#ident	"@(#)inittab.sh	1.33	98/12/18 SMI"	SVr4.0 1.18.6.1

case "$MACH" in
sparc)
	TTY_TYPE=sun
	;;
i386)
	TTY_TYPE=sun-color
	;;

*)
	echo "$0: Error: Unknown architecture \"$MACH\"" >& 2
	exit 1
	;;
esac

echo "\
ap::sysinit:/sbin/autopush -f /etc/iu.ap
ap::sysinit:/sbin/soconfig -f /etc/sock2path
fs::sysinit:/sbin/rcS sysinit		>/dev/msglog 2<>/dev/msglog </dev/console
is:3:initdefault:
p3:s1234:powerfail:/usr/sbin/shutdown -y -i5 -g0 >/dev/msglog 2<>/dev/msglog
sS:s:wait:/sbin/rcS			>/dev/msglog 2<>/dev/msglog </dev/console
s0:0:wait:/sbin/rc0			>/dev/msglog 2<>/dev/msglog </dev/console
s1:1:respawn:/sbin/rc1			>/dev/msglog 2<>/dev/msglog </dev/console
s2:23:wait:/sbin/rc2			>/dev/msglog 2<>/dev/msglog </dev/console
s3:3:wait:/sbin/rc3			>/dev/msglog 2<>/dev/msglog </dev/console
s5:5:wait:/sbin/rc5			>/dev/msglog 2<>/dev/msglog </dev/console
s6:6:wait:/sbin/rc6			>/dev/msglog 2<>/dev/msglog </dev/console
fw:0:wait:/sbin/uadmin 2 0		>/dev/msglog 2<>/dev/msglog </dev/console
of:5:wait:/sbin/uadmin 2 6		>/dev/msglog 2<>/dev/msglog </dev/console
rb:6:wait:/sbin/uadmin 2 1		>/dev/msglog 2<>/dev/msglog </dev/console
sc:234:respawn:/usr/lib/saf/sac -t 300
co:234:respawn:/usr/lib/saf/ttymon -g -h -p \"\`uname -n\` console login: \" -T $TTY_TYPE -d /dev/console -l console -m ldterm,ttcompat" >inittab

exit 0
