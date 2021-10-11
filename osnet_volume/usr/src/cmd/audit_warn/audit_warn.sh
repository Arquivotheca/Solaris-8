#! /bin/sh
#
#	audit_warn.sh
#
#       @(#)audit_warn.sh 1.3 97/01/14 SMI
#
#  Copyright (c) 1987 by Sun Microsystems, Inc.
#

# This shell script warns the administrator when there are problems or
# potential problems with the audit daemon.  The default script sends
# a message to the machine console in the case where there
# is no audit space available.  It has comments in a few places where
# additional actions might be appropriate (eg. clearing some space).
#

# Check usage
if [ "$#" -lt "1" -o "$#" -gt "2" ]
then
	echo >&2 "Usage: $0 <option> [<args>]"
	exit 1
fi

# Process args
while [ -n "$1" ]
do

	SUBJECT="AUDIT DAEMON WARNING ($1)"

	case "$1" in 

	"soft" )	# Check soft arg 
			# One audit filesystem has filled to the soft limit
			# set up in audit_control.

			if [ ! -n "$2" ]
			then
				echo 2>&1 "Need filename arg with this option!"
				exit 1
			else
				FILE=$2
			fi

			# Set message
			MESSAGE="Soft limit exceeded in file $FILE."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;
		
	"allsoft" )	# Check all soft arg
			# All the audit filesystems have filled to the soft
			# limit set up in audit_control.

			# Set message
			MESSAGE="Soft limit exceeded on all filesystems."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;
	
	"hard" )	# Check hard arg
			# One audit filesystem has filled completely.

			if [ ! -n "$2" ]
			then
				echo 2>&1 "Need filename arg with this option!"
				exit 1
			else
				FILE=$2
			fi

			# Set message
			MESSAGE="Hard limit exceeded in file $FILE."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;

	"allhard" )	# Check all hard arg
			# All the audit filesystems have filled completely.
			# The audit daemon will remain in a loop sleeping
			# and checking for space until some space is freed.

			if [ ! -n "$2" ]
			then
				echo 2>&1 "Need count arg with this option!"
				exit 1
			else
				COUNT=$2
			fi

			# Set message
			MESSAGE="Hard limit exceeded on all filesystems. (count=$COUNT)"

			if [ $COUNT -eq 1 ]
			then
				echo "$0: $MESSAGE" | \
					/usr/ucb/mail -s "$SUBJECT" audit_warn
			fi
			# This might be a place to make space in the
			# audit file systems.

			break
			;;

	"ebusy" )	# Check ebusy arg
			# The audit daemon is already running and can not
			# be started more than once.

			# Set message
			MESSAGE="The audit daemon is already running on this system."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;

	"tmpfile" )	# Check tempfile arg
			# The tempfile used by the audit daemon could not
			# be opened even though it was unlinked.
			# This error will cause the audit daemon to exit.

			# Set message
			MESSAGE="The audit daemon can not open audit_tmp.  "
			MESSAGE="${MESSAGE}This implies a serious problem. "
			MESSAGE="${MESSAGE}The audit daemon has exited!"
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn

			break
			;;

	"nostart" )	# Check no start arg
			# If the system calls fchroot and fchdir are 
			# executed, auditing can not be done because we
			# do not have the required full pathnames for the
			# audit records.  In this case, the audit daemon
			# can not be started until the system is rebooted. 

			MESSAGE="Can not start the audit daemon because fchdir or fchroot was run.  "
			MESSAGE="${MESSAGE}Must reboot to start auditing!"

			# This might be changed to actually do a reboot.
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn

			break
			;;

	"auditoff" )	# Check audit off arg
			# Someone besides the audit daemon called the 
			# system call auditon to "turn auditing off"
			# by setting the state to AUC_NOAUDIT.  This
			# will cause the audit daemon to exit.

			# Set message
			MESSAGE="Auditing has been turned off unexpectedly."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;

	"postsigterm" )	# Check post sigterm arg
			# While the audit daemon was trying to shutdown
			# in an orderly fashion (corresponding to audit -t)
			# it got another signal or an error.  Some records
			# may not have been written.

			# Set message
			MESSAGE="Received some signal or error while writing audit records after SIGTERM.  "
			MESSAGE="${MESSAGE}Some audit records may have been lost."
			echo "$0: $MESSAGE" | \
				/usr/ucb/mail -s "$SUBJECT" audit_warn
			break
			;;

	"getacdir" )	# Check getacdir arg
			# There is a problem getting the directory list from
			# /etc/security/audit_control.  Auditd is
			# going to hang in a sleep loop until the file is
			# fixed.

			if [ ! -n "$2" ]
			then
				echo 2>&1 "Need count arg with this option!"
				exit 1
			else
				COUNT=$2
			fi

			# Set message
			MESSAGE="There is a problem getting the directory"
			MESSAGE="${MESSAGE} list from audit_control  "
			MESSAGE="${MESSAGE}The audit daemon will hang until"
			MESSAGE="${MESSAGE} this file is fixed."
			MESSAGE="${MESSAGE} This message has been displayed $COUNT times."
			if [ $COUNT -eq 1 ]
			then
				echo "$0: $MESSAGE" | \
					/usr/ucb/mail -s "$SUBJECT" audit_warn
			fi
			break
			;;
	
	* )		# Check other args
			echo 2>&1 "Arg not recognized: $1"
			exit 1
			;;

	esac
	
	shift
done
			
echo "$0: $MESSAGE" >/dev/console

exit 0
