#!/usr/dist/exe/perl

$PRIV_SYMS="private_syms";
$PUB_SYMS="public_syms";

if ($#ARGV < 0) {
	print("usage: check_mapfile <mapfile> ... \n");
	exit(1);
}

#
# Read in all the mapfiles and record their symbols
#
for($i = 0; $i <= $#ARGV; $i++) {
	$mapfile = $ARGV[$i];
	if ( ! -f $mapfile ) {
		print("check_mapfile: file $mapfile does not exist\n");
		exit(1);
	}
	open(mapfile, "$mapfile");
	$global = 0;
	$private = 0;
	while(<mapfile>) {
		chop($_);
	
		# 
		# Throw away comments and blank lines
		#
		if ( /^\#/ || /\}.*;/o || /^[ \t]*$/o ) {
			next;
		}

		#
		# identify collection of 'private' symbols
		#
		if ( /^[ \t]*SUNW.*private.*\{/o ) {
			$vers = $_;
			$vers =~ s/[ 	]*\{//go;
			$private = 1;
			next;
		}
	
		#
		# Identify collection of 'public' symbols
		if ( /.*\{/ ) {
			$vers = $_;
			$vers =~ s/[ 	]*\{//go;
			$private = 0;
			next;
		}
	
		if ( /^[ \t]*global:/ ) {
			$global = 1;
			next;
		}
	
		if ( /^[ \t]*local:/ ) {
			$global = 0;
			next;
		}

		#
		# Only collect information for global symbols
		#
		if ($global == "0") {
			next;
		}

		$sym = $_;
		$sym =~ s/^[ \t]*([^;]+);.*/$1/o;
		$sym =~ s/[ \t]*//go;
	
		if ($private == "1") {
			$private_syms{$sym} = 0;
		} else {
			$public_syms{$sym} = 0;
		}
		$version{$sym} = $vers;
	}
	close(<mapfile>);

}


#
# Check globals first
#

if ( ! -f $PUB_SYMS ) {
	print("check_mapfile: file $PUB_SYMS does not exist\n");
	exit(1);
}

open(publics, "$PUB_SYMS");

while (<publics>) {
	chop($_);
	$sym = $_;
	$sym =~ s/;//;
	if (defined($private_syms{$sym})) {
		printf("PUBLIC SYM SCOPED PRIVATE: $sym ($version{$sym})\n");
		$private_syms{$sym} = 1;
	} elsif (!defined($public_syms{$sym})) {
		printf("PUBLIC SYM NOT IN MAPFILE: $sym\n");
	} else {
		$public_syms{$sym} = 1;
	}
}

close(publics);

if ( ! -f $PRIV_SYMS ) {
	print("check_mapfile: file $PRIV_SYMS does not exist\n");
	exit(1);
}

open(privates, "$PRIV_SYMS");

while (<privates>) {
	chop($_);
	$sym = $_;
	$sym =~ s/;//;
	if (defined($public_syms{$sym})) {
		printf("PRIVATE SYM SCOPED PUBLIC: $sym ($version{$sym})\n");
		$public_syms{$sym} = 1;
	} elsif (!defined($private_syms{$sym})) {
		printf("PRIVATE SYM NOT IN MAPFILE: $sym\n");
	} else {
		$private_syms{$sym} = 1;
	}
}
close(privates);

foreach $key (keys(%public_syms)) {
	if ($public_syms{$key} == 0 ) {
		print("MAPFILE_PUB SYM NOT IN FILES: $key ($version{$key})\n");
	}
}

foreach $key (keys(%private_syms)) {
	if ($private_syms{$key} == 0 ) {
		print("MAPFILE_PRIV SYM NOT IN FILES: $key ($version{$key})\n");
	}
}
