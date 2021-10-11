#!/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)amilogin.sh	1.1 99/07/11 SMI"
#
# This script is used to invoke the ami login java command. 
# The command line options are passed as they are to  the
# java program, which checks them for correct usage
#

JAVA_HOME=/usr/java1.2; export JAVA_HOME
CLASSPATH=/usr/share/lib/ami/ami.jar:/usr/share/lib/ami; export CLASSPATH
LD_LIBRARY_PATH=/usr/lib; export LD_LIBRARY_PATH
JAVA=$JAVA_HOME/jre/bin/java

if [ ! -x $JAVA ]
then
	exit 1;
fi

$JAVA com.sun.ami.cmd.AMI_LoginToAMIServ "$@"
