#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)aset.restore.sh	1.1	91/07/26 SMI"

# This script calls the restore scripts in each task's directory, *.restore
# to restore the system back to the condition before ASET was ever run.
#
# It also deschedules ASET if it is scheduled.

myname=`expr $0 : ".*/\(.*\)" \| $0`

fail()
{
   echo
   echo "$myname: failed:"
   echo $*
   exit 1
}

dflag=false
if [ $# -gt 0 ]
then
	while getopts d: c
	do
		case $c in
		d)	dflag=true;
			case $OPTARG in
			"" | -*)
				usageerr=true;
				break;;
			*)	ASETDIR=$OPTARG;
			esac;;
		\?)	usageerr=true;
			break;;
		esac
	done
fi

if [ "$usageerr" = "true" ]
then
	echo
	echo "Usage: aset.restore [-d aset_dir]"
	exit 1
fi

# the -d option has the highest priority
if [ "$dflag" = "false" ]
then
	# then check the environment
	if [ "$ASETDIR" = "" ]
	then
		# otherwise set to the default value
		ASETDIR=/usr/aset
	fi
fi

if [ ! -d $ASETDIR ]
then
	echo
	echo "ASET startup unsuccessful:"
	echo "Working directory $ASETDIR missing"
	exit 2
fi

# expand the working directory to the full path
ASETDIR=`$ASETDIR/util/realpath $ASETDIR`
if [ "$ASETDIR" = "" ]
then
	echo
	echo "ASET startup unsuccessful:"
	echo "Cannot expand $ASETDIR to full pathname."
	exit 2
fi
export ASETDIR

echo
echo "$myname: beginning restoration ..."

# get user id 
UID=`id | sed -n 's/uid=\([0-9]*\).*/\1/p'` 
export UID 
 
if [ "$UID" -ne 0 ]
then
   fail "Permission Denied."
fi

# Set level to null
ASETSECLEVEL=null
PREV_ASETSECLEVEL=`/usr/ucb/tail -1 $ASETDIR/archives/asetseclevel.arch`
export ASETSECLEVEL PREV_ASETSECLEVEL

for restore_script in $ASETDIR/tasks/*.restore
do
   echo;echo "Executing $restore_script"
   $restore_script
done

schedule=`/bin/crontab -l root | /bin/grep "aset "`
if [ "$schedule" != "" ]
then
   echo
   echo "Descheduling ASET from crontab file..."
   echo "The following is the ASET schedule entry to be deleted:"
   echo "$schedule"
   echo "Proceed to deschedule: (y/n) \c"
   read answer
   if [ "$answer" = "y" ]
   then
      /bin/crontab -l root | /bin/grep -v "aset " | crontab
   fi
fi

echo
echo "Resetting security level from $PREV_ASETSECLEVEL to null."
echo "null" >> $ASETDIR/archives/asetseclevel.arch
echo
echo "$myname: restoration completed."
