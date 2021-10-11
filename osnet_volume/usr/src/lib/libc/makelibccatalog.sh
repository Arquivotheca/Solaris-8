#!/bin/sh

#
#ident	"@(#)makelibccatalog.sh	1.3	98/12/03 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

XGETTEXT=xgettext
MSGDIR=$1

#
# Change Directory
#
	cd ./port/gen
	rm -f *.po

#
#	get list of files
#
FILES=`grep gettext *.c | sed "s/:.*//" | sort | sed "s/\.c//" | uniq`


#
#	Create po files
#		No need for options for xgettext
#
for	i in ${FILES}
do
	cat ${i}.c | sed "s/_libc_gettext/gettext/" > ${i}.i
	${XGETTEXT} ${i}.i
	cat messages.po | sed "/^domain/d" > ${i}.po
	rm -f ${i}.i messages.po
done

#
#	Create po files
#		Use -a
#

# First, create errlst.c, if it doesn't exist.
# new_list.c is created as a side effect
if [ ! -f errlst.c ]; then
	awk -f errlist.awk errlist
	rmerr="errlst.c new_list.c"
else
	rmerr=
fi

for	i in siglist errlst
do
	cat ${i}.c | sed "s/_libc_gettext/gettext/" > ${i}.i
	${XGETTEXT} -a  ${i}.i
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
	rm -f *.po ${rmerr}
