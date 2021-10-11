#! /bin/sh
#
# @(#)ypxfr_1perhour.sh 1.9 92/12/18 Copyr 1990 Sun Microsystems, Inc.  
#
# ypxfr_1perhour.sh - Do hourly NIS map check/updates
#

PATH=/bin:/usr/bin:/usr/lib/netsvc/yp:$PATH
export PATH

# set -xv
ypxfr passwd.byname
ypxfr passwd.byuid 
