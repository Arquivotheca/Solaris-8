#!/bin/ksh -p
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)bld_awk_pkginfo.ksh	1.1	99/08/11 SMI"

usage()
{
	echo "Usage: $0 -p <prodver> -m <mach> -o <awk_script>"
	exit 1
}

#
# Awk strings
#
# Two VERSION patterns: one for Dewey decimal, one for Dewey plus ",REV=n".
# The first has one '=' character and the second has two or more '=' characters.
#
VERSION1="VERSION=[^=]*$"
VERSION2="VERSION=[^=]*=.*$"
PRODVERS="^SUNW_PRODVERS="
ARCH='ARCH=\"ISA\"'

rev=$(date "+%Y.%m.%d.%H.%M")
unset mach prodver awk_script

while getopts o:p:m: c; do
	case $c in
	o) awk_script=$OPTARG ;;
	m) mach=$OPTARG ;;
	p) prodver=$OPTARG ;;
	\?) usage ;;
	esac
done

[[ -z "$prodver" || -z "$mach" || -z "$awk_script" ]] && usage
[[ -f $awk_script ]] && rm -f $awk_script

#
# Build awk script which will process all the pkginfo.tmpl files.
# The first VERSION pattern is replaced with a leading quotation mark.
#
cat << EOF > $awk_script
/$VERSION1/ {
      sub(/\=[^=]*$/,"=\"$rev\"")
      print
      next
   }
/$VERSION2/ {
      sub(/\=[^=]*$/,"=$rev\"")
      print
      next
   }
/$PRODVERS/ { 
      printf "SUNW_PRODVERS=\"%s\"\n", "$prodver" 
      next
   }
/$ARCH/ {
      printf "ARCH=\"%s\"\n", "$mach"
      next
   }
{ print }
EOF
