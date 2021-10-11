#!/usr/dist/exe/perl

if ($#ARGV != 0) {
	print("usage: find_libs <base_dir>\n");
	exit(1);
}

if (!defined($ENV{'ROOT'}) || $ENV{'ROOT'} eq "") {
	print("ROOT not set, assuming /\n");
	$ROOT="/";
} else {
	$ROOT=$ENV{'ROOT'};
}

$base_dir = $ARGV[0];

open(file_list, "find $base_dir -type f -print |");

while (<file_list>) {
	chop($_);
	$file = $_;
	$file_type = `file $file`;
	if ($file_type =~ /ELF.*dynamic lib/) {
		$versions = `pvs -d $file`;
		$file =~ s|^$ROOT||;
		if ($versions eq "") {
			print("UNVERSIONED: $file\n");
		} else {
			print("VERSIONED: $file\n");
		}
	}
}

close(<file_list>);

