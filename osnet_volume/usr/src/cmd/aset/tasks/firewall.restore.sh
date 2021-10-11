#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)firewall.restore.sh	1.4	94/12/13 SMI"
#
# This reverses the effect of firewall.task, which is assumed to have been
# run at least once.

CAT=/bin/cat
MV=/bin/mv
ADB=/bin/adb
ROUTED="/usr/sbin/in.routed"
RC2INET="/etc/rc2.d/S69inet"

arch=${ASETDIR}/archives/ipforwarding.arch
myname=`expr $0 : ".*/\(.*\)" \| $0`

fail()
{
   echo
   echo "$myname failed:"
   echo $*
   exit 1
}

echo
echo "Beginning $myname..."

if [ "$ASETDIR" = "" ]
then
   fail "ASETDIR variable undefined."
fi

if [ $UID -ne 0 ]
then
   fail "Permission denied."
fi

if [ ! -s $arch ]
then
   fail "$arch not found."
fi

if [ ! -s ${ROUTED}.asetoriginal ]
then
   fail "${ROUTED}.asetoriginal not found."
fi

if [ ! -s ${RC2INET}.asetoriginal ]
then
   fail "${RC2INET}.asetoriginal not found."
fi

oldvalue=`$CAT ${ASETDIR}/archives/ipforwarding.arch`
case $oldvalue in
   -1 | 1 | 0)
      ;;
   *)
      fail "Invalid value in $arch.";;
esac

/usr/sbin/ndd -set /dev/ip ip_forwarding $oldvalue
# ndd bug# 1185290 - ndd always indicates failure when setting a network entry
#if [ $? -ne 0 ]
#then
#   fail "Could not restore original state of IP packet forwarding"
#fi

$MV ${RC2INET}.asetoriginal $RC2INET
if [ $? -ne 0 ]
then
   fail "Could not restore ${RC2INET}."
fi

num=`find /etc/rc* -exec grep -l ndd {} \;|sed '/S69inet$/d'|wc -c`
if [ $num != 0 ]
then 
   echo 
   echo "The following files in /etc/rc* have embedded \`ndd' commands"
   find /etc/rc* -exec grep -l ndd {} \;|sed '/S69inet$/d'
fi

echo
echo "Restored ip_forwarding to previous value - $oldvalue."

$MV ${ROUTED}.asetoriginal $ROUTED
if [ $? -ne 0 ]
then
   fail "Could not restore ${ROUTED}."
fi

echo
echo "Restored ${ROUTED}."

echo
echo "$myname completed."
