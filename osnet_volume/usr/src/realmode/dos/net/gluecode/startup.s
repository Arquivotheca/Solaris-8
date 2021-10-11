;
; Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
;

; ident	"@(#)startup.s	1.5	94/05/23 SMI\n"

;
;Assembler preamble for gluecode module.
;

;Test program: calling C modules from MASM.

    .MODEL COMPACT, C, NEARSTACK
;
;NOTE the missing stack declaration
;
;    .386

       .CODE			    ;code segment begins here

EXTRN  main:NEAR

	   org 1000h

TSTACK:
@Startup:
    cli
;
    mov ax, cs                      ;initialize segment registers
    mov ss, ax
    mov sp, word ptr TSTACK
    mov ax, SEG DGROUP              ;must be same seg or use far refs
    mov ds, ax
    mov es, ax
    sti
    call main                  ;C entry point
;
hang:
    jmp hang

PUBLIC  putachar
putachar PROC NEAR
	push    bp
	mov     bp, sp
	mov     ax, 4[bp]
	mov     ah, 0eH
	int     10H
	pop     bp
	retf
putachar ENDP
    
    END @Startup
