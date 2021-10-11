#! /usr/bin/sh
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# ident	"@(#)bld_lint.sh	1.1	98/08/28 SMI"
#

DASHES="============================================================"

MACH=	`uname -p`

if [ $MACH = "sparc" ]; \
then
	MACH64="sparcv9"
else
	MACH64="unknown"
fi

LOG=lint.$MACH.log


#
# Keep the first run as a backup, so that
# subsequent runs can diff against it.
#
if [ -f $LOG ]
then
	if [ ! -f $LOG.bak ]
	then
		mv $LOG $LOG.bak
	else
		rm -f $LOG
	fi
fi

#
# Grab the lint.out from all of our directories.
#
for ii in $*
do
	if [ $ii = ".WAIT" ]
	then
		continue
	fi

	# Handle the libldmake and rdb_demo directories.
	# They're ignored for now because linting the lex
	# and yacc stuff in rdb is a mess, especially for
	# the 64-bit side (broken lint libs for liby, etc.).
	BN=`basename $ii`
	if [ $BN = $MACH  -o $BN = $MACH64 ]
	then
		continue
	fi

	# Concatinate the lint.out to our log file.
	echo $ii/$MACH >> $LOG
	echo $DASHES >> $LOG
	cat $ii/$MACH/lint.out >> $LOG
	echo "\n" >> $LOG

	# If there is a 64-bit directory, tack that
	# on as well.
	if [ -f $ii/$MACH64/lint.out ]
	then
		echo $ii/$MACH64 >> $LOG
		echo $DASHES >> $LOG
		cat $ii/$MACH64/lint.out >> $LOG
		echo "\n" >> $LOG
	fi
done


#
# If there is a backup log, diff the current
# one against it.
#
if [ -f $LOG.bak ]
then
	echo "Running diff on log file..."
	diff $LOG.bak $LOG
fi

exit 0
