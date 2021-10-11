#!/bin/sh
#
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)usrgrp.restore.sh	1.2	92/07/14 SMI"

# This script restores password and group files changed by usrgrp.task back
# to what they used to be according to the archive file -

passwd_arch=${ASETDIR}/archives/passwd.arch.$ASETSECLEVEL
group_arch=${ASETDIR}/archives/group.arch.$ASETSECLEVEL
shadow_arch=${ASETDIR}/archives/shadow.arch.$ASETSECLEVEL
CP=/bin/cp

myname=`expr $0 : ".*/\(.*\)" \| $0`

fail()
{
   echo
   echo "$myname failed:"
   echo $*
   exit 1
}

doit()
# usage: doit command_string
# "command_string" is expected to succeed.
{
   $*
   status=$?
   if [ $status -ne 0 ]
   then
      echo;echo "Operation failed: $*"
   fi
   return $status
}

echo
echo "Beginning $myname..."

if [ "${ASETDIR}" = "" ]
then
   fail "ASETDIR variable undefined."
fi

if [ $UID -ne 0 ]
then
   fail "Permission Denied."
fi

doit $CP /etc/passwd /etc/passwd.asetbak
if [ $? = 0 ]
then
   echo;echo "Restoring /etc/passwd. Saved existing file in /etc/passwd.asetbak."
fi

doit $CP /etc/group /etc/group.asetbak
if [ $? = 0 ]
then
   echo;echo "Restoring /etc/group. Saved existing file in /etc/group.asetbak."
fi

doit $CP /etc/shadow /etc/shadow.asetback
if [ $? = 0 ]
then
   echo; echo "Restoring /etc/shadow. Saved existing file in /etc/shadow.asetback."
fi

doit $CP $passwd_arch /etc/passwd
doit $CP $group_arch /etc/group
doit $CP $shadow_arch /etc/shadow

echo
echo "$myname completed."
