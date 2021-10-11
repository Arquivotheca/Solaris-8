#! /bin/sh
#
# @(#)ypxfr_2perday.sh 1.9 92/12/18 Copyr 1990 Sun Microsystems, Inc.  
#
# ypxfr_2perday.sh - Do twice-daily NIS map check/updates
#

PATH=/bin:/usr/bin:/usr/lib/netsvc/yp:$PATH
export PATH

# set -xv
ypxfr hosts.byname
ypxfr hosts.byaddr
ypxfr ethers.byaddr
ypxfr ethers.byname
ypxfr netgroup
ypxfr netgroup.byuser
ypxfr netgroup.byhost
ypxfr mail.aliases 
