#
# This file is auto-generated. ***ANY*** changes here will be lost
#

package Errno;
use vars qw(@EXPORT_OK %EXPORT_TAGS @ISA $VERSION %errno $AUTOLOAD);
use Exporter ();
use Config;
use strict;

$Config{'myarchname'} eq "i86pc-solaris" or
	die "Errno architecture (i86pc-solaris) does not match executable architecture ($Config{'myarchname'})";

$VERSION = "1.111";
@ISA = qw(Exporter);

@EXPORT_OK = qw(ENOANO EFAULT ENOSYS ELIBACC ELIBBAD ENETDOWN
	EAFNOSUPPORT ENOEXEC EMSGSIZE EALREADY ELOCKUNMAPPED ENOENT ECHILD
	EDEADLK EL2HLT EL2NSYNC ENOTSUP ENOTBLK EISCONN ENOLINK EIDRM EEXIST
	ERANGE EBADRQC EBADMSG ECANCELED ECONNABORTED ECHRNG EMULTIHOP EUSERS
	EDESTADDRREQ EOVERFLOW EREMCHG EXDEV EBFONT EOWNERDEAD ESTALE ELOOP
	EISDIR ENOTRECOVERABLE ELIBSCN ENODEV ENOCSI ESHUTDOWN ECONNREFUSED
	ENOTTY ESOCKTNOSUPPORT EADDRINUSE ENOTEMPTY ESPIPE EUNATCH ENOMSG
	E2BIG ENONET ENOPROTOOPT EREMOTE ENETUNREACH EPIPE ENOTUNIQ ENOTDIR
	ECONNRESET ESRMNT ENODATA EFBIG EIO EADDRNOTAVAIL EROFS EINVAL EBADSLT
	EADV ELIBEXEC EILSEQ EACCES ENOTCONN EAGAIN ENOSPC ENAMETOOLONG
	EHOSTUNREACH ESTRPIPE EBADE EMFILE EBADF ENOBUFS ETXTBSY ENFILE ETIME
	EPFNOSUPPORT EBADFD ENOSTR EWOULDBLOCK ENOLCK EL3RST EBADR EMLINK
	ENOMEM EINTR ELIBMAX ENXIO ENOPKG EDOM ELNRNG ENOTSOCK ENOSR EBUSY
	ERESTART EL3HLT ENETRESET EXFULL ETIMEDOUT ECOMM EINPROGRESS EDQUOT
	EPROTONOSUPPORT ESRCH EPERM EPROTO EPROTOTYPE EHOSTDOWN EDEADLOCK
	ETOOMANYREFS EOPNOTSUPP);

%EXPORT_TAGS = (
    POSIX => [qw(
	E2BIG EACCES EADDRINUSE EADDRNOTAVAIL EAFNOSUPPORT EAGAIN EALREADY
	EBADF EBUSY ECHILD ECONNABORTED ECONNREFUSED ECONNRESET EDEADLK
	EDESTADDRREQ EDOM EDQUOT EEXIST EFAULT EFBIG EHOSTDOWN EHOSTUNREACH
	EINPROGRESS EINTR EINVAL EIO EISCONN EISDIR ELOOP EMFILE EMLINK
	EMSGSIZE ENAMETOOLONG ENETDOWN ENETRESET ENETUNREACH ENFILE ENOBUFS
	ENODEV ENOENT ENOEXEC ENOLCK ENOMEM ENOPROTOOPT ENOSPC ENOSYS ENOTBLK
	ENOTCONN ENOTDIR ENOTEMPTY ENOTSOCK ENOTTY ENXIO EOPNOTSUPP EPERM
	EPFNOSUPPORT EPIPE EPROTONOSUPPORT EPROTOTYPE ERANGE EREMOTE ERESTART
	EROFS ESHUTDOWN ESOCKTNOSUPPORT ESPIPE ESRCH ESTALE ETIMEDOUT
	ETOOMANYREFS ETXTBSY EUSERS EWOULDBLOCK EXDEV
    )]
);

sub EPERM () { 1 }
sub ENOENT () { 2 }
sub ESRCH () { 3 }
sub EINTR () { 4 }
sub EIO () { 5 }
sub ENXIO () { 6 }
sub E2BIG () { 7 }
sub ENOEXEC () { 8 }
sub EBADF () { 9 }
sub ECHILD () { 10 }
sub EWOULDBLOCK () { 11 }
sub EAGAIN () { 11 }
sub ENOMEM () { 12 }
sub EACCES () { 13 }
sub EFAULT () { 14 }
sub ENOTBLK () { 15 }
sub EBUSY () { 16 }
sub EEXIST () { 17 }
sub EXDEV () { 18 }
sub ENODEV () { 19 }
sub ENOTDIR () { 20 }
sub EISDIR () { 21 }
sub EINVAL () { 22 }
sub ENFILE () { 23 }
sub EMFILE () { 24 }
sub ENOTTY () { 25 }
sub ETXTBSY () { 26 }
sub EFBIG () { 27 }
sub ENOSPC () { 28 }
sub ESPIPE () { 29 }
sub EROFS () { 30 }
sub EMLINK () { 31 }
sub EPIPE () { 32 }
sub EDOM () { 33 }
sub ERANGE () { 34 }
sub ENOMSG () { 35 }
sub EIDRM () { 36 }
sub ECHRNG () { 37 }
sub EL2NSYNC () { 38 }
sub EL3HLT () { 39 }
sub EL3RST () { 40 }
sub ELNRNG () { 41 }
sub EUNATCH () { 42 }
sub ENOCSI () { 43 }
sub EL2HLT () { 44 }
sub EDEADLK () { 45 }
sub ENOLCK () { 46 }
sub ECANCELED () { 47 }
sub ENOTSUP () { 48 }
sub EDQUOT () { 49 }
sub EBADE () { 50 }
sub EBADR () { 51 }
sub EXFULL () { 52 }
sub ENOANO () { 53 }
sub EBADRQC () { 54 }
sub EBADSLT () { 55 }
sub EDEADLOCK () { 56 }
sub EBFONT () { 57 }
sub EOWNERDEAD () { 58 }
sub ENOTRECOVERABLE () { 59 }
sub ENOSTR () { 60 }
sub ENODATA () { 61 }
sub ETIME () { 62 }
sub ENOSR () { 63 }
sub ENONET () { 64 }
sub ENOPKG () { 65 }
sub EREMOTE () { 66 }
sub ENOLINK () { 67 }
sub EADV () { 68 }
sub ESRMNT () { 69 }
sub ECOMM () { 70 }
sub EPROTO () { 71 }
sub ELOCKUNMAPPED () { 72 }
sub EMULTIHOP () { 74 }
sub EBADMSG () { 77 }
sub ENAMETOOLONG () { 78 }
sub EOVERFLOW () { 79 }
sub ENOTUNIQ () { 80 }
sub EBADFD () { 81 }
sub EREMCHG () { 82 }
sub ELIBACC () { 83 }
sub ELIBBAD () { 84 }
sub ELIBSCN () { 85 }
sub ELIBMAX () { 86 }
sub ELIBEXEC () { 87 }
sub EILSEQ () { 88 }
sub ENOSYS () { 89 }
sub ELOOP () { 90 }
sub ERESTART () { 91 }
sub ESTRPIPE () { 92 }
sub ENOTEMPTY () { 93 }
sub EUSERS () { 94 }
sub ENOTSOCK () { 95 }
sub EDESTADDRREQ () { 96 }
sub EMSGSIZE () { 97 }
sub EPROTOTYPE () { 98 }
sub ENOPROTOOPT () { 99 }
sub EPROTONOSUPPORT () { 120 }
sub ESOCKTNOSUPPORT () { 121 }
sub EOPNOTSUPP () { 122 }
sub EPFNOSUPPORT () { 123 }
sub EAFNOSUPPORT () { 124 }
sub EADDRINUSE () { 125 }
sub EADDRNOTAVAIL () { 126 }
sub ENETDOWN () { 127 }
sub ENETUNREACH () { 128 }
sub ENETRESET () { 129 }
sub ECONNABORTED () { 130 }
sub ECONNRESET () { 131 }
sub ENOBUFS () { 132 }
sub EISCONN () { 133 }
sub ENOTCONN () { 134 }
sub ESHUTDOWN () { 143 }
sub ETOOMANYREFS () { 144 }
sub ETIMEDOUT () { 145 }
sub ECONNREFUSED () { 146 }
sub EHOSTDOWN () { 147 }
sub EHOSTUNREACH () { 148 }
sub EALREADY () { 149 }
sub EINPROGRESS () { 150 }
sub ESTALE () { 151 }

sub TIEHASH { bless [] }

sub FETCH {
    my ($self, $errname) = @_;
    my $proto = prototype("Errno::$errname");
    if (defined($proto) && $proto eq "") {
	no strict 'refs';
        return $! == &$errname;
    }
    require Carp;
    Carp::confess("No errno $errname");
} 

sub STORE {
    require Carp;
    Carp::confess("ERRNO hash is read only!");
}

*CLEAR = \&STORE;
*DELETE = \&STORE;

sub NEXTKEY {
    my($k,$v);
    while(($k,$v) = each %Errno::) {
	my $proto = prototype("Errno::$k");
	last if (defined($proto) && $proto eq "");
	
    }
    $k
}

sub FIRSTKEY {
    my $s = scalar keys %Errno::;
    goto &NEXTKEY;
}

sub EXISTS {
    my ($self, $errname) = @_;
    my $proto = prototype($errname);
    defined($proto) && $proto eq "";
}

tie %!, __PACKAGE__;

1;
__END__

=head1 NAME

Errno - System errno constants

=head1 SYNOPSIS

    use Errno qw(EINTR EIO :POSIX);

=head1 DESCRIPTION

C<Errno> defines and conditionally exports all the error constants
defined in your system C<errno.h> include file. It has a single export
tag, C<:POSIX>, which will export all POSIX defined error numbers.

C<Errno> also makes C<%!> magic such that each element of C<%!> has a non-zero
value only if C<$!> is set to that value, eg

    use Errno;
    
    unless (open(FH, "/fangorn/spouse")) {
        if ($!{ENOENT}) {
            warn "Get a wife!\n";
        } else {
            warn "This path is barred: $!";
        } 
    } 

=head1 AUTHOR

Graham Barr <gbarr@pobox.com>

=head1 COPYRIGHT

Copyright (c) 1997-8 Graham Barr. All rights reserved.
This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

=cut

