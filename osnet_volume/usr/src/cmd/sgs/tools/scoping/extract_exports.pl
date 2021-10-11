#!/usr/dist/exe/perl

$tmp_file="/tmp/exports.$$";

if ( $#ARGV != 0) {
	print("usage: extract_exports <binding_file>\n");
	exit(1);
}


$bindings_file=$ARGV[0];

open(bindings,"$bindings_file");
open(tmp_file, ">$tmp_file");

while (<bindings>) {
	chop;
	($source, $sym, $consumer, $obj) = split(/\|/, $_, 4);
	if ($source ne $consumer) {
		print(tmp_file "$source|$sym\n");
	}
}

close(bindings);
close(tmp_file);

system("sort -u $tmp_file");
unlink($tmp_file);
