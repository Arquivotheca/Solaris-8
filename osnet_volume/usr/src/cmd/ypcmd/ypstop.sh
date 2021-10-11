#!/bin/sh
#
#ident	"@(#)ypstop.sh	1.5	97/02/26 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# DESCRIPTION:
# Script to stop NIS services. This script handles both client and
# server cases.

# 
# Bring all of the NIS daemons to a halt.
# note the "daemons" list is ordered, and they will be
# stopped in that order.
#
daemons="ypxfrd rpc.yp ypbind ypserv fnsypd"
daemons="/|"`echo "$daemons" | tr " " "|"`"|/"
pidlist=`/usr/bin/ps -e -o "pid fname" | nawk '$2 ~ x { print $1 }' x="$daemons"`
if [ "$pidlist" ]; then
	kill $pidlist
fi

