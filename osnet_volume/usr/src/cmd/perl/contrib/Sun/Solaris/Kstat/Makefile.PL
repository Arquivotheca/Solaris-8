#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.PL	1.2	99/11/24 SMI"
#
# cmd/perl/contrib/Sun/Solaris/Kstat/Makefile.PL
#

use ExtUtils::MakeMaker;

my $def = "";
#$def .= "-DNDEBUG ";		# Uncomment this to turn off assertions
#$def .= "-DREPORT_UNKNOWN ";	# Uncomment this to report unknown binary kstats
my $inc = "";

# Additional required include directories

if ($ENV{'PERL_PLAT'} =~ /sun4/) {
	#
	# Because there are still platform specific raw kstats, we need to
	# consume the platform specific header files.  Since we may build the
	# module on a different architecture than those we run it on, we need to
	# include all available platforms with defined raw kstats for this ISA.
	# Presently, this set consists of only sun4u.
	#
	$inc = "-I$ENV{'ROOT'}/usr/platform/sun4u/include";
} else {
	$inc = "-I$ENV{'ROOT'}/usr/platform/$ENV{'PERL_PLAT'}/include";
}

WriteMakefile(
    NAME         => 'Sun::Solaris::Kstat',
    VERSION_FROM => 'Kstat.pm',
    LIBS         => [ '-lkstat' ],
    INC          => $inc,
    OBJECT       => '$(O_FILES)',
    DEFINE       => $def,
);
