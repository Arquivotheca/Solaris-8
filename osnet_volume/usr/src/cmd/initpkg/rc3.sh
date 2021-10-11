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
#ident	"@(#)rc3.sh	1.15	99/02/23 SMI"

# Run Commands executed when the system is changing to init state 3,
# same as state 2 (multi-user) but with remote file sharing.

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

# Export net boot configuration strategy. _INIT_NET_IF is set to the
# interface name of the netbooted interface if this is a net boot.
# _INIT_NET_STRATEGY is set to the network configuration strategy.
set -- `/sbin/netstrategy`
if [ $? -eq 0 ]; then
	if [ "$1" = "nfs" -o "$1" = "cachefs" ]; then
		_INIT_NET_IF="$2"
	fi
	_INIT_NET_STRATEGY="$3"
	export _INIT_NET_IF _INIT_NET_STRATEGY 
fi

[ $_INIT_PREV_LEVEL = 2 ] && echo 'Changing to state 3.'

if [ -d /etc/rc3.d ]; then
	for f in /etc/rc3.d/K*; do
		if [ -s $f ]; then
			case $f in
				*.sh)	.	 $f ;;
				*)	/sbin/sh $f stop ;;
			esac
		fi
	done

	for f in /etc/rc3.d/S*; do
		if [ -s $f ]; then
			case $f in
				*.sh)	.	 $f ;;
				*)	/sbin/sh $f start ;;
			esac
		fi
	done
fi

# Unload all the loadable modules brought in during boot

modunload -i 0 & >/dev/null 2>&1

if [ $_INIT_PREV_LEVEL = S -o $_INIT_PREV_LEVEL = 1 ]; then
	echo 'The system is ready.'
else
	echo 'Change to state 3 has been completed.'
fi
