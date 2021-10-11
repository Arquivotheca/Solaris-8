#! /usr/bin/csh -f
#
# Copyright(c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident "@(#)which.csh	1.4	97/04/23 SMI"
#
#       which : tells you which program you get
#
# Set prompt so .cshrc will think we're interactive and set aliases.
# Save and restore path to prevent .cshrc from messing it up.
set _which_saved_path_ = ( $path )
set prompt = ""
if ( -r ~/.cshrc && -f ~/.cshrc ) source ~/.cshrc
set path = ( $_which_saved_path_ )
unset prompt _which_saved_path_
set noglob
foreach arg ( $argv )
    set alius = `alias $arg`
    switch ( $#alius )
        case 0 :
            breaksw
        case 1 :
            set arg = $alius[1]
            breaksw
        default :
            echo ${arg}: "      " aliased to $alius
            continue
    endsw
    unset found
    if ( "$arg:h" != "$arg:t" ) then		# head != tail, don't search
        if ( -e $arg ) then			# just do simple lookup
            echo $arg
        else
            echo $arg not found
        endif
        continue
    else
        foreach i ( $path )
            if ( -x $i/$arg && ! -d $i/$arg ) then
                echo $i/$arg
                set found
                break
            endif
        end
    endif
    if ( ! $?found ) then
        echo no $arg in $path
    endif
end

