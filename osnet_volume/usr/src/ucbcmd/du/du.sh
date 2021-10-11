#!/usr/bin/sh
#
# ident	"@(#)du.sh	1.3	93/04/21 SMI"
#
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#
# Replace /usr/ucb/du
#

exec /usr/bin/du -kr "$@"
