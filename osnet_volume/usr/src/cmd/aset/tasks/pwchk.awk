#
# Copyright 1990, 1991 Sun Microsystems, Inc.  All Rights Reserved.
#
#
#ident	"@(#)pwchk.awk	1.2	92/07/14 SMI"

#   First line is for a yellow pages entry in the password file.
BEGIN {FS = ":" }
{
   if (substr($1,1,1) != "+") {
      if ($0 ~ /^[ 	]*$/) {
	 printf("\nWarning!  Password file, line %d, is blank\n", NR)
      } else {
         if (NF != 7) {
	    printf("\nWarning!  Password file, line %d,", NR);
	    printf("does not have 7 fields: \n\t%s\n", $0)
         }
         if ($1 !~ /[A-Za-z0-9]/) {
	    printf("\nWarning!  Password file, line %d,", NR);
	    printf("nonalphanumeric user name: \n\t%s\n", $0)
         }
#         if ($2 == "") {
#	    printf("\nWarning!  Password file, line %d,", NR);
#	    printf("no password: \n\t%s\n", $0)
#         }
#         if ("${C2}" == "true" && $2 ~ /^##/ && "##"$1 != $2) {
#	    printf("\nWarning!  Password file, line %d,", NR);
#	    printf("invalid password field for C2: \n\t%s\n", $0)
#         }
         if ($3 !~ /[0-9]/) {
	    printf("\nWarning!  Password file, line %d,", NR);
	    printf("nonnumeric user id: \n\t%s\n", $0)
         }
#        if ($3 == "0" && $1 != "root") {
#	    printf("\nWarning!  Password file, line %d,", NR);
#	    printf("user %s has uid = 0 and is not root\n\t%s\n", $1, $0)
#	 }
         if ($4 !~ /[0-9]/) {
		printf("\nWarning!  Password file, line %d,", NR);
		printf("nonnumeric group id: \n\t%s\n", $0)
	 }
         if ($6 !~ /^\//) {
		printf("\nWarning!  Password file, line %d,", NR);
		printf("invalid login directory: \n\t%s\n", $0)
	 }
      }
   }
}
