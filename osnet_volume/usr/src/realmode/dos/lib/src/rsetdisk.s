;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)rsetdisk.s	1.6	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	reset_disk  (rsetdisk.s)
;
;   Calling Syntax:	rc = reset_disk ( drive# )
;
;   Description:	resets the specified device to a known state.
;			This function should be called after any I/O operation
;			fails, in order to reset the device to a known state.
;			Input argument is drive# (0 or 1).  Returns 0 if no
;			error, disk service error code otherwise.
;
;   Assumptions:	this routine uses an external variable "tmpAX",
;			that must be defined by the calling routine.
;

;this allows the model to be defined from the command line.
;the default in this case is small.

;ifndef model
;model   textequ <small>
;endif

    .MODEL model, C, NEARSTACK
;    include ..\bioserv.inc     this doesn't work-causes multiple def's
    .386

;    .DATA


    .CODE			;code segment begins here

PUBLIC reset_disk
EXTERN tmpAX : WORD

reset_disk:			;reset_disk ( drive# )

     pusha			;NOTE: the system will not respond 
     mov bp, sp			;immediately; device will recalibrate 

if FARCODE

     add bp, 14h		;at start of next I/O operation.
else
     add bp, 12h		;at start of next I/O operation.

endif

     mov dl, [bp]		;specify the target drive number
     xor ax, ax			;initialize return code register

     mov ah, 0h			;reset diskette subsystem function number
     int 13h

     mov tmpAX, ax           ;al not used by this function (hd or fd)
     popa
     mov ax, tmpAX		;0=no error, other diskette svc. error code

if FARCODE

     retf
else
     ret

endif

     END

