#!/sbin/sh
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
#	All rights reserved.
#
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Copyright (c) 1984 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#ident	"@(#)logchecker.sh	1.7	98/10/07 SMI"	/* SVr4.0 1.6	*/

# This command is used to determine if the cron log file is approaching the
# ulimit of the system.  If it is, then the log file will be moved to olog
# this command is executed by the crontab entry 'root'
# When file size limit is unlimited or unreasonable huge,
# we set one for it to avoid disk hogging.

#set umask
umask 022

# log files
LOG=/var/cron/log
OLOG=/var/cron/olog

# If MARKER is 0, we may not be able to copy LOG to OLOG due to file size limit
# by the time this cron job is run.
MARKER=100
LIMIT=`ulimit`
if [ "$LIMIT" = "unlimited" -o $LIMIT -gt 1024 ]
then
	LIMIT=1024	# better than nothing: 0.5MB file
fi
target=`expr $LIMIT - $MARKER`
if [ $target -ge $MARKER ]	# should be the common case
then
	LIMIT=$target	# use this as threshold, keep the old LIMIT otherwise.
fi

# find the size of the log file (in blocks)
if [ -f $LOG ]
then
	FILESIZE=`du -a $LOG | cut -f1`
else
	exit 1
fi

# move log file to olog file if the file is too big
if [ $FILESIZE -ge $LIMIT ]
then
	cp $LOG $OLOG
	chgrp bin $OLOG
	>$LOG
fi
