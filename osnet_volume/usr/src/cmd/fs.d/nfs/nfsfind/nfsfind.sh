#!/bin/sh
# Copyright (c) 1993 by Sun Microsystems, Inc.

#ident  "@(#)nfsfind.sh 1.6     98/06/02 SMI"        /*      */
#
# Check shared NFS filesystems for .nfs* files that 
# are more than a week old.
#
# These files are created by NFS clients when an open file
# is removed. To preserve some semblance of Unix semantics
# the client renames the file to a unique name so that the
# file appears to have been removed from the directory, but
# is still usable by the process that has the file open.

if [ ! -s /etc/dfs/sharetab ]; then exit ; fi

# Get all NFS filesystems exported with read-write permission.

DIRS=`/usr/bin/nawk '($3 != "nfs") { next }
	($4 ~ /^rw$|^rw,|^rw=|,rw,|,rw=|,rw$/) { print $1; next }
	($4 !~ /^ro$|^ro,|^ro=|,ro,|,ro=|,ro$/) { print $1 }' /etc/dfs/sharetab`

for dir in $DIRS
do
        find $dir -type f -name .nfs\* -mtime +7 -mount -exec rm -f {} \;
done
