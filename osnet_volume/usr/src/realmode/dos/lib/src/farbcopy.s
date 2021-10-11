;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)farbcopy.s	1.5	94/05/23 SMI\n"

;
; Solaris Primary Boot Subsystem - Support Library Routine
;===========================================================================
; Provides minimal services for the real-mode environment that the operating 
; system would normally supply.
;
;   Function name:	farbcopy  (farbcopy.s)
;
;   Calling Syntax:	farbcopy ( *farsource, *fardest, longlength )
;
;   Description:	copies bytes from source to destination
;			(uses far pointers)
;
;   Restriction:	transfers may occur between different segments,
;			but 64K bytes maximum may be copied during a
;			single operation.  No return code.
;

;ifndef model
;model  textequ <small>
;endif
        .MODEL model, C, NEARSTACK
        .386

        .CODE
        
PUBLIC farbcopy

farbcopy:

        enter 0, 0
        push cx
        push es

        ;load fs, es from segment portions of s, d pointers.
        ;normalize values in fs, es; e.g., fs should contain the segment
        ;portion of "s", plus the high-order 12 bits of the offset portion
        ;of the pointer.  This will leave a very small starting offset value.
        ;This will prevent segment wrapping, as long as we impose a
        ;known restriction of maximum (64KB - 16) transfers.

        ;WARNING: this is a real mode routine; if either the source or
        ;destination pointers is too close to the 1MB barrier, the address
        ;*will* wrap!

        ;prepare to do the transfer; "normalize" the source and destination
        ;addresses, so that, given the restrictions noted above, we can
        ;transfer data, and eliminate the possibility of segment wrap.

        ;recompute source address info: fs + high-order 12 bits of offset

        mov ax, [bp+4]          ;load offset portion of pointer
        mov si, ax              ;si will contain our small source offset
        shr ax, 4               ;strip off low-order 4 bits of source offset
        add ax, [bp+6]          ;create composite segment
        mov fs, ax

        ;load si with low-order 4 bits of offset
        and si, 000Fh

        ;recompute destination address info: es + high-order 12 bits of offset

        mov ax, [bp+8]          ;load offset part of "d"
        mov di, ax              ;di will eventually contain the dest offset
        shr ax, 4               ;strip off low-order 4 bits of dest offset
        add ax, [bp+10]         ;create composite segment
        mov es, ax

        ;load di with low-order 4 bits of offset
        and di, 000Fh           ;retain low-order 4 bits of dest offset

        ;at this point, we have source addr:      fs:si
        ;                       destination addr: es:di
        
        mov ecx, DWORD PTR [bp+12]   ;number of bytes to transfer
        cld                     ;set direction flag for forward processing
        cli

        rep movs BYTE PTR es:[di], BYTE PTR fs:[si]

        sti
        

;        mov ax, WORD PTR [bp-8]                ;number of bytes to transfer
;        mov dx, WORD PTR [bp-6]
;        sub WORD PTR [bp-8], 1
;        sbb WORD PTR [bp-6], 0
;        or  dx, ax
;        je  ALLDONE

;        mov al, BYTE PTR fs:[si]
;        mov BYTE PTR gs:[di], al

;        inc WORD PTR [si]     ;source pointer
;        inc WORD PTR [di]     ;destination pointer

;        jmp SHORT MORE2GO

        pop es
        pop cx
        leave
        ret

        END
