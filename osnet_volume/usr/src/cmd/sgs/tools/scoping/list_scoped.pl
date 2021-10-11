#!/usr/dist/exe/perl


for ($i = 0; $i <= $#ARGV; $i++) {
	$file = $ARGV[i];
	$type = `file $file`;
	if ($type =~ /\, stripped/o) {
		print("$file is stripped, unable to list scoped symbols\n");
		next;
	}

	if ($#ARGV > 0) {
		print("scoped symbols from $file\n");
	}
	open(syms_in, "dump -sv -n .symtab $file |");
	$basename = $file;
	$basename =~ s/.*\///o;
	$gather_scoped = 0;
	while (<syms_in>) {
		chop($_);
		($index, $addr, $size, $type, $bind, $other, $shndx, $name) =
			split(/[ \t]+/o, $_, 8);
		if (!($index =~ /\[[0-9]+\]/o)) {
			next;
		}
		if ($type eq "FILE") {
			if ($name eq $basename) {
				$gather_scoped = "1";
			} else {
				$gather_scoped = "0";
			}
			next;
		}
		if (($bind eq LOCL) && ($type ne "SECT") &&
		    ($gather_scoped == "1")) {
			print("$name\n");
		}
	}
}
