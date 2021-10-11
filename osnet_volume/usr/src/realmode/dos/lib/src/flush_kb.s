;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)flush_kb.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	flush_kb  (flush_kb.s)
;
;   Calling Syntax:	flush_kb ( )
;	read_disk ( dev, cyl#, head#, sector#, nsectors, destination )
;
;   Description:	flushes the keyboard buffer.
;		        no input arguments, no return code.
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

PUBLIC flush_kb

flush_kb:			;empties keyboard buffer
     mov ah, 01h                ;read keyboard, any char's waiting?
     int 16h

     jz done                    ;if ZF = 1, buffer has been emptied

     mov ah, 0h			;waits for input
     int 16h			;returns ASCII code for key

     jmp flush_kb

done:

if FARCODE

     retf
else
     ret

endif

     END
