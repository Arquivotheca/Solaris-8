#!/bin/sh
#
# @(#)1.1 tokenize.sh 99/01/11 SMI
#
# Creates "tokenized" FCode programs from Forth source code.
#
# Usage:  tokenize xxx.fth
#
# xxx.fth is the name of the source file, which must end in ".fth"
# The output file will be named xxx.fcode .  It will have an a.out header
# so that it can be downloaded to a PROM burner with "pburn"

#
# Get tokenizer.exe from same directory that this command is in.
#
mypath=`dirname $0`

infile=/tmp/$$
echo 'fcode-version1' > $infile
cat $1 >> $infile
echo 'end0' >> $infile
outfile=`basename $1 .fth`.fcode
(set -x; ${mypath}/forth ${mypath}/tokenize.exe -s "aout-header? off silent? on tokenize $infile $outfile" < /dev/null)
rm $infile
