#!/usr/dist/exe/perl

$|=1;


sub check_file {
	local($file) = @_;
	if ( ! -f $file ) {
		print("print_globs: file $file not found!\n");
		system("pwd");
		exit(1);
	}
}

sub move {
	local($old, $new) = @_;
	unlink($new);
	link($old, $new);
	unlink($old);
}

#
# build list of interfaces from Manpage directories
#
# follows the convention that each interface should have
# a unique interface in the man page directory
#
sub build_manint {
	local($mandir) = @_;
	local($pwd, $mdir, $entry, $set);
	$pwd = `pwd`;
	chop($pwd);
	if ( ! -d $mandir) {
		print("Manual directory not found: $mandir\n");
		exit(1);
	}
	chdir($mandir);
	while (<man2* man3*>) {
		$mdir = $_;
		if ( -d $mdir) {
			chdir($mdir);
			while (<*>) {
				($entry, $set) = split(/\./o, $_, 2);
				$mansymbols{$entry} = 1;
			}
			chdir("../");
		}
	}
	chdir($pwd);
}


sub record_lib_exports {
	local($master_lib, $exports) = @_;
	local($lib, $sym);

	&check_file($exports);
	open(exports,"<$exports");
	while (<exports>) {
		chop($_);
		($lib, $sym) = split(/\|/o, $_, 2);
		if ( "$lib" =~ m/$master_lib$/o) {
			$exported{$sym} = "1";
		}
	}
	close(exports);
}

if ($#ARGV != 2) {
	printf("usage: print_globs <man_dir> <sym_export_list> <shared_object>\n");
	exit(1);
}

$mandir = $ARGV[0];
$exports = $ARGV[1];
$lib = $ARGV[2];

$PUBLIC = "public_syms";
$SCOPED = "scoped_syms";
$PRIVATE_SYMS = "private_syms";
$SYMS = "syms";
$HEADER_CACHE = "/tmp/headers.$$";
$CPP_FILE = "cplusplus_syms";
$ERR_FILE = "errors_syms";


@SPEC_SYMS =	(
		"_DYNAMIC",
		"_end",
		"_etext",
		"_edata",
		"_init",
		"_fini",
		"_PROCEDURE_LINKAGE_TABLE_",
		"_GLOBAL_OFFSET_TABLE_",
		"_lib_version"
		);


if ( !defined($ENV{'ROOT'}) || $ENV{'ROOT'} eq "") {
	print("ROOT not set, assuming /\n");
	$ROOT = "/";
} else {
	$ROOT = $ENV{'ROOT'};
}

#
# Build spec_sym table
#
foreach $sym (@SPEC_SYMS) {
	$special_sym{$sym} = $sym;
}


$lib_name = $lib;
$lib_name =~ s|.*/||;


&build_manint($mandir);

&record_lib_exports($lib_name, $exports);

&check_file($lib);

open(dump_out, "dump -Cpv $lib |");
open(cpp_out, ">$CPP_FILE");
open(sym_out, ">$SYMS");

$gather_scoped = 0;
$sym_count = 0;

while (<dump_out>) {
	chop($_);
	($burn, $addr, $size, $type, $bind, $other, $shndx, $name, $cpp_name) =
		split(/[ 	]+/, $_, 9);
	if ($type eq "FILE") {
		if ($name eq $lib_name) {
			$gather_scoped = "1";
		} else {
			$gather_scoped = "0";
		}
		next;
	}

	if ((($bind eq "LOCL") && ($type ne "SECT") &&
	    ($gather_scoped == "1")) || (($bind =~ /(^GLOB|^WEAK)/o) &&
	    ($shndx ne "UNDEF"))) {
		if ($cpp_name ne "") {
			$name = "$name $cpp_name";
			$cplusplus = 1;
		} else {
			$cplusplus = 0;
		}

		if ($cplusplus > 0) {
			print(cpp_out "$name;\n");
			$name =~ s/.*\[//o;
			$name =~ s/\].*//o;
		}
		print(sym_out "$addr|$bind|$name\n");
		$sym_list[$sym_count] = "$addr|$bind|$name";
		$sym_count++;
	}
}


close(dump_out);
close(cpp_out);
close(sym_out);


#
# build mini_cache of header files
#
system("comment_filter $ROOT/usr/include/*.h $ROOT/usr/include/*/*.h $ROOT/usr/ucbinclude/*.h $ROOT/usr/ucbinclude/*/*.h | grep -v '^#' > $HEADER_CACHE");

open(scoped_out, ">$SCOPED");
open(public_out, ">$PUBLIC");
open(private_out, ">$PRIVATE_SYMS");
open(error_out, ">$ERR_FILE");

for ($i = 0; $i < $sym_count; $i++) {
	($addr, $bind, $sym) = split(/\|/o, $sym_list[$i], 3);
	if (defined($special_sym{$sym})) {
		print(scoped_out "$sym;\n");
		print("Special: $sym\n");
		next;
	}
	$scoped = 1;

	$rc = system("egrep", "-s",
		"([	 \\*\\&]|^)$sym([\\,\\[\; 	\(]|\$)",
		$HEADER_CACHE);

	if (($rc == 0) || (defined($mansymbols{$sym}))) {
		print("Defined: $sym\n");
		print(public_out "$sym;\n");
		$scoped = 0;
		if (($rc == 0) && (!defined($mansymbols{$sym}))) {
			print("DEFINED IN HEADER AND NOT IN MAN: $lib:$sym\n");
			print(error_out "DEFINED IN HEADER AND NOT IN MAN: $lib:$sym\n");
		} elsif (($rc != 0) && (defined($mansymbols{$sym}))) {
			print("DEFINED IN MAN AND NOT IN HEADER: $lib:$sym\n");
			print(error_out "DEFINED IN MAN AND NOT IN HEADER: $lib:$sym\n");
		}
	} else  {
		if (defined($exported{$sym})) {
			print("Private: $sym\n");
			print(private_out  "$sym;\n");
			$scoped = 0;
		}
	}

	if ($scoped == 0) {
		@weak_pairs = grep(/$addr.*/, @sym_list);
		foreach $weak_line (@weak_pairs) {
			($waddr, $wbind, $wsym) =
			    split(/\|/o, $weak_line, 3);
			if ($wsym ne $sym) {
				print("WEAK_STRONG Inherited: $wsym\n");
				print(private_out "$wsym;\n");
			}
		}
		next;
	}

	print("Undefined: $sym\n");
	print(scoped_out "$sym;\n");
}

close(public_out);
close(scoped_out);
close(private_out);
close(error_out);

unlink("$HEADER_CACHE");

#
# clean up dups that may be present in some files.  This happens when
# a simbol was raised pulled into another file because of weak/strong
# inheritance.
#
system("sort -u $PUBLIC > $PUBLIC.tmp");
&move("$PUBLIC.tmp", "$PUBLIC");

system("sort -u $SCOPED > $SCOPED.tmp");
&move("$SCOPED.tmp", "$SCOPED");

system("sort -u $PRIVATE_SYMS > $PRIVATE_SYMS.tmp");
&move("$PRIVATE_SYMS.tmp", "$PRIVATE_SYMS");

#
# cleanup private dups
#
system("sort $PRIVATE_SYMS $PUBLIC | uniq -d | sort $PRIVATE_SYMS - | uniq -u > $PRIVATE_SYMS.tmp");
&move("$PRIVATE_SYMS.tmp", "$PRIVATE_SYMS");

#
# cleanup scoped dups
#
system("sort $PRIVATE_SYMS $SCOPED | uniq -d | sort $SCOPED - | uniq -u > $SCOPED.tmp");
&move("$SCOPED.tmp", "$SCOPED");

system("sort $PUBLIC $SCOPED | uniq -d | sort $SCOPED - | uniq -u > $SCOPED.tmp");
&move("$SCOPED.tmp", "$SCOPED");
