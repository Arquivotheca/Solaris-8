# @(#)cshrc 1.11 89/11/29 SMI
umask 022
set path=(/bin /usr/bin /usr/ucb /etc .)
if ( $?prompt ) then
	set history=32
endif
