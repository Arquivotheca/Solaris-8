#!/bin/sh
#
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)tune.restore.sh	1.5	94/12/07 SMI"

# This script reverses file attributes changed by tune.task back
# to what they used to be according to the archive file -

STR_TO_MODE=${ASETDIR}/util/str_to_mode
FILE_ATTR=${ASETDIR}/util/file_attr
AWK=/bin/awk
LS=/bin/ls
export STR_TO_MODE FILE_ATTR AWK LS

# name of this script
myname=`expr $0 : ".*/\(.*\)" \| $0`

# -p option is for previewing the changes.
usage="$myname [-p]"

fail()
{
   echo
   echo "$myname failed:"
   echo $*
   exit 1
}

not_lower()
# usage: not_lower level1 level2
# return: 0 if level1 is not lower than level2 (higher or equal)
#         1 if lower
{
   level1=$1
   level2=$2
   case $level1 in
   null)
      if [ "$level2" = "null" ]
      then
         return 0
      fi;;
   low)
      if [ "$level2" = "null" -o "$level2" = "low" ]
      then
	 return 0
      fi;;
   med)
      if [ "$level2" != "high" ]
      then
	 return 0
      fi;;
   high)
      return 0;;
   esac
   return 1
}

between_levels()
# usage: between_levels level1 level2
# prints all the levels in between (inclusively) level1 and level2
# from the highest down.
# level1 is assumed to be not lower than level2.
{
   level1=$1
   level2=$2
   if not_lower $level1 $level2
   then
      l=$level1
      echo "$l \c"
      while [ "$l" != "$level2" ]
      do
         case $l in
         high)   l=med;;
         med)    l=low;;
         low)    l=null;;
         esac
         echo "$l \c"
      done
      echo
   fi
}

CHOWN=/usr/bin/chown
CHMOD=/bin/chmod
CHGRP=/bin/chgrp

echo
echo "Beginning $myname..."
echo "(This may take a while.)"

if [ "$ASETDIR" = "" ]
then
   fail "ASETDIR variable undefined."
fi  
    
if [ $UID -ne 0 ]
then
   fail "Permission denied."
fi

if [ $# -gt 0 ]
then
   if [ "$1" = "-p" ]
   then
      echo
      echo "Performing preview only ..."
      CHOWN="echo chown "
      CHMOD="echo chmod "
      CHGRP="echo chgrp "
   else
      echo $usage
      exit 1
   fi
fi

export CHOWN CHMOD CHGRP

LEVELS=`between_levels $PREV_ASETSECLEVEL $ASETSECLEVEL`
export LEVELS

arch_files=""
for i in $LEVELS
do
   arch_files="$ASETDIR/archives/tune.arch.$i $arch_files"
done
if [ "$arch_files" != "" ]
then
   arch_files=`/bin/ls -t $arch_files 2> /dev/null`
fi
for arch in $arch_files
do
   while read path junkpath mode user group type junk
   do
      # Skip comments and white lines
      if [ "$path" = "#" ]
      then
         continue;
      elif [ "$path" = "" ]
      then
         continue;
      fi
   
      # Warn and skip lines without all the required fields
      if [ "$type" = "" ]
      then
         echo
         echo "Warning: bad entry:"
         echo "$path $mode $user $group $type"
         continue;
      fi
   
      # Warn and skip lines with too many fields
      if [ "$junk" != "" ]
      then
         echo
         echo "Warning: bad entry:"
         echo "$path $mode $user $group $type $junk"
         continue;
      fi
   
      #
      #   If the object does not exist on this system then skip it.
      #
      if [ ! -d "$path" -a ! -f "$path" ]
      then
	 echo
	 echo "Warning! $path does not exist - skipped."
         continue;
      fi

      old_attr=`$FILE_ATTR $path`

      if [ "$type" != "symlink" -a \
	   "$mode" != `echo $old_attr | $AWK '{print $2}'` ]
      then
         if [ "$type" = "directory" ]
         then
            $CHMOD g-s "$path"
         fi
         $CHMOD "$mode" "$path"
      fi

      if [ "$user" != `echo $old_attr | $AWK '{print $3}'` ]
      then
         $CHOWN "$user" "$path"
      fi

      if [ "$group" != `echo $old_attr | $AWK '{print $4}'` ]
      then
         $CHGRP "$group" "$path"
      fi
   done < $arch # while loop
done # for loop

echo
echo "$myname completed."
