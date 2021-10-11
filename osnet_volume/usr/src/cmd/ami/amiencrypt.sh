#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)amiencrypt.sh	1.1 99/07/11 SMI"
#	Script to invoke amiencrypt command
#

JAVA_HOME=/usr/java1.2; export JAVA_HOME
CLASSPATH=/usr/share/lib/ami/ami.jar:/usr/share/lib/ami; export CLASSPATH
LD_LIBRARY_PATH=/usr/lib; export LD_LIBRARY_PATH
JAVA=$JAVA_HOME/jre/bin/java

if [ ! -x $JAVA ]
then
	exit 1;
fi

$JAVA com.sun.ami.cmd.AMI_EncryptCmd "$@"
