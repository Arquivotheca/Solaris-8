#!/bin/sh
#
#ident	"@(#)ypstart.sh	1.8	97/02/26 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# DESCRIPTION:
# Script to start NIS services. This script handles both client and
# server cases. It also checks to see if kerbd is running 
#

# NIS (YP) support.
#
# When to start ypserv :
#	The value of $domain is non-null *and*
#	The directory /var/yp/$domain exists *and*
#	There is an executable ypserv in $YPDIR
#
# When to start ypbind :
# 	The value of $domain is non-null *and*
#	There is an executable ypbind in $YPDIR *and*
#	The directory /var/yp/binding/$domain exists. 
#
# This latter choice is there to switch on ypbind if sysidnis
# tells it to run. (it creates that directory)
#
# After starting ypbind, use ypwhich to determine the master
# server (the passwd map is chosen, but any other would do).
#
# If the name of the master is the same as this hostname, then
# start the rpc.yppasswdd and ypxfrd daemons.
#
# The rpc.yppasswdd daemon can be started with a -D option to
# point it at the passwd/shadow/passwd.adjunct file(s).
# The /var/yp/Makefile uses a PWDIR macro assignment to define
# this directory.  In the rpc.yppasswdd invocation, we attempt
# to grab this info and startup accordingly.
#
# NB: -broadcast option is insecure and transport dependent !
# ... and ypbind is noisy about this fact...
#

#
# SUPPORT FUNCTIONS
#	Search for MAIN to see main script
#

# 
# Halt all of the given daemons.
# note the "daemons" list is ordered, and they will be
# stopped in that order.
#
StopDaemons () {
	daemons="$*"
	daemons="/|"`echo "$daemons" | tr " " "|"`"|/"
	# NOTE: the -o option to ps is only available since Sol 2.5
	pidlist=`/usr/bin/ps -e -o "pid fname" | nawk '$2 ~ x { print $1 }' x="$daemons"`
	if [ "$pidlist" ]
	then
		kill $pidlist
	fi
}	# StopDaemons


#
# Start the NIS services
#	Assumes that $domain has been checked and is set
#

StartYp () {

	# start NIS server
	if [ -x $YPDIR/ypserv -a -d /var/yp/$domain ]; then
		if [ -f /etc/resolv.conf ]; then
			$YPDIR/ypserv -d && echo ' ypserv\c'
		else
			$YPDIR/ypserv && echo ' ypserv\c'
		fi
		sleep 1		# give ypserv time to come up or the 
				# binds may fail below

		YP_SERVER=TRUE	# remember we're a server for later

		# check to see if we are the master
		if [ -x /usr/sbin/makedbm ]; then
			master=`/usr/sbin/makedbm -u /var/yp/$domain/passwd.byname | grep YP_MASTER_NAME | nawk '{ print $2 }'`
		fi
	fi

	# Are we the FNS NIS master server? If so, start the
	# fnsypd, so that users/hosts can update their FNS contexts
	# Also, FNS NIS master server can be different from
	# NIS master server, hence this should not be will the NIS
	# master server script.
	if [ -f /etc/fn/$domain/fns.lock -a X$YP_SERVER = "XTRUE" ]; then
		if [ -x $FNSYPD ]; then
			$FNSYPD > /dev/null 2>&1
			echo " fnsypd\c"
		fi
	fi

	# start ypbind
	if [ -x $YPDIR/ypbind ]; then
		if [ -d $YPSRV -a -f $YPSRV/ypservers ]; then
			$YPDIR/ypbind > /dev/null 2>&1
			echo " ypbind\c"
		elif [ -d $YPSRV ]; then
			$YPDIR/ypbind -broadcast > /dev/null 2>&1
			echo " ypbind\c"
		fi

		# do a ypwhich to force ypbind to get bound
		ypwhich > /dev/null 2>&1
	fi

	# Are we the master server?  If so, start the
	# ypxfrd, rpc.yppasswdd and rpc.ypupdated daemons.
	if [ "$master" = "$hostname" -a X$YP_SERVER = "XTRUE" ]; then
		if [ -x $YPDIR/ypxfrd ]; then
			$YPDIR/ypxfrd && echo ' ypxfrd\c'
		fi
		if [ -x $YPDIR/rpc.yppasswdd ]; then
			PWDIR=`grep "^PWDIR" /var/yp/Makefile 2> /dev/null` \
			&& PWDIR=`expr "$PWDIR" : '.*=[ 	]*\([^ 	]*\)'`
			if [ "$PWDIR" ]; then
				if [ "$PWDIR" = "/etc" ]; then
					unset PWDIR
				else
					PWDIR="-D $PWDIR"
				fi
			fi
			$YPDIR/rpc.yppasswdd $PWDIR -m \
				&& echo ' rpc.yppasswdd\c'
		fi
		if [ -x $YPDIR/rpc.ypupdated -a -f /var/yp/updaters ]; then
			$YPDIR/rpc.ypupdated && echo ' rpc.ypupdated\c'
		fi
	fi
}	# StartYp



#
# MAIN FUNCTION
#

domain=`domainname`
hostname=`uname -n | cut -d. -f1 | tr '[A-Z]' '[a-z]'`
YPDIR=/usr/lib/netsvc/yp
YPSTOP=$YPDIR/ypstop
YPSRV=/var/yp/binding/$domain
FNSYPD=/usr/sbin/fnsypd

case "$#" in
'0')
	# make sure domain is set before going on
	if [ -z "$domain" ]; then
		echo "ERROR: Default domain is not defined. Use \"domainname\" to set domain."
		exit 1
	fi

	# make sure ypserv is not running already
	ypservpid=`/usr/bin/ps -e -o "pid fname" | nawk '$2 ~ /ypserv/ { print $1 }'`
	if [ "$ypservpid" ]; then
		echo "The NIS server (ypserv) is already running."
		echo "Please run $YPSTOP to stop NIS services."
		exit 1
	fi
		
	echo "starting NIS (YP server) services:\c"
	StopDaemons ypbind
	StartYp
	echo " done."
	;;
'1')
	# Assume being called by /etc/init.d/rpc (with rpcstart argument)
	# and caller is responsible for starting kerbd
	if [ "$1" = "rpcstart" ]; then
		if [ ! -z "$domain" ]; then
			StartYp
		fi
	else
		echo "Usage: ypstart"
		exit
	fi
	;;
*)
	echo "Usage: ypstart"
	exit
esac
