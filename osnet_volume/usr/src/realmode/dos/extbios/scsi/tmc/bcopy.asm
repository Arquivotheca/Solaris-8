;
; Copyright 05/16/94 Sun Microsystems, Inc. All Rights Reserved
;
 
; PRAGMA_PLACEHOLDER

; Name      : scsibcopy
; Purpose   : To perform a burst transfer of data from the controller's read
;	     data register map to the host memory
; Called by : tmc_ph_handler()
; Arguments : doff:WORD,dseg:WORD,soff:WORD,sseg:WORD,count:WORD
; Returns   : None
;
; The scsibcopy routine performs a burst transfer of count bytes from the
; controller's read register map to the host memory area specified by its
; segment and offset values as parameters to the function, without performing
; a REQ-ACK handshake for each byte transferred. 
;
;
	.model small,c
scsibcopy	proto	c doff:WORD,dseg:WORD,soff:WORD,sseg:WORD,count:WORD
	.code
	public scsibcopy
scsibcopy proc near c doff:WORD,dseg:WORD,soff:WORD,sseg:WORD,count:WORD
	push	ds
	push	es
	push	si
	push	di
	push	cx
	mov	ax,dseg
	mov	es,ax
	mov	ax,sseg
	mov	ds,ax
	mov	si,soff
	mov	di,doff
	mov	cx,count
	repz	movsb
	pop	cx
	pop	di
	pop	si
	pop	es
	pop	ds
	ret
scsibcopy	endp
	end

	
