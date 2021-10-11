#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Utils.pm	1.1	99/08/16 SMI"
#
# Utils.pm provides the bootstrap for the Sun::Solaris::Utils module.
#

package Sun::Solaris::Utils;
use strict;
use Exporter;
use DynaLoader;
use vars qw($VERSION @ISA @EXPORT_OK);
$VERSION = '1.00';
@ISA = qw(Exporter DynaLoader);
@EXPORT_OK = qw(gmatch gettext textdomain bindtextdomain dcgettext dgettext);
bootstrap Sun::Solaris::Utils $VERSION;
1;
