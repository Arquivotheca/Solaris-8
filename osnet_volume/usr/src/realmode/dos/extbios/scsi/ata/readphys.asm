;
; Copyright (c) 1994 Sun Microsystems, Inc. All Rights Reserved
;
; @(#)readphys.asm   1.1   94/11/09 SMI
;
 
; The readphys routine performs a transfer of count words from the
; specified port to the host memory area specified by its
; segment and offset values as parameters to the function
;
	.model small,c

	.286		; masm rejects "rep insw" etc without this

readphys	proto	c doff:WORD,dseg:WORD,port:WORD,count:WORD
	.code
	public readphys
readphys proc near c doff:WORD,dseg:WORD,port:WORD,count:WORD
	push	dx
	push	es
	push	di
	push	cx
	mov	ax,dseg
	mov	es,ax
	mov	di,doff
	mov	dx,port
	mov	cx,count
	rep	insw
	pop	cx
	pop	di
	pop	es
	pop	dx
	ret
readphys	endp
	end

