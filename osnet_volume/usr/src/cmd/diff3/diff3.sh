#!/usr/bin/sh
#	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
#	  All Rights Reserved

#	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
#	The copyright notice above does not evidence any
#	actual or intended publication of such source code.

#	Copyright (c) 1999 by Sun Microsystems, Inc.
#	All rights reserved.

#ident	"@(#)diff3.sh	1.15	99/05/21 SMI"	/* SVr4.0 1.4	*/

usage="usage: diff3 file1 file2 file3"

# mktmpdir - Create a private (mode 0700) temporary directory inside of /tmp
# for this process's temporary files.  We set up a trap to remove the 
# directory on exit (trap 0), and also on SIGHUP, SIGINT, SIGQUIT, SIGPIPE,
# and SIGTERM.

mktmpdir() {
	tmpdir=/tmp/diff3.$$
	trap '/usr/bin/rm -rf $tmpdir' 0 1 2 3 13 15
	/usr/bin/mkdir -m 700 $tmpdir || exit 1
}
mktmpdir

e=
case $1 in
-*)
	e=$1
	shift;;
esac
if [ $# != 3 ]; then
	echo ${usage} 1>&2
	exit 1
fi
if [ \( -f $1 -o -c $1 \) -a \( -f $2 -o -c $2 \) -a \( -f $3 -o -c $3 \) ]; then
	:
else
	echo ${usage} 1>&2
	exit 1
fi
f1=$1 f2=$2 f3=$3
if [ -c $f1 ]
then
	/usr/bin/cat $f1 > $tmpdir/d3c$$
	f1=$tmpdir/d3c$$
fi
if [ -c $f2 ]
then
	/usr/bin/cat $f2 > $tmpdir/d3d$$
	f2=$tmpdir/d3d$$
fi
if [ -c $f3 ]
then
	/usr/bin/cat $f3 > $tmpdir/d3e$$
	f3=$tmpdir/d3e$$
fi

/usr/bin/diff $f1 $f3 > $tmpdir/d3a$$ 2> $tmpdir/d3a$$.err
STATUS=$?
if [ $STATUS -gt 1 ]
then
	/usr/bin/cat $tmpdir/d3a$$.err
	exit $STATUS
fi

/usr/bin/diff $f2 $f3 > $tmpdir/d3b$$ 2> $tmpdir/d3b$$.err
STATUS=$?
if [ $STATUS -gt 1 ]
then
	/usr/bin/cat $tmpdir/d3b$$.err
	exit $STATUS
fi

/usr/lib/diff3prog $e $tmpdir/d3[ab]$$ $f1 $f2 $f3
