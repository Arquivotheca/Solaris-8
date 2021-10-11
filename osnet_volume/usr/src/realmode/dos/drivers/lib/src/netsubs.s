;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)netsubs.s	1.2	97/03/10 SMI\n"
;
;
;	Interrupt entry stubs for network realmode drivers.
;
	TITLE   netsubs

_TEXT	SEGMENT  BYTE PUBLIC 'CODE'
_TEXT	ENDS
_DATA	SEGMENT  WORD PUBLIC 'DATA'
_DATA	ENDS
CONST	SEGMENT  WORD PUBLIC 'CONST'
CONST	ENDS
_BSS	SEGMENT  WORD PUBLIC 'BSS'
_BSS	ENDS
DGROUP	GROUP	CONST,	_BSS,	_DATA
	ASSUME  CS: _TEXT, DS: _DATA, SS: nothing, ES: nothing

_TEXT   SEGMENT
	extrn	_net_service: near
	extrn	_net_device_interrupt: near
	extrn	_vec_wrap: near

	.386		; Allow assembly of 386-specific instructions

;; _bios_intercept
;; general interface to the driver which supports things like open,
;; close, read, and write. this is a private interface developed by
;; SunSoft. Push the interface pointer and a dummy argument so that
;; _vec_wrap can setup the stack and call the routine

	public	_bios_intercept
_bios_intercept	proc	far
	push	offset _net_service
	push	0
	jmp	_vec_wrap
_bios_intercept	endp

; intr_vec --
; the network framework has stored this far pointer routine into one
; of the hardware interrupt vectors. Push the call pointer and a
; dummy argument so that _vec_wrap can set things up and call the handler.

	public	_intr_vec
_intr_vec	proc	far
	push	offset _net_device_interrupt
	push	0
	jmp	_vec_wrap
_intr_vec	endp


_TEXT	ENDS
END
