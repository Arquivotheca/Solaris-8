#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)swchk.awk	1.2	92/07/14 SMI"

BEGIN {FS = ":" }
{
   if (substr($1,1,1) != "+") {
      if ($0 ~ /^[ 	]*$/) {
	 printf("\nWarning!  Shadow file, line %d, is blank\n", NR)
      } else {
         if (NF != 9) {
	    printf("\nWarning!  Shadow file, line %d,", NR);
	    printf(" does not have 9 fields: \n\t%s\n", $0)
         }
         if ($1 !~ /[A-Za-z0-9]/) {
	    printf("\nWarning!  Shadow file, line %d,", NR);
	    printf(" nonalphanumeric user name: \n\t%s\n", $0)
         }
         if ($2 == "") {
	    printf("\nWarning!  Shadow file, line %d,", NR);
	    printf(" no password: \n\t%s\n", $0)
         }
      }
   }
}
