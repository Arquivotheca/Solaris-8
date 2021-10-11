; 	@(#)low.s	1.2
; 
	.MODEL TINY, C, NEARSTACK
	.386
	.CODE
EXTRN   oldvect: dword
EXTRN	stack: word
EXTRN	start_stk: word
EXTRN	stacksize: word
EXTRN   initmain: near

_low    PROC FAR
	;; 
	;; This code is from the setup.s and I've collapsed just the code
	;; that's needed here.
	;;
	;; ---- Set up segment registers and a local stack ----
	mov     ax, cs
	mov     ds, ax
	mov     es, ax

	;; ---- Pop return address off old stack ----
	pop     cx
	pop     dx

	;; ---- setup our stack ----
	mov     ss, ax
	lea     sp, start_stk
	add	sp, ss:stacksize

	;; ---- Push return address on local stack ----
	push    dx
	push    cx

	call    initmain

	ret
_low    ENDP

	EXTRN   resmain: near

PUBLIC sssave
PUBLIC sosave
PUBLIC MDBmark
PUBLIC resaddr
PUBLIC ressize
PUBLIC MDBcode
PUBLIC topmem
mydata		WORD	0
sosave  	WORD    0
sssave  	WORD    0
MDBmark 	WORD	0BEF1h
resaddr		DWORD	0
ressize		BYTE	0
MDBcode		BYTE	0
topmem          WORD    0

	PUBLIC newvect
newvect PROC FAR
	;; 
	;; INTERCEPTED BIOS ENTRY POINT.
	;; PRESERVE ALL REGISTERS.
	;; 
	sub     sp, 4         ; ---- allow room for long return ----
	push    bp
	push    ds

	mov     cs:sosave, sp
       	mov     cs:sssave, ss
	mov     ss, cs:mydata
	lea     sp, stack
	add	sp, ss:stacksize
	sti

	push	bp
	xor	DWORD PTR ebp, DWORD PTR ebp
	push    ds
	mov     ds, cs:mydata
	ASSUME  ds: DGROUP

	;; ---- Save registers used here ----
	push    es
	mov     es, cs:mydata
	push    di
	push    si
	push    dx
	xor	DWORD PTR edx, DWORD PTR edx
	push    cx
	xor	DWORD PTR ecx, DWORD PTR ecx
	push    bx
	xor	DWORD PTR ebx, DWORD PTR ebx
	push    ax
	xor	DWORD PTR eax, DWORD PTR eax

	call    resmain

	cmp     ax, 0
	je      passon

	pop     ax
	pop     bx
	pop     cx
	pop     dx
	pop     si
	pop     di
	pop     es

	mov     ss, cs:sssave
	mov     sp, cs:sosave

	mov     bp, sp
	jg      success
	or      word ptr 12[bp], 41h	; modify flags on stack
	jmp     common
success:
	;; ---- Turn off carry and zero flag on the stack ----
	and     word ptr 12[bp], 0FFbEH
common:

	pop     ds
	pop     bp
	add     sp, 4
	iret

passon:
	;; ---- Set up pass-on address ----
	les     bx, oldvect
	mov     ds, cs:sssave
	mov	bp, cs:sosave
	mov     ds:4[bp], bx
	mov     ds:6[bp], es

	;; ---- Restore registers ----
	pop     ax
	pop     bx
	pop     cx
	pop     dx
	pop     si
	pop     di
	pop     es

	mov     ss, cs:sssave
	mov     sp, cs:sosave

	pop     ds
	pop     bp
	cli
	ret              ; jump to pass-on address

newvect ENDP

	PUBLIC  hidata
hidata PROC    NEAR
	push    bp
	mov     bp, sp
	mov     ax, 4[bp]
	mov     cs:mydata, ax
	pop     bp
	ret
hidata ENDP

END	_low

