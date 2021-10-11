;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)nwrd_key.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	nowait_read_key  (nwrd_key.s)
;
;   Calling Syntax:	keycode = nowait_read_key ( )
;
;   Description:	checks keyboard status; returns an ASCII keycode
;			if one is waiting in keyboard buffer, otherwise
;			returns 0.
;
;

;this allows the model to be defined from the command line.
;the default in this case is small.

;ifndef model
;model   textequ <small>
;endif

    .MODEL model, C, NEARSTACK
;    include ..\bioserv.inc     this doesn't work-causes multiple def's
    .386

    .CODE			;code segment begins here

PUBLIC nowait_read_key

nowait_read_key:		;ASCII code = nowait_read_key ( )
     mov ah, 1h			;read keyboard status
     int 16h			;returns ASCII code for key

     jz end_key                 ;ZF = 1 means no character ready, just rtn.

     mov ah, 0h			;status call tells us a character is waiting.
     int 16h			;returns ASCII code for key

     xor ah, ah                 ;ah contains scan code;we only want the ascii
                                ;value in al.
if FARCODE

     retf
else
     ret

endif

end_key:

     xor ax, ax                 ;return 0 if the cupboard was bare......

if FARCODE

     retf
else
     ret

endif

     END
