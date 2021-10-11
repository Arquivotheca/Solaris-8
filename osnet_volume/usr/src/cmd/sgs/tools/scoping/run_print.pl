#!/usr/dist/exe/perl

$|=1;

$PRINT_GLOBS="perl /home/msw/data/print_globs.pl";

if ($#ARGV < 2) {
	print("usage: run_print <man_dir> <sym_export_list> <base_dir> ...\n");
	exit(1);
}

if (!defined($ENV{'ROOT'}) || $ENV{'ROOT'} eq "") {
	print("ROOT not set, assuming /\n");
	$ROOT="/";
} else {
	$ROOT=$ENV{'ROOT'};
}

$lib_list = "lib_list";

open(lib_list, ">$lib_list");
$anchor=`pwd`;

$man_dir = $ARGV[0];
$sym_export_list = $ARGV[1];

for ($i = 2; $i <= $#ARGV; $i++) {
	$base_dir = $ARGV[$i];

	open(file_list, "find $base_dir -type f -print |");

	while (<file_list>) {
		chop($_);
		$file = $_;
		$file_type = `file $file`;
		if ($file_type =~ /ELF.*dynamic lib/) {
			$dir = $file;
			$dir =~ s|/[^/]*$||o;
			$versions = `pvs -d $file`;
			$file =~ s|^$ROOT||;
			if ($versions eq "") {
				print(lib_list "UNVERSIONED: $file\n");
				print("UNVERSIONED: $file\n");
			} else {
				print(lib_list "VERSIONED: $file\n");
				print("VERSIONED: $file\n");
			}
			$file =~  s|.*/||o;
			chdir($dir);
			system("$PRINT_GLOBS $man_dir $sym_export_list $file");
			chdir($anchor);
		}
	}

	close(file_list);
}

close(lib_list);
