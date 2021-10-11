#ident	"@(#)login.csh	1.7	98/10/03 SMI"

# The initial machine wide defaults for csh.

if ( $?TERM == 0 ) then
	if { /bin/i386 } then
		setenv TERM sun-color
	else
		setenv TERM sun
	endif
else
	if ( $TERM == "" ) then
		if { /bin/i386 } then
			setenv TERM sun-color
		else
			setenv TERM sun
		endif
	endif
endif

if (! -e .hushlogin ) then
	/usr/sbin/quota
	/bin/cat -s /etc/motd
	/bin/mail -E
	switch ( $status )
	case 0: 
		echo "You have new mail."
		breaksw;
	case 2: 
		echo "You have mail."
		breaksw;
	endsw
endif
