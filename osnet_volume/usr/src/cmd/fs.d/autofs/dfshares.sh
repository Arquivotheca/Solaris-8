#!/usr/bin/sh
#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)dfshares.sh 1.1     99/05/24 SMI"

# dfshares is a server utility but autofs is a client
# filesystem, so dfshares for autofs will do nothing.
# This utility is needed because autofs is included in
# /etc/dfs/fstypes.

exit 0
