#!/sbin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)apache.sh	1.3	99/11/10 SMI"

APACHE_HOME=/usr/apache
CONF_FILE=@@SYSCONFDIR@@/httpd.conf
PIDFILE=/var/run/httpd.pid

if [ ! -f ${CONF_FILE} ]; then
	exit 0
fi

case "$1" in
start)
	/bin/rm -f ${PIDFILE}
	cmdtext="starting"
	;;
restart)
	cmdtext="restarting"
	;;
stop)
	cmdtext="stopping"
	;;
*)
	echo "Usage: $0 {start|stop|restart}"
	exit 1
	;;
esac

echo "httpd $cmdtext."

status=`${APACHE_HOME}/bin/apachectl $1 2>&1`

if [ $? != 0 ]; then
	echo "$status"
	exit 1
fi
exit 0
