#!/usr/bin/ksh -p
#
#ident "@(#)bld_awk_pkginfo.ksh	1.1	93/09/10 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# Simple script which builds the awk_pkginfo awk script.  This awk script
# is used to convert the pkginfo.tmpl files into pkginfo files
# for the build.
#


usage()
{
   echo "usage: bld_awk_pkginfo -p <prodver> -m <mach> -o <awk_script>"
}
#
# Awk strings
#
VERSION="VERSION\="
PRODVERS="^PRODVERS\="
ARCH='ARCH=\"ISA\"'


#
# parse command line
#
mach=""
prodver=""
awk_script=""

while getopts o:p:m: c
do
   case $c in
   o)
      awk_script=$OPTARG
      ;;
   m)
      mach=$OPTARG
      ;;
   p)
      prodver=$OPTARG
      ;;
   \?)
      usage
      exit 1
      ;;
   esac
done

if [[ ( -z $prodver ) || ( -z $mach ) || ( -z $awk_script ) ]]
then
   usage
   exit 1
fi

if [[ -f $awk_script ]]
then
	rm -f $awk_script
fi

#
# Build REV= field based on date
#
rev=$(date "+%y.%m.%d.%H.%M")

#
# Build awk script which will process all the
# pkginfo.tmpl files.
#
rm -f $awk_script
cat << EOF > $awk_script
/$VERSION/ {
      sub(/\=[^=]*$/,"=$rev\"")
      print
      next
   }
/$PRODVERS/ { 
      printf "PRODVERS=\"%s\"\n", "$prodver" 
      next
   }
/$ARCH/ {
      printf "ARCH=\"%s\"\n", "$mach"
      next
   }
{ print }
EOF

