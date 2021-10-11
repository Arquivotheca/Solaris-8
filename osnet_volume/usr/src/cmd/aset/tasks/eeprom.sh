#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)eeprom.sh	1.3	94/11/05 SMI"

exittask()
{
   exit
}

bad_value()
{
   setting=$1
   case $setting in
      none | command | full)
	 # not a bad value
         return 1;;
      *)
	 # is a bad value
	 return 0;;
   esac
}

echo
echo "*** Begin EEPROM Check ***"

eeprom=/usr/sbin/eeprom

if [ ! -x $eeprom ]
then
   exit
fi

secureline=`$eeprom -i secure`
setting=`echo $secureline | $AWK -F= '{print $2}'`
if bad_value $setting
then
   secureline=`$eeprom -i security-mode`
   setting=`echo $secureline | $AWK -F= '{print $2}'`
   if bad_value $setting
   then
      echo
      echo "Security option not found on eeprom. Task skipped."
      exittask
   fi
fi

echo
echo EEPROM security option currently set to \"$setting\".

if [ "$ASETSECLEVEL" = "med" ]
then
   if [ "$setting" != "command" ]
   then
      echo
      echo Recommend setting to \"command\".
   fi
   exittask
fi

if [ "$ASETSECLEVEL" = "high" ]
then
   if [ "$setting" != "full" ]
   then
      echo
      echo Recommend setting to \"full\".
   fi
   exittask
fi

echo
echo "*** End EEPROM Check ***"
