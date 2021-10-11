#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.PL	1.1	99/08/16 SMI"
#
# cmd/perl/contrib/Sun/Solaris/Utils/Makefile.PL
#

use ExtUtils::MakeMaker;

WriteMakefile(
   NAME         => 'Sun::Solaris::Utils',
   VERSION_FROM => 'Utils.pm',
   LIBS         => [ '-lgen', '-lintl' ],
   OBJECT       => '$(O_FILES)',
);
