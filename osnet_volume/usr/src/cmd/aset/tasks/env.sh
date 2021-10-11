#!/bin/sh
#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)env.sh	1.5	96/05/22 SMI"

dot_cshrc=".cshrc"
dot_profile=".profile"
dot_login=".login"
root_env="/$dot_cshrc /$dot_profile /$dot_login /etc/profile"

########## FUNCTIONS ##########

check_umask()
{
   for i in $root_env
   do
      if [ -s $i ]
      then
         umask=`$GREP umask $i 2>/dev/null`
         if [ $? -eq 0 ]
         then
            mask=`echo $umask | $AWK '{ \
	       if ($2 != "") { \
	          if (length($2) == 1) \
		     print "00"$2; \
	          else if (length($2) == 2) \
		     print "0"$2; \
	          else \
		     print $2; \
	       } else
	          print "000";
            }'`
	    perm=`echo $mask | $SED 's/..\(.\).*/\1/'`
	    if [ "$perm" -lt 6 ]
	    then
	       if [ "$umask" ]
	       then
	          echo
	          echo "Warning! umask set to $umask in $i - not recommended."
	       fi
	    fi
         fi
      fi
   done
}

check_path()
#
# Usage: check_path users
# Root is always checked.
# use /tmp for location of working directory.
#   this guarantees that we access the temp file
#   while being the `user'
#
{
 tmpenv=/tmp/tmpenv.$$
 tmppath=/tmp/tmppath.$$
 tmpstatus=/tmp/tmpstatus.$$

 for user in root $*
 do

   home=`echo $user | $HOMEDIR`
   if [ "$home" = "NONE" ]
   then
      continue
   fi
   cshrc=`echo "${home}/.cshrc" | $SED 's/\/\//\//g'`
   profile=`echo "${home}/.profile" | $SED 's/\/\//\//g'`
   login=`echo "${home}/.login" | $SED 's/\/\//\//g'`

   # check cshrc file
   # note: execute .cshrc file as `user' not as root
   {
      if [ -r $cshrc ]
      then
         echo "#!/bin/csh" > $tmpenv
	 echo "set home = $home" >> $tmpenv
	 echo "set path =" >> $tmpenv
         echo "source $cshrc" >> $tmpenv
         echo "echo \$PATH > $tmppath" >> $tmpenv
         chmod 666 $tmppath
        /bin/su $user -c "/bin/csh $tmpenv > /dev/null 2>&1"
      else
        > $tmppath
      fi
   }
   $SED -n -e 's/^.*\(::\).*/\1/p' $tmppath > $tmpstatus
   $SED -n -e 's/^.*\(:\.:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(=:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(:\.\)$/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^\(\.:\).*/\1/p' $tmppath >> $tmpstatus

   status=`$CAT $tmpstatus`
   if [ "$status" != "" ]
   then
	echo
	echo "Warning! \".\" is in path variable!"
	echo "         Check $cshrc file."
   fi

   for i in `$CAT $tmppath | $SED 's/:/ /g'`
   do
      echo $i
   done | $SORT -u |
   while read i
   do
	if [ -d $i -a "$i" != "." ]
	then
	    if $IS_WRITABLE $i
	    then
		echo
		echo "Warning! Directory $i is world writable!"
		echo "         Should not be in path variable."
		echo "         Check $cshrc file."
	    fi
	fi
   done

   # check login file
   # note: execute .login file as `user' not as root
   {
      if [ -r $login ]
      then
         echo "#!/bin/csh" > $tmpenv
	 echo "set home = $home" >> $tmpenv
	 echo "set path =" >> $tmpenv
         echo "source $login" >> $tmpenv
         echo "echo \$PATH > $tmppath" >> $tmpenv
         chmod 666 $tmppath
        /bin/su $user -c "/bin/csh $tmpenv > /dev/null 2>&1"
      else
         > $tmppath
      fi
   }
   $SED -n -e 's/^.*\(::\).*/\1/p' $tmppath > $tmpstatus
   $SED -n -e 's/^.*\(:\.:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(=:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(:\.\)$/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^\(\.:\).*/\1/p' $tmppath >> $tmpstatus

   status=`$CAT $tmpstatus`
   if [ "$status" != "" ]
   then
	echo
	echo "Warning! \".\" is in path variable!"
	echo "         Check $login file."
   fi

   for i in `$CAT $tmppath | $SED 's/:/ /g'`
   do
      echo $i
   done | $SORT -u |
   while read i
   do
	if [ -d $i -a "$i" != "." ]
	then
	    if $IS_WRITABLE $i
	    then
		echo
		echo "Warning! Directory $i is world writable!"
		echo "         Should not be in path variable."
		echo "         Check $login file."
	    fi
	fi
   done

   # check profile file
   # note: execute .profile file as `user' not as root
   {
      if [ -r $profile ]
      then
         echo "#!/bin/sh" > $tmpenv
	 echo "HOME=$home; export HOME" >> $tmpenv
	 echo "PATH=" >> $tmpenv
         echo ". $profile" >> $tmpenv
         echo "echo \$PATH" >> $tmpenv
         su $user -c "/bin/sh $tmpenv 2> /dev/null"
      fi
   } > $tmppath
   $SED -n -e 's/^.*\(::\).*/\1/p' $tmppath > $tmpstatus
   $SED -n -e 's/^.*\(:\.:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(=:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(:\.\)$/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^\(\.:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^\(:\).*/\1/p' $tmppath >> $tmpstatus
   $SED -n -e 's/^.*\(:\)$/\1/p' $tmppath >> $tmpstatus

   status=`$CAT $tmpstatus`
   if [ "$status" != "" ]
   then
	echo
	echo "Warning! \".\" is in path variable!"
	echo "         Check $profile file."
   fi

   for i in `$CAT $tmppath | $SED 's/:/ /g'`
   do
      echo $i
   done | $SORT -u |
   while read i
   do
	if [ -d $i -a "$i" != "." ]
	then
	    if $IS_WRITABLE $i
	    then
		echo
		echo "Warning! Directory $i is world writable!"
		echo "         Should not be in path variable."
		echo "         Check $profile file."
	    fi
	fi
   done

  done # for user
  $RM -f $tmpenv $tmppath $tmpstatus
} # end check_path

########## MAIN ##########

echo
echo "*** Begin Enviroment Check ***"

# relocate to / so that csh can stat .
cd /

check_umask

if [ "$CHECK_USERS" != "" ]
then
   check_path `$CAT $CHECK_USERS`
else
   check_path
fi

echo
echo "*** End Enviroment Check ***"
