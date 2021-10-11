#!/sbin/sh
#
# Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T.
# All rights reserved.
#
# THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
# The copyright notice above does not evidence any
# actual or intended publication of such source code.
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)perf.sh	1.7	97/12/08 SMI"

# Uncomment the following lines to enable system activity data gathering.
# You will also need to uncomment the sa entries in the system crontab
# /var/spool/cron/crontabs/sys.  Refer to the sar(1) and sadc(1m) man pages
# for more information.

# if [ -z "$_INIT_RUN_LEVEL" ]; then
# 	set -- `/usr/bin/who -r`
# 	_INIT_RUN_LEVEL="$7"
# 	_INIT_RUN_NPREV="$8"
# 	_INIT_PREV_LEVEL="$9"
# fi
# 
# if [ $_INIT_RUN_LEVEL -ge 2 -a $_INIT_RUN_LEVEL -le 4 -a \
#     $_INIT_RUN_NPREV -eq 0 -a \( $_INIT_PREV_LEVEL = 1 -o \
#     $_INIT_PREV_LEVEL = S \) ]; then
# 
# 	/usr/bin/su sys -c "/usr/lib/sa/sadc /var/adm/sa/sa`date +%d`"
# fi
