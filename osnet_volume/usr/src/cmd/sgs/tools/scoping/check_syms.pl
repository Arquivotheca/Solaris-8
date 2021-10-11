#!/usr/dist/exe/perl

if ($#ARGV != 0) {
	print("usage: check_syms.pl <executable>\n");
	exit(1);
}

$object = $ARGV[0];

open(deps, "ldd $object |");

$objects[0] = $object;
$cnt = 1;
while (<deps>) {
	chop($_);
	$dep = $_;
	if ( /\=\>/ ) {
		$dep =~ s/.*\=\>[ \t]*//o;
		$objects[$cnt++] = $dep;
	}
}
close(deps);

$sym_file = "/tmp/syms.$$";

open(sym_out,">$sym_file");
for ($i = 0; $i <= $#objects; $i++) {
	$type = `file $objects[$i]`;
	if ( $type =~ /\, stripped/o ) {
		$DUMP = "dump -sv -n .dynsym"
	} else {
		$DUMP = "dump -sv -n .symtab"
	}
	open(syms_in, "$DUMP $objects[$i] |");
	$basename = $objects[$i];
	$basename =~ s/.*\///o;
	$gather_scoped = 0;
	while (<syms_in>) {
		chop($_);
		($index, $addr, $size, $type, $bind, $other, $shndx, $name) =
			split(/[ \t]+/o, $_, 8);
		if ( !($index =~ /\[[0-9]+\]/o) ) {
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
		if ((($bind eq "LOCL") && ($type ne "SECT") &&
		    ($gather_scoped == "1")) || (($bind ne "LOCL") &&
		    ($shndx ne "UNDEF"))) {
			print(sym_out
			    "$name|$basename|$type|$bind|$shndx\n");
			if (!defined($sym_cache{$name})) {
				$sym_cache{$name} = $bind;
			}
		}

	};
	close(syms_in);
}
close(sym_out);

$objs = join(" ", @objects);
open(syms, "dump -rv $objs | nawk '/^0x/ {print \$2}' | sort -u |");


while (<syms>) {
	chop ($_);
	$sym = $_;
	if ($sym_cache{$sym} ne "LOCL") {
		next;
	}
	open(grep_list, "grep '^$sym|' $sym_file |");
	print("Suspicios symbol: $sym\n");
	while (<grep_list>) {
		print("\t$_");
	}
	close (<grep_list>);
}
close(syms);

unlink($sym_file);
