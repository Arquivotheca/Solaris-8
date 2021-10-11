#!/bin/sh -- # This comment tells perl not to loop!
#
eval 'exec perl -S $0 "$@"'
if 0;
#
# @(#)cstyle 1.58 98/09/09 (from shannon)
#ident	"@(#)cstyle.sh	1.1	99/01/11 SMI"
#
# Copyright (c) 1991-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# cstyle - check for some common stylistic errors.
#
#	cstyle is a sort of "lint" for C coding style.
#	It attempts to check for the style used in the
#	kernel, sometimes known as "Bill Joy Normal Form".
#
#	There's a lot this can't check for, like proper
#	indentation of continuation lines.  There's also
#	a lot more this could check for.
#
#	A note to the non perl literate:
#
#		perl regular expressions are pretty much like egrep
#		regular expressions, with the following special symbols
#
#		\s	any space character
#		\S	any non-space character
#		\w	any "word" character [a-zA-Z0-9_]
#		\W	any non-word character
#		\d	a digit [0-9]
#		\D	a non-digit
#		\b	word boundary (between \w and \W)
#		\B	non-word boundary
#
#require "getopts.pl";
# XXX - because some versions of perl can not find the lib directory,
# we just include this here.
;# getopts.pl - a better getopt.pl

;# Usage:
;#      do Getopts("a:bc");  # -a takes arg. -b & -c not. Sets opt_* as a
;#                           #  side effect.

sub Getopts {
    local($argumentative) = @_;
    local(@args,$_,$first,$rest);
    local($[) = 0;
    local($errs) = 0;

    @args = split( / */, $argumentative );
    while(($_ = $ARGV[0]) =~ /^-(.)(.*)/) {
	($first,$rest) = ($1,$2);
	$pos = index($argumentative,$first);
	if($pos >= $[) {
	    if($args[$pos+1] eq ":") {
		shift(@ARGV);
		if($rest eq "") {
		    $rest = shift(@ARGV);
		}
		eval "\$opt_$first = \$rest;";
	    }
	    else {
		eval "\$opt_$first = 1";
		if($rest eq "") {
		    shift(@ARGV);
		}
		else {
		    $ARGV[0] = "-$rest";
		}
	    }
	}
	else {
	    print STDERR "Unknown option: $first\n";
	    ++$errs;
	    if($rest ne "") {
		$ARGV[0] = "-$rest";
	    }
	    else {
		shift(@ARGV);
	    }
	}
    }
    $errs == 0;
}

1;
# end of getopts.pl

$usage =
"usage: cstyle [-c] [-h] [-p] [-v] [-C] [-P] file ...
	-c	check continuation line indenting
	-h	perform heuristic checks that are sometimes wrong
	-p	perform some of the more picky checks
	-v	verbose
	-C	don't check anything in header block comments
	-P	check for use of non-POSIX types
";

if (!&Getopts("chpvCP")) {
	print $usage;
	exit 1;
}

$check_continuation = $opt_c;
$heuristic = $opt_h;
$picky = $opt_p;
$verbose = $opt_v;
$ignore_hdr_comment = $opt_C;
$check_posix_types = $opt_P;

if ($verbose) {
	$fmt = "%s: %d: %s\n%s\n";
} else {
	$fmt = "%s: %d: %s\n";
}

# Note, following must be in single quotes so that \s and \w work right.
$typename = '(int|char|short|long|unsigned|float|double|\w+_t|struct\s+\w+|union\s+\w+|FILE)';

# mapping of old types to POSIX compatible types
%old2posix = (
	'unchar', 'uchar_t',
	'ushort', 'ushort_t',
	'uint', 'uint_t',
	'ulong', 'ulong_t',
	'u_int', 'uint_t',
	'u_short', 'ushort_t',
	'u_long', 'ulong_t',
	'u_char', 'uchar_t',
	'quad', 'quad_t'
);

$warlock_comment = "(
VARIABLES PROTECTED BY|
MEMBERS PROTECTED BY|
ALL MEMBERS PROTECTED BY|
READ-ONLY VARIABLES:|
READ-ONLY MEMBERS:|
VARIABLES READABLE WITHOUT LOCK:|
MEMBERS READABLE WITHOUT LOCK:|
LOCKS COVERED BY|
LOCK UNNEEDED BECAUSE|
LOCK NEEDED:|
LOCK HELD ON ENTRY:|
READ LOCK HELD ON ENTRY:|
WRITE LOCK HELD ON ENTRY:|
LOCK ACQUIRED AS SIDE EFFECT:|
READ LOCK ACQUIRED AS SIDE EFFECT:|
WRITE LOCK ACQUIRED AS SIDE EFFECT:|
LOCK RELEASED AS SIDE EFFECT:|
LOCK UPGRADED AS SIDE EFFECT:|
LOCK DOWNGRADED AS SIDE EFFECT:|
FUNCTIONS CALLED THROUGH POINTER|
FUNCTIONS CALLED THROUGH MEMBER|
LOCK ORDER:
)";
$warlock_comment =~ tr/\n//d;

if ($#ARGV >= 0) {
	foreach $arg (@ARGV) {
		if (!open(STDIN, $arg)) {
			printf "%s: can not open\n", $arg;
		} else {
			&cstyle($arg);
			close STDIN;
		}
	}
} else {
	&cstyle("<stdin>");
}

sub err {
	printf $fmt, $filename, $., $_[0], $line;
}

sub cstyle {

$in_comment = 0;
$in_header_comment = 0;
$in_warlock_comment = 0;
$in_continuation = 0;
$in_function = 0;
$in_function_header = 0;
$in_initialization = 0;
$in_declaration = 0;
$note_level = 0;
$nextok = 0;
$nocheck = 0;
$expect_continuation = 0;
$prev = '';

$filename = $_[0];

line: while (<STDIN>) {
	s/\r?\n$//;	# strip return and newline

	# save the original line, then remove all text from within
	# double or single quotes, we do not want to check such text.

	$line = $_;
	s/"[^"]*"/\"\"/g;
	s/'.'/''/g;

	# strip off trailing backslashes, which appear in long macros
	s/\s*\\$//;

	# an /* END CSTYLED */ comment ends a no-check block.
	if ($nocheck) {
		if (/\/\* *END *CSTYLED *\*\//) {
			$nocheck = 0;
		} else {
			next line;
		}
	}

	# a /*CSTYLED*/ comment indicates that the next line is ok.
	if ($nextok) {
		if ($okmsg) {
			do err($okmsg);
		}
		$nextok = 0;
		$okmsg = 0;
		if (/\/\* *CSTYLED.*\*\//) {
			/^.*\/\* *CSTYLED *(.*) *\*\/.*$/;
			$okmsg = $1;
			$nextok = 1;
		}
		$prev = $line;
		next line;
	}

	# check length of line.
	# first, a quick check to see if there is any chance of being too long.
	if ($line =~ tr/\t/\t/ * 7 + length($line) > 80) {
		# yes, there is a chance.
		# replace tabs with spaces and check again.
		$eline = $line;
		1 while $eline =~
		    s/\t+/' ' x (length($&) * 8 - length($`) % 8)/e;
		if (length($eline) > 80) {
			do err("line > 80 characters");
		}
	}
#	this is the fastest way to check line length,
#	but it doesnt work with perl 3.0.
#	if ($line =~ tr/\t/\t/ * 7 + length($line) > 80) {
#		$pos = $oldp = $p = 0;
#		while (($p = index($line, "\t", $p)) >= 0) {
#			$pos = ($pos + $p - $oldp + 8) & ~7;
#			$oldp = ++$p;
#		}
#		$pos += length($line) - $oldp;
#		if ($pos > 80) {
#			do err("line > 80 characters");
#		}
#	}

	# remember whether we expect to be inside a continuation line.
	$in_continuation = $expect_continuation;

	# check for proper continuation line.  blank lines
	# and C preprocessor lines in the middle of the
	# continuation do not count.
	# XXX - only check within functions.
	if ($check_continuation && $expect_continuation && $in_function &&
	    !/^#/ && !/^\s*$/) {
		if ($in_initialization) {
			if (!/^$continuation_indent\S/) {
				do err("continuation line improperly indented");
			}
		} else {
			# continuation line must start with whitespace of
			# previous line, plus either 4 spaces or a tab, but
			# do not check lines that start with a string constant
			# since they are often shifted to the left to make them
			# fit on the line.
			if (!/^$continuation_indent    \S/ &&
			    !/^$continuation_indent\t\S/ && !/^\s*"/) {
				do err("continuation line improperly indented");
			}
		}
		$expect_continuation = 0;
	}

	# ignore NOTE(...) annotations (assumes NOTE is on lines by itself).
	if ($note_level || /\b_?NOTE\s*\(/) { # if in NOTE or this is NOTE
		s/[^()]//g;			  # eliminate all non-parens
		$note_level += s/\(//g - length;  # update paren nest level
		next;
	}
		
	# a /* BEGIN CSTYLED */ comment starts a no-check block.
	if (/\/\* *BEGIN *CSTYLED *\*\//) {
		$nocheck = 1;
	}

	# a /*CSTYLED*/ comment indicates that the next line is ok.
	if (/\/\* *CSTYLED.*\*\//) {
		/^.*\/\* *CSTYLED *(.*) *\*\/.*$/;
		$okmsg = $1;
		$nextok = 1;
	}
	if (/\/\/ *CSTYLED/) {
		/^.*\/\/ *CSTYLED *(.*)$/;
		$okmsg = $1;
		$nextok = 1;
	}

	# is this the beginning or ending of a function?
	# (not if "struct foo\n{\n")
	if (/^{$/ && $prev =~ /\)/) {
		$in_function = 1;
		$in_declaration = 1;
		$in_initialization = 0;
		$in_function_header = 0;
		$expect_continuation = 0;
		$in_continuation = 0;
		$prev = $line;
		next line;
	}
	if (/^}\s*(\/\*.*\*\/\s*)*$/) {
		if ($prev =~ /^\s*return\s*;/) {
			$str = "unneeded return at end of function";
			printf $fmt, $filename, $. - 1, $str, $prev;
		}
		$in_function = 0;
		$prev = $line;
		next line;
	}
	if (/^\w*\($/) {
		$in_function_header = 1;
	}

	# is this the beginning or ending of an initialization?
	if (/=\s*{$/) {
		$in_initialization = 1;
	}
	if (/^\s*};$/) {
		$in_initialization = 0;
		$prev = $line;
		next line;
	}

	if ($in_warlock_comment && /\*\//) {
		$in_warlock_comment = 0;
		$prev = $line;
		next line;
	}

	# a blank line terminates the declarations within a function.
	# XXX - but still a problem in sub-blocks.
	if ($in_declaration && /^$/) {
		$in_declaration = 0;
	}

	# does this looks like the start of a block comment?
	if (/^\s*\/\*$/) {
		if (!/^\t*\/\*$/) {
			do err("block comment not indented by tabs");
		}
		$in_comment = 1;
		s/\/\*/ /;
		$comment_prefix = $_;
		if ($comment_prefix eq " ") {
			$in_header_comment = 1;
		}
		$prev = $line;
		next line;
	}
	# are we still in the block comment?
	if ($in_comment && !/^$comment_prefix\*/) {
		# assume out of comment
		$in_comment = 0;
		$in_header_comment = 0;
	}

	if ($in_header_comment && $ignore_hdr_comment) {
		$prev = $line;
		next line;
	}

	# check for errors that might occur in comments and in code.

	# allow spaces to be used to draw pictures in header comments.
	if (/[^ ]     / && !/".*     .*"/ && !$in_header_comment) {
		do err("spaces instead of tabs");
	}
	if (/^ / && !/^ \*[ \t\/]/ && !/^ \*$/ &&
	    (!/^    \w/ || $in_function != 0)) {
		do err("indent by spaces instead of tabs");
	}
	if (/\s$/) {
		do err("space or tab at end of line");
	}
	if (/^[\t]+ [^ \t\*]/ || /^[\t]+  \S/ || /^[\t]+   \S/) {
		do err("continuation line not indented by 4 spaces");
	}
	if (/[^ \t(]\/\*/ && !/\w\(\/\*.*\*\/\);/) {
		do err("comment preceded by non-blank");
	}
	if (/\t[ ]+\t/) {
		do err("spaces between tabs");
	}
	if (/ [\t]+ /) {
		do err("tabs between spaces");
	}
	if (/\/\*\s*$warlock_comment/o && !/\*\//) {
		$in_warlock_comment = 1;
		$prev = $line;
		next line;
	}
	if (/^\s*\/\*./ && !/^\s*\/\*.*\*\//) {
		do err("improper first line of block comment");
	}

	if ($in_comment) {	# still in comment
		$prev = $line;
		next line;
	}

	if ((/[^(]\/\*\S/ || /^\/\*\S/) &&
	    !(/\/\*(ARGSUSED[0-9]*|NOTREACHED|LINTLIBRARY|VARARGS[0-9]*)\*\// ||
	    /\/\*(CONSTCOND|CONSTANTCOND|CONSTANTCONDITION|EMPTY)\*\// ||
	    /\/\*(FALLTHRU|FALLTHROUGH|LINTED.*|PRINTFLIKE[0-9]*)\*\// ||
	    /\/\*(PROTOLIB[0-9]*|SCANFLIKE[0-9]*|CSTYLED.*)\*\//)) {
		do err("missing blank after open comment");
	}
	if (/\S\*\/[^)]|\S\*\/$/ &&
	    !(/\/\*(ARGSUSED[0-9]*|NOTREACHED|LINTLIBRARY|VARARGS[0-9]*)\*\// ||
	    /\/\*(CONSTCOND|CONSTANTCOND|CONSTANTCONDITION|EMPTY)\*\// ||
	    /\/\*(FALLTHRU|FALLTHROUGH|LINTED.*|PRINTFLIKE[0-9]*)\*\// ||
	    /\/\*(PROTOLIB[0-9]*|SCANFLIKE[0-9]*|CSTYLED.*)\*\//)) {
		do err("missing blank before close comment");
	}
	if (/\/\/\S/) {		# C++ comments
		do err("missing blank after start comment");
	}
	# check for unterminated single line comments, but allow them when
	# they are used to comment out the argument list of a function
	# declaration.
	if (/\S.*\/\*/ && !/\S.*\/\*.*\*\// && !/\(\/\*/) {
		do err("unterminated single line comment");
	}

	if (/^#(else|endif|include)/) {
		$prev = $line;
		next line;
	}

	# delete any comments and check everything else.
	s/\/\*.*\*\///g;
	s/\/\/.*$//;		# C++ comments

	# delete any trailing whitespace; we have already checked for that.
	s/\s*$//;

	# following checks do not apply to text in comments.

	# if it looks like an operator at the end of the line, and it is
	# not really the end of a comment (...*/), and it is not really
	# a label (done:), and it is not a case label (case FOO:),
	# or we are not in a function definition (ANSI C style) and the
	# operator is a "," (to avoid hitting "int\nfoo(\n\tint i,\n\tint j)"),
	# or we are in a function and the operator is a
	# "*" (to avoid hitting on "char*\nfunc()").
	if ((/[-+|&\/?:=]$/ && !/\*\/$/ && !/^\s*\w*:$/ &&
	    !/^\s\s*case\s\s*\w*:$/) ||
	    (!$in_function_header && /,$/) ||
	    ($in_function && /\*$/)) {
		$expect_continuation = 1;
		if (!$in_continuation) {
			/^(\s*)\S/;
			$continuation_indent = $1;
		}
	}
	if (/[^<>\s][!<>=]=/ || /[^<>][!<>=]=\S/ ||
	    (/[^->]>[^=>\s]/ && !/[^->]>$/) || (/[^<]<[^=<\s]/ && !/[^<]<$/) ||
	    /[^<\s]<[^<]/ || /[^->\s]>[^>]/) {
		do err("missing space around relational operator");
	}
	if (/\S>>=/ || /\S<<=/ || />>=\S/ || /<<=\S/ || /\S[-+*\/&|^%]=/ ||
	    (/[^-+*\/&|^%!<>=\s]=[^=]/ && !/[^-+*\/&|^%!<>=\s]=$/) ||
	    (/[^!<>=]=[^=\s]/ && !/[^!<>=]=$/)) {
		# XXX - should only check this for C++ code
		# XXX - there are probably other forms that should be allowed
		if (!/\soperator=/) {
			do err("missing space around assignment operator");
		}
	}
	if (/[,;]\S/ && !/\bfor \(;;\)/) {
		do err("comma or semicolon followed by non-blank");
	}
	# allow "for" statements to have empty "while" clauses
	if (/\s[,;]/ && !/^[\t]+;$/ && !/^\s*for \([^;]*; ;[^;]*\)/) {
		do err("comma or semicolon preceded by blank");
	}
	if (/^\s*(&&|\|\|)/) {
		do err("improper boolean continuation");
	}
	if (/\S   *(&&|\|\|)/ || /(&&|\|\|)   *\S/) {
		do err("more than one space around boolean operator");
	}
	if (/\b(for|if|while|switch|sizeof|return|case)\(/) {
		do err("missing space between keyword and paren");
	}
	if (/(\b(for|if|while|switch|return)\b.*){2,}/ && !/^#define/) {
		# multiple "case" and "sizeof" allowed
		do err("more than one keyword on line");
	}
	if (/\b(for|if|while|switch|sizeof|return|case)\s\s+\(/ &&
	    !/^#if\s+\(/) {
		do err("extra space between keyword and paren");
	}
	# try to detect "func (x)" but not "if (x)" or
	# "#define foo (x)" or "int (*func)();"
	if (/\w\s\(/) {
		$s = $_;
		# strip off all keywords on the line
		s/\b(for|if|while|switch|return|case|sizeof)\s\(/XXX(/g;
		s/#elif\s\(/XXX(/g;
		s/^#define\s+\w+\s+\(/XXX(/;
		# do not match things like "void (*f)();"
		# or "typedef void (func_t)();"
		s/\w\s\(+\*/XXX(*/g;
		s/\b($typename|void)\s+\(+/XXX(/og;
		if (/\w\s\(/) {
			do err("extra space between function name and left paren");
		}
		$_ = $s;
	}
	# try to detect "int foo(x)", but not "extern int foo(x);"
	# XXX - this still trips over too many legitimate things,
	# like "int foo(x,\n\ty);"
#		if (/^(\w+(\s|\*)+)+\w+\(/ && !/\)[;,](\s|)*$/ &&
#		    !/^(extern|static)\b/) {
#			do err("return type of function not on separate line");
#		}
	# this is a close approximation
	if (/^(\w+(\s|\*)+)+\w+\(.*\)(\s|)*$/ &&
	    !/^(extern|static)\b/) {
		do err("return type of function not on separate line");
	}
	if (/^#define /) {
		do err("#define followed by space instead of tab");
	}
	if (/^\s*return\W[^;]*;/ && !/^\s*return\s*\(.*\);/) {
		do err("unparenthesized return expression");
	}
	if (/\bsizeof\b/ && !/\bsizeof\s*\(.*\)/) {
		do err("unparenthesized sizeof expression");
	}
	if (/\(\s/) {
		do err("whitespace after left paren");
	}
	# allow "for" statements to have empty "continue" clauses
	if (/\s\)/ && !/^\s*for \([^;]*;[^;]*; \)/) {
		do err("whitespace before right paren");
	}
	if (/^\s*\(void\)[^ ]/) {
		do err("missing space after (void) cast");
	}
	if (/\S{/ && !/{{/) {
		do err("missing space before left brace");
	}
	if ($in_function && /^\s+{/ &&
	    ($prev =~ /\)\s*$/ || $prev =~ /\bstruct\s+\w+$/)) {
		do err("left brace starting a line");
	}
	if (/}(else|while)/) {
		do err("missing space after right brace");
	}
	if (/}\s\s+(else|while)/) {
		do err("extra space after right brace");
	}
	if (/\b_VOID\b|\bVOID\b|\bSTATIC\b/) {
		do err("obsolete use of VOID or STATIC");
	}
	if (/\b$typename\*/o) {
		do err("missing space between type name and *");
	}
	if (/^\s+#/) {
		do err("preprocessor statement not in column 1");
	}
	if (/^#\s/) {
		do err("blank after preprocessor #");
	}
	if (/!\s*(strcmp|strncmp|bcmp)\s*\(/) {
		do err("don't use boolean ! with comparison functions");
	}
	if ($picky) {
		# try to detect spaces after casts, but allow (e.g.)
		# "sizeof (int) + 1" and "void (*funcptr)(int) = foo;"
		if (/\($typename( \*+)?\)\s/o &&
		    !/sizeof\s*\($typename( \*)?\)\s/o &&
		    !/\($typename( \*+)?\)\s+=[^=]/o) {
			do err("space after cast");
		}
		if (/\b$typename\s*\*\s/o &&
		    !/\b$typename\s*\*\s+const\b/o) {
			do err("unary * followed by space");
		}
	}
	if ($check_posix_types) {
		# try to detect old non-POSIX types.
		# POSIX requires all non-standard typedefs to end in _t,
		# but historically these have been used.
		if (/\b(unchar|ushort|uint|ulong|u_int|u_short|u_long|u_char|quad)\b/) {
			do err("non-POSIX typedef $1 used: use $old2posix{$1} instead");
		}
	}
	if ($heuristic) {
		# cannot check this everywhere due to "struct {\n...\n} foo;"
		if ($in_function && !$in_declaration &&
		    /}./ && !/}\s+=/ && !/{.*}[;,]$/ && !/}(\s|)*$/ &&
		    !/} (else|while)/ && !/}}/) {
			do err("possible bad text following right brace");
		}
		# cannot check this because sub-blocks in
		# the middle of code are ok
		if ($in_function && /^\s+{/) {
			do err("possible left brace starting a line");
		}
	}
	if (/^\s*else\W/) {
		if ($prev =~ /^\s*}$/) {
			$str = "else and right brace should be on same line";
			printf $fmt, $filename, $., $str, $prev;
			if ($verbose) {
				printf "%s\n", $line;
			}
		}
	}
	$prev = $line;
}

if ($prev eq "") {
	do err("last line in file is blank");
}

}
