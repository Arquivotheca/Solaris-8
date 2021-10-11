#! /bin/sh
#
# @(#)st_clean.sh 1.5 97/10/02 SMI; SunOS BSM
#
#  This a clean script for all tape drives
# 

PROG=`basename $0`
PATH="/usr/sbin:/usr/bin"
TEXTDOMAIN="SUNW_OST_OSCMD"
export TEXTDOMAIN

#	/*
#	 * TRANSLATION_NOTE:
#	 * If you make changes to messages of st_clean command,
#	 * don't forget to make corresponding changes in dev_clean.po file.
#	 */

USAGE=`gettext "%s [-s|-f|-r] device info_label"`

#
# 		*** Shell Function Declarations ***
#


con_msg() {
    form=`gettext "%s: Media in %s is ready.  Please, label and store safely."`
    printf "${form}\n" $PROG $DEVICE > /dev/console
}

e_con_msg() {
    form=`gettext "%s: Error cleaning up device %s."`
    printf "${form}\n" $PROG $DEVICE > /dev/console
}

user_msg() {
    form=`gettext "%s: Media in %s is ready.  Please, label and store safely."`
    printf "${form}\n" $PROG $DEVICE > /dev/tty
}

e_user_msg() {
    form=`gettext "%s: Error cleaning up device %s."`
    printf "${form}" $PROG $DEVICE > /dev/tty
    gettext "Please inform system administrator.\n" > /dev/tty
}

mk_error() {
   chown bin /etc/security/dev/$1
   chmod 0100 /etc/security/dev/$1
}

while getopts ifs c
do
   case $c in
   i)   FLAG=$c;;
   f)   FLAG=$c;;
   s)   FLAG=$c;;
   \?)   printf "${USAGE}\n" $PROG >/dev/tty
      exit 1 ;;
   esac
done
shift `expr $OPTIND - 1`

# get the map information

TAPE=$1
MAP=`dminfo -v -n $TAPE`
DEVICE=`echo $MAP | cut -f1 -d:`
TYPE=`echo $MAP | cut -f2 -d:`
FILES=`echo $MAP | cut -f3 -d:`
DEVFILE=`echo $FILES | cut -f1 -d" "`

#if init then do once and exit

if [ "$FLAG" = "i" ] ; then
   x="`mt -f $DEVFILE rewoffl 2>&1`"
   z="$?"   

   case $z in
   0)

   # if this is a open reel tape than we a sucessful
   # else must be a cartrige tape we failed

      if mt -f $DEVFILE status 2>&1 | grep "no tape loaded" >/dev/null ; then  
         con_msg
         exit 0
      else 
         e_con_msg
         mk_error $DEVICE
         exit 1
      fi;;
   1) 
   
   # only one error mesage is satisfactory

      if echo $x | grep "no tape loaded" >/dev/null ; then
         con_msg
         exit 0
      else
         e_con_msg
         mk_error $DEVICE
         exit 1
      fi;;

   2) 

   # clean up failed exit with error

      e_con_msg
      mk_error $DEVICE
      exit 1;;

   esac
else
# interactive clean up
   x="`mt -f $DEVFILE rewoffl 2>&1`"
   z="$?"

   case $z in
   0)

   # if this is a open reel tape than we a sucessful
   # else must be a cartrige tape we must retry until user removes tape

      if mt -f $DEVFILE status 2>&1 | grep "no tape loaded"  > /dev/null ; then
         user_msg
         exit 0
      else
         while true
         do
            if mt -f $DEVFILE status 2>&1 | grep "no tape loaded" > /dev/null ; then
                user_msg
                exit 0
            else
		form=`gettext "Please remove the tape from the %s."`
                printf "${form}\n" $DEVICE  >/dev/tty
                /usr/5bin/echo \\007 >/dev/tty
                sleep 3
            fi
         done
      fi;;
   1)

   # only one error mesage is satisfactory

      if echo $x | grep "no tape loaded" > /dev/null ; then
         user_msg
         exit 0
      else
         e_user_msg
         mk_error $DEVICE
         exit 1
      fi;;

   2)

   # clean up failed exit with error

      e_user_msg
      mk_error $DEVICE
      exit 1;;

   esac
fi
exit 2
