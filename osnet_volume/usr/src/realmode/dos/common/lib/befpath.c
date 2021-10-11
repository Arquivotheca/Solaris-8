/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Solaris x86 realmode device driver locator:
 *
 *    This file contains a routine that may be used to search Solaris x86
 *    realmode driver directories for a specific driver.  Searching direc-
 *    tories rather than hard coding driver path names allows us to alter
 *    ISA probe sequences by simply moving drivers from one directory to
 *    another.    
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)befpath.c	1.5	95/04/15 SMI\n"
#include <sys/types.h>
#include <sys/stat.h>
#include <befext.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>

char *GetBefPath (char _far *file)
{	/*
	 *  Find driver path:
	 *
	 *  This routine will search the various realmode driver directories
	 *  looking for the driver with the specified "file"name.  When (and if)
	 *  we find it, we return a pointer to the complete path name of that
	 *  driver.  Returns null if the driver doesn't exist!
	 */
  
	DIR *dp;
	char *cp = 0;
	struct stat st;
	struct dirent *dep;
	static char pathname[256];

	char *fp = _fstrrchr(file, '\\'); // Set ptr to file name portion of the
	if (!fp++) fp = file;             // .. input path name.

	if (dp = opendir(DRIVERDIR)) {
		/*
		 *  Got the drivers directory open, now let's search all subdirectories
		 *  for the one containing the desired driver.
		 */

		while (!cp && (dep = readdir(dp))) {
			/*
			 *  Format a complete pathname from the base driver directory
			 *  name, the current component, and the target file.  If we
			 *  can successfully "stat" this file, we've found our driver!
			 */

			char *xp = dep->d_name - 1;
			while (*++xp) *xp = tolower(*xp);
			sprintf(pathname, "%s\\%s\\%s", DRIVERDIR, dep->d_name, fp);
			if (!stat(pathname, &st)) cp = pathname;
		}

		closedir(dp);
	}

	return(cp);
}
