#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)gpchk.awk	1.2	92/07/14 SMI"

BEGIN {FS = ":" }

(substr($1,1,1) != "+") {
   if ($0 ~ /^[ 	]*$/) {
      printf("Warning!  Group file, line %d, is blank\n", NR)
   } else {
      if (NF != 4) {
         printf("Warning!  Group file, line ");
	 printf("%d, does not have 4 fields: %s\n", NR, $0);
      }
      if ($1 !~ /[A-Za-z0-9]/) {
         printf("Warning!  Group file, line ");
	 printf("%d, nonalphanumeric group id: %s\n", NR, $0)
      }
      if ($2 != "" && $2 != "*") {
#         if ("'$C2'" != "true") {
#            printf("Warning!  Group file, line ");
#	    printf("%d, group has password: %s\n", NR, $0);
#         } else {
#            if ("#$"$1 != $2)
#            printf("Warning!  Group file, line ");
#	    printf("%d, group has invalid field for C2:\n%s\n", NR, $0)
#	 }
      }
      if ($3 !~ /[0-9]/) {
         printf("Warning!  Group file, line ");
	 printf("%d, nonnumeric group id: %s\n", NR, $0)
      }
   }
}
