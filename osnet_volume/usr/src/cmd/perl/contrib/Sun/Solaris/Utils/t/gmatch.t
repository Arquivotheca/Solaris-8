#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)gmatch.t	1.1	99/08/16 SMI"
#
# test script for Sun::Solaris::Utils gmatch()
#

use strict;

BEGIN { $| = 1; print "1..49\n"; }
my $loaded;
END {print "not ok 1\n" unless $loaded;}
use Sun::Solaris::Utils qw(gmatch);
$loaded = 1;
print "ok 1\n";

my ($test);
$test = 2;

my @strs = ( 'a', 'aa', 'z', 'zz', '0', '0123456789' );
my @tests = (
    { pattern => 'a',       results => [ 1, 0, 0, 0, 0, 0 ] }, 
    { pattern => '*',       results => [ 1, 1, 1, 1, 1, 1 ] }, 
    { pattern => '?',       results => [ 1, 0, 1, 0, 1, 0 ] }, 
    { pattern => '??',      results => [ 0, 1, 0, 1, 0, 0 ] }, 
    { pattern => '[a-z]*',  results => [ 1, 1, 1, 1, 0, 0 ] }, 
    { pattern => '[!a-z]*', results => [ 0, 0, 0, 0, 1, 1 ] }, 
    { pattern => '[0-9]*',  results => [ 0, 0, 0, 0, 1, 1 ] }, 
    { pattern => '[!0-9]*', results => [ 1, 1, 1, 1, 0, 0 ] }, 
);

foreach my $t (@tests) {
	for (my $i = 0; $i < @strs; $i++) {
		if (gmatch($strs[$i], $t->{pattern}) == $t->{results}[$i]) {
			print("ok $test\n");
		} else {
			print("not ok $test\n");
		}
		$test++;
	}
}

exit(0);
