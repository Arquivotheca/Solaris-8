#!/bin/sh
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)flash-update.sh	1.4	98/06/22 SMI"

PATH=/sbin:/usr/sbin:/usr/bin; export PATH

# designed to be run at boot time from an /etc/init.d script.
# exits silently, if conditions don't warrant a flash prom upgrade.

DONT_ASK=/etc/.FLASH-UPDATE
V9DIR=/kernel/drv/sparcv9
PROM="/usr/platform/sun4u/lib/prom/`uname -i`"

[ -f $DONT_ASK -o ! -f $PROM -o ! -d $V9DIR ] && exit 0

PKG=SUNWcarx
pkginfo -q $PKG > /dev/null 2>&1 || exit 0

prtconf -x > /dev/null 2>&1
status=$?
[ $status -le 0 -o $status -eq 255 ] && exit 0

text=SUNW_UXFL_WRAPPER

upgrade_msg=`gettext $text '
This system has older firmware.  Although the current firmware is fully
capable of running the 32-bit packages, you will not be able to run the 64-bit
packages installed on this system until you update the system flash PROM.
'`

desktop_msg=`gettext $text '
This system ships with flash PROM write-protect jumpers in the
"write disabled" position.  Before running the flash PROM update, please
verify that the flash PROM jumpers are in the "write-enabled" position.
'`

server_msg=`gettext $text '
This system ships with flash PROM write-protect jumpers in the
"write enabled" position.  Unless the jumpers on this system have been
changed, there is no need to change them in order to run the system flash
PROM update.

The front panel keyswitch on this system must NOT be in the "SECURE"
position while the system flash PROM update is running.  Please check
the keyswitch position before answering the next question.
'`

moreinfo_msg=`gettext $text '
See the Hardware Platform Guide for more information.
'`

case `uname -i` in
SUNW,Ultra-1 | SUNW,Ultra-2)
	echo "$upgrade_msg"
	echo "$desktop_msg"
	echo "$moreinfo_msg"
	;;
SUNW,Ultra-4 | SUNW,Ultra-Enterprise)
	echo "$upgrade_msg"
	echo "$server_msg"
	echo "$moreinfo_msg"
	;;
*)
	exit 0 ;;
esac

answer=
timeout=
yesstr="yes"; YESSTR="YES"; yes="y"; YES="Y"
nostr="no"; NOSTR="NO"; no="n"; NO="N"
yn='[y,n]'

timeout_prompt=`gettext $text '
Please answer the next question within 90 seconds,
or press the ENTER key to disable the timer.
'`

timer_disabled=`gettext $text '
The timer has been disabled.
'`

timeout_msg=`gettext $text '
read timed out ... the system flash PROM has not been changed.
'`
logmsg1="read confirming the flash PROM update timed out."
logmsg2="The system flash PROM has not been changed"
logmsg3="run /etc/init.d/flashprom to update the system flash PROM."
pri=daemon.notice
tag=flashprom

log_timeout()
{
	echo "$timeout_msg"
	logger -p $pri -t $tag "$logmsg1"
	logger -p $pri -t $tag "$logmsg2"
	logger -p $pri -t $tag "$logmsg3"
}

format_y_or_n=`gettext $text "%s or %s? %s"`
yes_or_no=`printf "$format_y_or_n" $yesstr $nostr $yn`
format_error_yn=`gettext $text "\nPlease answer \"%s\" or \"%s\""`
error_yn=`printf "$format_error_yn" $yesstr $nostr`

#
# usage: question prompt default timeout?
#
# returns a string ("yes" or "no") in $answer
# exits if the (one time) optional timeout occurs.
#
question()
{
    answer=$2
    timeout=$3
    while [ true ]
    do
	if [ $timeout -ne 0 ]; then
	    trap 'log_timeout ; exit 0' 17
	    echo "$timeout_prompt"
	    echo "${1}${yes_or_no} \c"
	    (trap "exit 0" 16; sleep 90; kill -17 $$) &
	    read ans
	    kill -16 $!
	else
	    echo "${1}${yes_or_no} \c"
	    read ans
	fi
	case "$ans" in
	$nostr | $NOSTR | $no | $NO)
		answer=no; return 0;;
	$yesstr | $YESSTR | $yes | $YES)
		answer=yes; return 0;;
	esac
	if [ $timeout -ne 0 ]; then
		echo "$timer_disabled"
	else
		echo "$error_yn"
	fi
	timeout=0
    done
}

trap "" 1 2 3 15

prompt=`gettext $text '
\tWould you like to run the system flash PROM update now?
\t(By default the system flash PROM update will not be run now.)
\t'`

question "$prompt" no 1
if [ "$answer" = "yes" ]; then
	/bin/sh $PROM
	exit 0
fi

prompt_text="\n\
\tWould you like to be asked this question on subsequent reboots?\n\
\t(By default the question will be asked on each reboot until the\n\
\tsystem flash PROM is updated.)\n\t"

prompt=`gettext $text "$prompt_text"`

question "$prompt" yes 0
[ "$answer" = "no" ] && touch $DONT_ASK

exit 0
