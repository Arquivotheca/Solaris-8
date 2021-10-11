#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)cklist.sh	1.2	92/07/14 SMI"

# Checklist comparison on "static" system objects attributes
#
# This script compares the master copy of checklist, which
# lists the expected attributes of specified "static" system
# objects, with a current snapshot of these same objects and
# reports and differences found.
#
# If the master copy is not found, it will be reported, and
# the current snapshot will become the master copy -- no
# comparison can be done, of course.
#
# Since the creation of the checklist involves running the checksum
# program, sum(1), which requires read access on the system objects,
# superuser privilege is required for successful completion of this
# task.

# Create master copy, if not created already; else
# create temporary file and compare with master.
tmpcklist=/tmp/cklist.${ASETSECLEVEL}.$$
mastercklist=${ASETDIR}/masters/cklist.${ASETSECLEVEL}

echo
echo "*** Begin Checklist Task ***"

if [ "$UID" -ne 0 ]
then
	echo
	echo "You are not authorized for the creation and/or comparison"
	echo "of system checklist. Task skipped."
	exit 3
fi

if [ ! -s $mastercklist ]
then
	echo
	echo "No checklist master - comparison not performed."
	echo "... Checklist master is being created now. Wait ..."
	/bin/sh ${ASETDIR}/tasks/create_cklist $mastercklist
	echo "... Checklist master created."
else
	echo
	echo "... Checklist snapshot is being created. Wait ..."
	/bin/sh ${ASETDIR}/tasks/create_cklist $tmpcklist
	echo "... Checklist snapshot created."
	echo
	/bin/cmp -s $mastercklist $tmpcklist
	if [ $? -eq 0 ]
	then
		echo "No differences in the checklist."
	else
		echo "Here are the differences in the checklist."
		echo "< lines are from the master;"
		echo "> lines are from the current snapshot"
		echo
		$DIFF $mastercklist $tmpcklist
	fi
	$RM -f $tmpcklist
fi

echo
echo "*** End Checklist Task ***"
