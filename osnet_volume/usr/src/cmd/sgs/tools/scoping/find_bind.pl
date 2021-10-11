#!/usr/dist/exe/perl

$OUT_DIR = "/tmp/out.$$";


sub print_bindings {
	local($obj) = @_;
	unlink (<$OUT_DIR/*>);
	
	$ENV{'LD_DEBUG'}="bindings";
	$ENV{'LD_DEBUG_OUTPUT'}="$OUT_DIR/bind";
	system("ldd -r $obj > /dev/null");
	$ENV{'LD_DEBUG'}="";
	$ENV{"LD_DEBUG_OUTPUT"}="";

	@files=<$OUT_DIR/bind.*>;
	$obj =~ s|^$ROOT||;

	#
	# I'm assuming that the file with the highest PID is the
	# one I'm interested in...
	#
	$bind_file="";
	for ($i=0; $i <= $#files; $i++) {
		if ($bind_file lt $files[$i]) {
			$bind_file = $files[$i];
		}
	}

	if ( $bind_file eq "" ) {
		#
		# didn't find anything - I give up on this one.
		#
		return;
	}

	open(bindings, "$bind_file");
	while (<bindings>) {
		chop($_);
		if ($_ =~ /\(undefined weak\)/o) {
			next;
		}
		($burn, $title, $from, $burn, $to, $burn, $sym) =
		    split(/  */, $_, 7);
		if ($title eq "binding") {
			$from = substr($from, 5);
			$from =~ s|^$ROOT||;
			$to = substr($to, 5, length($to) - 6);
			$to =~ s|^$ROOT||;
			$sym = substr($sym, 1, length($sym) - 2);
			if ($to eq $from) {
				next;
			}
			print(out_file "$to|$sym|$from|$obj\n");
		}
	}
	close(<bindings>);
}

if ( $#ARGV != 1 ) {
	print("usage: find_bind <outfile> <base_directory>\n");
	print("\toutfile\t\t- name of output file for binding information\n");
	print("\tbase_directory\t- directory under which to search for objects to analyze\n");
	exit(1);
}


if ( !defined($ENV{'ROOT'}) || $ENV{'ROOT'} eq "" ) {
	print "ROOT not set, assuming /\n";
	$ROOT="/";
} else {
	$ROOT=$ENV{'ROOT'};
}

$outfile=@ARGV[0];
$base_dir=@ARGV[1];

open(file_list,"find $base_dir -type f -print |");
open(out_file, ">$outfile");

mkdir("$OUT_DIR",0777);

while (<file_list>) 
{
	chop($_);
	$file=$_;
	$file_type=`file $file`;
	print ("$file_type");
	if ( $file_type =~ /ELF.*executable.*dynamically/ ) {
		&print_bindings($file);
	} elsif ( $file_type =~ /ELF.*dynamic lib/ ) {
		&print_bindings($file);
	} elsif ( $file_type =~ /Sun demand paged SPARC.*dynamically linked/) {
		&print_bindings($file);
	}
}

close(<file_list>);
close(<out_file>);

unlink (<$OUT_DIR/*>);
rmdir($OUT_DIR);
