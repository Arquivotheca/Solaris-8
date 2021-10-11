#!/usr/dist/exe/perl

sub list_scoped {
	local($library) = @_;
	local($basename, $burn, $addr, $size, $type, $bind, $other,
	    $shndx, $name, $cpp_name, $gather_scoped);
	local(@syms);
	local($cnt);

	$basename = $library;
	$basename =~ s|.*/||o;
	open(dump_out, "dump -Cpv $library |");

	$cnt = 0;
	$gather_scoped = 0;
	while (<dump_out>) {
		chop($_);
		($burn, $addr, $size, $type, $bind, $other, $shndx,
		    $name, $cpp_name) = split(/[ 	]+/, $_, 9);
		if ($type eq "FILE") {
			if ($name eq $basename) {
				$gather_scoped = "1";
			} else {
				$gather_scoped = "0";
			}
			next;
		}
		if (($bind eq "LOCL") && ($type ne "SECT") &&
		    ($gather_scoped == "1")) {
			if ($cpp_name ne "") {
				$name = "$name $cpp_name";
			}
			$syms[$cnt] = $name;
			$cnt++;
		}
	}
	close(dump_out);
	@syms = sort @syms;

	open(scoped_out, ">$basename.scoped_syms");
	for ($cnt = 0; $cnt < $#syms; $cnt++) {
		print(scoped_out "$syms[$cnt]\n");
	}
	close(scoped_out);
}



if ($#ARGV != 0) {
	print("usage: find_scoped <base_dir>\n");
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
	$library = $_;
	$file_type = `file $library`;
	if ($file_type =~ /ELF.*dynamic lib/) {
		$versions = `pvs -d $library`;
		$file = $library;
		$file =~ s|^$ROOT||;
		if ($versions eq "") {
			print("UNVERSIONED: $file\n");
		} else {
			print("VERSIONED: $file\n");
			&list_scoped($library);
		}
	}
}

close(<file_list>);

