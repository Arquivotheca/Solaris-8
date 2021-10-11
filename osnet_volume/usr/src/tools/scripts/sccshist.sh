#!/bin/sh
#
# Copyright (c) 1993-1998 by Sun Microsystems, Inc.
# All rights reserved.
# 
#ident	"@(#)sccshist.sh	1.1	99/01/11 SMI"
#
#  Print sccs history of a file with
#  comment and differences for each delta.
#
#  Usage:  sccshist  <filename>
#

FILE=$1
F1=/tmp/sid1.$$
F2=/tmp/sid2.$$
trap "rm -f $F1 $F2 ; exit" 0 1 2 3 15

sccs prt $FILE |
while read LINE
do
   set - $LINE
   if [ $1 = "D" ] ; then
      sccs get -s -p -k -r$2 $FILE > $F1
      if [ -r $F2 ] ; then
         diff -wt $F1 $F2
         echo "________________________________________________________"
      fi
      mv $F1 $F2
   fi
   echo "$LINE"
done

