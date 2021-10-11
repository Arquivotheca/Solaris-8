#! /bin/sh
#
# @(#)fd_clean.sh 1.5 97/10/02 SMI; SunOS BSM
#
#  This a clean script for the floppy drive
# 

PROG=`basename $0`
PATH="/usr/sbin:/usr/bin"
TEXTDOMAIN="SUNW_OST_OSCMD"
export TEXTDOMAIN

#	/*
#	 * TRANSLATION_NOTE:
#	 * If you make changes to messages of fd_clean command,
#	 * don't forget to make corresponding changes in dev_clean.po file.
#	 */

USAGE=`gettext "%s [-s|-f|-i] device info_label"`

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
    printf "${form}\n" $PROG $DEVICE > /dev/tty
    gettext "Please inform system administrator.\n" > /dev/tty
}

mk_error() {
   chown bin /etc/security/dev/$1
   chmod 0100 /etc/security/dev/$1
}

#
# 		*** Begin main program ***
#

# Parse the argumnets

while getopts ifs c
do
   case $c in
   i)   FLAG=$c;;
   f)   FLAG=$c;;
   s)   FLAG=$c;;
   \?)   printf "${USAGE}\n" $PROG
      exit 1 ;;
   esac
done
shift `expr $OPTIND - 1`

# get the map information

FLOPPY=$1
MAP=`dminfo -v -n $FLOPPY`
DEVICE=`echo $MAP | cut -f1 -d:`
TYPE=`echo $MAP | cut -f2 -d:`
FILES=`echo $MAP | cut -f3 -d:`
DEVFILE=`echo $FILES | cut -f1 -d" "`

#if init then do once and exit

lform=`gettext "%s error: %s."`

if [ "$FLAG" = "i" ] ; then
   x="`eject -q $DEVFILE 2>&1`"		# Determine if there is media in drive
   z="$?"   

   case $z in
   0) 					# Media is in the drive.
	a="`eject -f $DEVFILE 2>&1`"
	b="$?"

	case $b in
	0)				# Media has been ejected 
		con_msg
		exit 0;;
	1)				# Media not ejected
		mk_error $DEVICE
		printf "${lform}\n" $PROG $a >/dev/tty
		e_con_msg
		exit 1;;
	2)			# Error 
		mk_error $DEVICE
		printf "${lform}\n" $PROG $a >/dev/tty
		e_con_msg
		exit 1;;
	3)			# Error - Perhaps drive doesn't support ejection
		mk_error $DEVICE
		printf "${lform}\n" $PROG $a >/dev/tty
		e_con_msg
		exit 1;;
	esac;;
   1) 		# No media in drive
	con_msg
	exit 0;;	
   2)			# Error 
		mk_error $DEVICE
		printf "${lform}\n" $PROG $x >/dev/tty
		e_con_msg
		exit 1;;
   3)			# Error 
		mk_error $DEVICE
		printf "${lform}\n" $PROG $x >/dev/tty
		e_con_msg
		exit 1;;
   esac
else
# interactive clean up
   x="`eject -q $DEVFILE 2>&1`"		# Determine if there is media in drive
   z="$?"   

   case $z in
   0)					# Media is in the drive.
	a="`eject -f $DEVFILE 2>&1`"
	b="$?"
	case $b in
	0)				# Media has been ejected
		user_msg
		exit 0;;
	1)				# Media didn't eject
         	mk_error $DEVICE
		printf "${lform}\n" $PROG $a >/dev/tty
         	e_user_msg
         	exit 1;;
	2)				# Other Error 
		mk_error $DEVICE
		printf "${lform}\n" $PROG $a >/dev/tty
		e_user_msg
         	exit 1;;
	3)				
	
		if echo $a | grep "failed" >/dev/null ; then
         	while true 		# Drive doesn't support eject, so loop	
         	    do
			c="`eject -q $DEVFILE 2>&1`"	# Is floppy in drive?
			d="$?"
            		if [ $d -eq 0 ] ; then		# Yes, Floppy in drive
				form=`gettext "Please remove the floppy from %s."`
               			printf "${form}\n" $DEVICE >/dev/tty
               			/usr/5bin/echo \\007 > /dev/tty
               			sleep 3
            		elif echo $c | grep "NOT" > /dev/null ; then
							# No,Floppy NOT in drive
               			user_msg
               			exit 0
			else				# Error occurred
         			mk_error $DEVICE
				printf "${lform}\n" $PROG $a >/dev/tty
				e_user_msg
         			exit 1
            		fi
         	    done
		else 					# Some other failure
			printf "${lform}\n" $PROG $a >/dev/tty
         		e_user_msg
         		mk_error $DEVICE
         		exit 1
		fi;;
			
	esac;;
   1)							# No media in the drive
         user_msg
         exit 0;;
   2)
       	mk_error $DEVICE
	printf "${lform}\n" $PROG $x >/dev/tty
	e_user_msg
       	exit 1;;
   3)
       	mk_error $DEVICE
	printf "${lform}\n" $PROG $x >/dev/tty
	e_user_msg
       	exit 1;;
   esac
fi
exit 2
