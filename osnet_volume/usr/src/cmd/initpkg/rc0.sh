#!/sbin/sh
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T.
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
# Copyright (c) 1997-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)rc0.sh	1.21	99/04/09 SMI"

# "Run Commands" for init states 0, 5 and 6.

PATH=/usr/sbin:/usr/bin

echo 'The system is coming down.  Please wait.'

# Make sure /usr is mounted before proceeding since init scripts
# and this shell depend on things on /usr file system

/sbin/mount /usr >/dev/null 2>&1

# Export boot parameters to rc scripts

set -- `/usr/bin/who -r`

_INIT_RUN_LEVEL="$7"	# Current run-level
_INIT_RUN_NPREV="$8"	# Number of times previously at current run-level
_INIT_PREV_LEVEL="$9"	# Previous run-level

set -- `/usr/bin/uname -a`

_INIT_UTS_SYSNAME="$1"  # Operating system name (uname -s)
_INIT_UTS_NODENAME="$2" # Node name (uname -n)
_INIT_UTS_RELEASE="$3"  # Operating system release (uname -r)
_INIT_UTS_VERSION="$4"  # Operating system version (uname -v)
_INIT_UTS_MACHINE="$5"  # Machine class (uname -m)
_INIT_UTS_ISA="$6"      # Instruction set architecture (uname -p)
_INIT_UTS_PLATFORM="$7" # Platform string (uname -i)

export _INIT_RUN_LEVEL _INIT_RUN_NPREV _INIT_PREV_LEVEL \
    _INIT_UTS_SYSNAME _INIT_UTS_NODENAME _INIT_UTS_RELEASE _INIT_UTS_VERSION \
    _INIT_UTS_MACHINE _INIT_UTS_ISA _INIT_UTS_PLATFORM

# The following segment is for historical purposes.
# There should be nothing in /etc/shutdown.d.

if [ -d /etc/shutdown.d ]; then
	for f in /etc/shutdown.d/*; do
		[ -s $f ] && /sbin/sh $f
	done
fi

# End of historical section

if [ -d /etc/rc0.d ]; then
	for f in /etc/rc0.d/K*; do
		if [ -s $f ]; then
			case $f in
				*.sh)	.	 $f ;;
				*)	/sbin/sh $f stop ;;
			esac
		fi
	done

	# System cleanup functions ONLY (things that end fast!)	

	for f in /etc/rc0.d/S*; do
		if [ -s $f ]; then
			case $f in
				*.sh)	.	 $f ;;
				*)	/sbin/sh $f start ;;
			esac
		fi
	done
fi

[ -f /etc/.dynamic_routing ] && /usr/bin/rm -f /etc/.dynamic_routing

trap "" 15

# Kill all processes, first gently, then with prejudice.

/usr/sbin/killall
/usr/bin/sleep 5
/usr/sbin/killall 9
/usr/bin/sleep 10
[ -x /usr/lib/acct/closewtmp ] && /usr/lib/acct/closewtmp
/sbin/sync; /sbin/sync; /sbin/sync

# Unmount file systems. /usr, /var, /var/adm, /var/run are not unmounted by
# umountall because they are mounted by rcS (for single user mode) rather than
# mountall. If this is changed, mountall, umountall and rcS should also change.

/sbin/umountall
/sbin/umount /var/adm >/dev/null 2>&1
/sbin/umount /var/run >/dev/null 2>&1
/sbin/umount /var >/dev/null 2>&1
/sbin/umount /usr >/dev/null 2>&1

echo 'The system is down.'
