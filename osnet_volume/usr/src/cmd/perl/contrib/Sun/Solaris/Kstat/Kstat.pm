#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Kstat.pm	1.1	99/08/16 SMI"
#
# Kstat.pm provides the bootstrap for the Sun::Solaris::Kstat module.
#

package Sun::Solaris::Kstat;
use strict;
use DynaLoader;
use vars qw($VERSION @ISA);
$VERSION = '1.00';
@ISA = qw(DynaLoader);
bootstrap Sun::Solaris::Kstat $VERSION;
1;
