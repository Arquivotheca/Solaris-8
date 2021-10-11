#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)asetenv.sh	1.2	92/07/14 SMI"

# This is the "dot" script for ASET and should be invoked before
# running any ASET tasks.

###########################################
#                                         #
#      User Configurable Parameters       #
#                                         #
###########################################

CKLISTPATH_LOW=${ASETDIR}/tasks:${ASETDIR}/util:${ASETDIR}/masters:/etc
CKLISTPATH_MED=${CKLISTPATH_LOW}:/usr/bin:/usr/ucb
CKLISTPATH_HIGH=${CKLISTPATH_MED}:/usr/lib:/sbin:/usr/sbin:/usr/ucblib
YPCHECK=false
UID_ALIASES=${ASETDIR}/masters/uid_aliases
PERIODIC_SCHEDULE="0 0 * * *"
TASKS="firewall env sysconf usrgrp tune cklist eeprom"

###########################################
#                                         #
# ASET Internal Environment Variables     # 
#                                         #
# Don't change from here on down ...      #
# there shouldn't be any reason to.       #
#                                         #
###########################################

export YPCHECK UID_ALIASES PERIODIC_SCHEDULE

# full paths of system utilites
AWK=/bin/awk
LS=/bin/ls
RM=/bin/rm
MV=/bin/mv
MKDIR=/bin/mkdir
LN=/bin/ln
SUM=/bin/sum
CUT=/bin/cut
GREP=/bin/grep
EGREP=/bin/egrep
DIFF=/bin/diff
MAIL=/bin/mail
CHGRP=/bin/chgrp
CHMOD=/bin/chmod
CHOWN=/usr/bin/chown
SORT=/bin/sort
UNIQ=/bin/uniq
YPCAT=/bin/ypcat
PS=/bin/ps
CP=/bin/cp
REALPATH=${ASETDIR}/util/realpath
ADDCKSUM=${ASETDIR}/util/addcksum
MINMODE=${ASETDIR}/util/minmode
FILE_ATTR=${ASETDIR}/util/file_attr
STR_TO_MODE=${ASETDIR}/util/str_to_mode
IS_WRITABLE=${ASETDIR}/util/is_writable
IS_READABLE=${ASETDIR}/util/is_readable
HOMEDIR=${ASETDIR}/util/homedir
SED=/bin/sed
ED=/bin/ed
CAT=/bin/cat
EXPR=/bin/expr
CRONTAB=/bin/crontab
TOUCH=/bin/touch

sysutils="AWK LS RM MV MKDIR LN SUM CUT GREP EGREP DIFF MAIL CHGRP CHMOD CHOWN PS \
CP SORT UNIQ YPCAT REALPATH ADDCKSUM MINMODE FILE_ATTR STR_TO_MODE \
ED SED CAT IS_WRITABLE IS_READABLE HOMEDIR EXPR CRONTAB TOUCH"

progs="$AWK $LS $RM $MV $MKDIR $LN $SUM $CUT $GREP $EGREP \
$DIFF $MAIL $CHGRP $CHMOD $CHOWN $PS $CRONTAB $TOUCH \
$CP $SORT $UNIQ $YPCAT $REALPATH $ADDCKSUM $MINMODE $FILE_ATTR \
$STR_TO_MODE $ED $SED $CAT $IS_WRITABLE $IS_READABLE $HOMEDIR $EXPR"

noprog=false
for i in $progs
do
	if [ ! -x $i ]
	then
		if [ "$noprog" = "false" ]
		then
			noprog=true
			echo
			echo "ASET startup unsuccessful:"
		else
			echo "Could not find executable $i."
		fi
	fi
done
if [ "$noprog" = "true" ]
then
	echo "Unable to proceed."
	exit
fi

export $sysutils

TIMESTAMP=`date '+%m%d_%H:%M'`
QUIT="ASET: irrecoverable error -- exiting ..."

case $ASETSECLEVEL in
low)	CKLISTPATH=`echo "${CKLISTPATH_LOW}"`;;
med)	CKLISTPATH=`echo "${CKLISTPATH_MED}"`;;
high)	CKLISTPATH=`echo "${CKLISTPATH_HIGH}"`;;
*)	echo $QUIT;
	exit 3;;
esac

# Set up report directory
$RM -rf ${ASETDIR}/reports/${TIMESTAMP}
$MKDIR ${ASETDIR}/reports/${TIMESTAMP}
REPORT=${ASETDIR}/reports/${TIMESTAMP}
$RM -rf ${ASETDIR}/reports/latest
$LN -s $REPORT ${ASETDIR}/reports/latest

# temorary files directory
TMP=${ASETDIR}/tmp

export TASKS TIMESTAMP QUIT REPORT CKLISTPATH
export TMP
