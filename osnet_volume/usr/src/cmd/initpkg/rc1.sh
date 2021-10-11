#!/sbin/sh
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T.
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)rc1.sh	1.13	98/09/10 SMI"

# "Run Commands" executed when the system is changing to init state 1

PATH=/usr/sbin:/usr/bin

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

#
#	umount_fsys	umountall-args
#
#	Calls umountall with the specified arguments and reports progress
#	as file systems are unmounted if umountall -k is invoked.
#
umount_fsys ()
{
	/sbin/umountall "$@" 2>&1 | while read fs; do \
		shift $#; set -- $fs
		if [ "x$1" = xumount: ]; then
			echo "$*"	# Most likely an error message
		else
			echo "$1 \c" | /usr/bin/tr -d :
		fi
	done
	echo "done."
}

if [ $_INIT_PREV_LEVEL = S ]; then
	echo 'The system is coming up for administration.  Please wait.'

elif [ $_INIT_RUN_LEVEL = 1 ]; then
	echo 'Changing to state 1.'
	>/etc/nologin

        echo "Unmounting remote filesystems: \c"
	umount_fsys -r -k -s

	if [ -d /etc/rc1.d ]; then
		for f in /etc/rc1.d/K*; do
			if [ -s $f ]; then
				case $f in
					*.sh)	.	 $f ;;
					*)	/sbin/sh $f stop ;;
				esac
			fi
		done
	fi

	echo "Killing user processes: \c"
	#
	# Look for ttymon, in.telnetd, in.rlogind and processes
	# in their process groups so they can be terminated.
	#
	/usr/sbin/killall
	/usr/sbin/killall 9
	/usr/bin/pkill -TERM -v -u 0,1; sleep 5
	/usr/bin/pkill -KILL -v -u 0,1
	echo "done."

fi

if [ -d /etc/rc1.d ]; then
	for f in /etc/rc1.d/S*; do
		if [ -s $f ]; then
			case $f in
				*.sh)	.	 $f ;;
				*)	/sbin/sh $f start ;;
			esac
		fi
	done
fi

if [ $_INIT_RUN_LEVEL = 1 ]; then
	if [ $_INIT_PREV_LEVEL = S ]; then
		echo 'The system is ready for administration.'
	else
		echo 'Change to state 1 has been completed.'
	fi
fi

# sulogin and its children need a controlling tty
# to make exiting graceful.

exec <> /dev/console 2<> /dev/console
trap "" 15

# Allow the administrator to log in as root on the console.  If sulogin
# is aborted with ctrl-D, or if the administrator exits the root shell,
# then return to the default run-level.

/sbin/sulogin
deflevel=`/usr/bin/awk -F: '$3=="initdefault"{print $2}' /etc/inittab`
/sbin/init ${deflevel:-s}
