#!/bin/sh

#
#ident	"@(#)makelibcmdcatalog.sh	1.1	92/09/05 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

XGETTEXT=xgettext
MSGDIR=$1

#
# Change Directory
#
	rm -f *.po

#
#	get list of files
#
FILES=`grep dgettext *.c | sed "s/:.*//" | sort | sed "s/\.c//" | uniq`


#
#	Create po files
#		No need for options for xgettext
#
for	i in ${FILES}
do
	cat ${i}.c | sed "s/_dgettext/gettext/" > ${i}.i
	${XGETTEXT} ${i}.i
	cat messages.po | sed "/^domain/d" > ${i}.po
	rm -f ${i}.i messages.po
done

#
# 	Copy .po files
#
	cp *.po		${MSGDIR}

#
#	And remove them
#
	rm -f *.po
