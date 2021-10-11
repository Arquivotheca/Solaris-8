#!/bin/sh -- # This comment tells perl not to loop!
#
#
# Copyright (c) 1993-1999 by Sun Microsystems, Inc.
# All rights reserved.
# 
#
#ident	"@(#)hdrchk.sh	1.3	99/08/13 SMI"
#
eval 'exec perl -S $0 ${1+"$@"}'
if 0;
#
# hdrchk - check that header files conform to our standards
#
sub err {
	printf "%s: %s\n", $filename, $_[0];
}

$* = 1;		# matches can span lines

# following are the basic patterns that describe the format of header files

# comment at the end of a line
$eolcom = '[ \t]+/\*[ \t].*[ \t]\*/';
# comment at the beginning of the line, possibly a multi-line block comment
$comment = '(\n|/\*[ \t].*[ \t]\*/\n|/\*\n( \*([ \t].*)?\n)* \*/\n)';
$guard = '#ifndef[ \t](\w+)\n#define\t(\w+)\n\n';
# expanded and unexpanded #ident strings
# XXX - what if I put this script under SCCS control?!?  :-)
$identstr = '(\%Z\%(\%M\%)\t\%I\%|\%W\%)\t\%E\% SMI';
$xidentstr = '@\(#\)(\w[-\.\w]+\.h)\t\d+\.\d+(\.\d+\.\d+)?\t\d\d/\d\d/\d\d SMI';
$ident = "#pragma ident\t\"$identstr\"($eolcom)?\n";
$xident = "#pragma ident\t\"$xidentstr\"($eolcom)?\n";
$include =
    "(#include <.*>|using[ \t]std::.*|#if.*|#else|#elif.*|#endif)($eolcom)?\n";
$cplusplus = '\n#ifdef[ \t]__cplusplus\nextern "C" {\n#endif\n\n';
$cplusplusend = '#ifdef[ \t]__cplusplus\n}\n#endif\n';
$guardend = '\n#endif[ \t]/\* !?(\w+) \*/\n';

file:
while ($filename = $ARGV[0]) {
	shift;
	($sfilename = $filename) =~ s:.*/::;
	# in order to achor searches, put :'s at beginning and end of file
	# XXX - should use a more unique character
	if (!-r $filename) {
		&err("can't open");
		next file;
	}
	$_ = ':' . `cat $filename` . ':';

	# join continuation lines
	s/\\\n/ /g;

	# XXX - should check that first comment contains the word "Copyright"
	if (!s/:($comment)+\n/:/o || $` ne '') {
		&err("comment wrong");
	} elsif (!s/:$guard/:/o) {
		&err("guard wrong");
	} elsif (($tag = $1) ne $2 || $1 !~ /^_[A-Z0-9_]+$/) {
		&err("guard tag wrong");
	# XXX - also check that the guard tag is derived from the filename
	} elsif (!s/\n$guardend:/\n:/o || $1 ne $tag) {
		&err("end guard wrong");
	} elsif (!s/:$xident/:/o && !s/:$ident/:/o) {
		&err("#ident wrong");
	} elsif ($1 ne $sfilename && $1 !~ /^%.*%$/) {
		&err("#ident filename wrong");
	} else {
		# get rid of any includes and any comments before or after
		# the includes.  since includes can be empty, can't tell if
		# it's wrong yet.
		while (!s/:$cplusplus/:/o) {
			# didn't find the cplusplus, next line better
			# be a comment or an include
			if (!(s/:$comment/:/o || s/:$include/:/o)) {
				next file if ($_ eq '::');	# nothing left
				if (!s/$cplusplus//o) {
					# couldn't eliminate cplusplus so it
					# must not be there anywhere.
					&err("cplusplus wrong");
				} else {
					&err("#include wrong");
				}
				next file;
			}
		}
		# only get here if cplusplus was eliminated successfully

		# keep trying to eliminate cplusplusend, stripping off
		# trailing includes and comments
		while (!s/$cplusplusend:/:/o) {
			if (!(s/$include:/:/o || s/\n$comment:/\n:/o)) {
				if (s/$cplusplusend//o) {
					&err("garbage after end cplusplus");
				} else {
					&err("end cplusplus wrong");
				}
				next file;
			}
		}
	}
}
