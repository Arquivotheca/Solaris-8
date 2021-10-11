#!/bin/sh
#
#  This file was produced by running the Policy_sh.SH script, which
#  gets its values from config.sh, which is generally produced by
#  running Configure.  The Policy.sh file gets overwritten each time
#  Configure is run.  Any variables you add to Policy.sh will be lost
#  unless you copy Policy.sh somewhere else before running Configure.
#
#  The idea here is to distill in one place the common site-wide
#  "policy" answers (such as installation directories) that are
#  to be "sticky".  If you keep the file Policy.sh around in
#  the same directory as you are building Perl, then Configure will
#  (by default) load up the Policy.sh file just before the
#  platform-specific hints file.
#

#  Allow Configure command-line overrides; usually these won't be
#  needed, but something like -Dprefix=/test/location can be quite
#  useful for testing out new versions.

#Site-specific values:

case "$perladmin" in
'') perladmin='perl-bugs@sun.com' ;;
esac

# Installation prefix.  Allow a Configure -D override.  You
# may wish to reinstall perl under a different prefix, perhaps
# in order to test a different configuration.
case "$prefix" in
'') prefix='/usr' ;;
esac

# Installation directives.  Note that each one comes in three flavors.
# For example, we have privlib, privlibexp, and installprivlib.
# privlib is for private (to perl) library files.
# privlibexp is the same, except any '~' the user gave to Configure
#     is expanded to the user's home directory.  This is figured
#     out automatically by Configure, so you don't have to include it here.
# installprivlib is for systems (such as those running AFS) that
#     need to distinguish between the place where things
#     get installed and where they finally will reside.
# 
# In each case, if your previous value was the default, leave it commented
# out.  That way, if you override prefix, all of these will be
# automatically adjusted.
#
# WARNING:  Be especially careful about architecture-dependent and
# version-dependent names, particularly if you reuse this file for
# different versions of perl.

# bin='/usr/perl5/5.00503/bin'
# scriptdir='/usr/perl5/5.00503/bin'
# privlib='/usr/perl5/5.00503'
# archlib='/usr/perl5/5.00503/sun4-solaris'
# Preserving custom man1dir
man1dir='/usr/perl5/5.00503/man/man1'
# man3dir='/usr/perl5/5.00503/man/man3'
# sitelib='/usr/perl5/site_perl/5.005'
# sitearch='/usr/perl5/site_perl/5.005/sun4-solaris'
# installbin='/usr/perl5/5.00503/bin'
# installscript='/usr/perl5/5.00503/bin'
# installprivlib='/usr/perl5/5.00503'
# installarchlib='/usr/perl5/5.00503/sun4-solaris'
# installman1dir='/usr/perl5/5.00503/man/man1'
# installman3dir='/usr/perl5/5.00503/man/man3'
# installsitelib='/usr/perl5/site_perl/5.005'
# installsitearch='/usr/perl5/site_perl/5.005/sun4-solaris'
# man1ext='1'
# man3ext='3'

#  Lastly, you may add additional items here.  For example, to set the
#  pager to your local favorite value, uncomment the following line in
#  the original Policy_sh.SH file and re-run   sh Policy_sh.SH.
#
#  pager='/usr/bin/more'
#
#  A full Glossary of all the config.sh variables is in the file
#  Porting/Glossary.

