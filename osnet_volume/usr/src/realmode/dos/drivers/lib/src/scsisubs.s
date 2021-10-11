;
; Copyright (c) 1997 by Sun Microsystems, Inc.
; All Rights Reserved.
;
;
; ident "@(#)scsisubs.s	1.2	97/03/10 SMI\n"
;
;
;	Interrupt entry stubs for network realmode drivers.
; 
;	Entry points for all possible SCSI HBA device interrupts.
;	We need to be able to support multiple adapters on different
;	IRQ lines.  So we provide separate stubs for each possible IRQ
;	and pass the IRQ number as an argument to the handler.
;
	TITLE   scsisubs

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
	extrn	_scsi_device_interrupt: near
	extrn	_vec_wrap: near

	.386		; Allow assembly of 386-specific instructions
	
	public	_scsi_irq0
_scsi_irq0	proc	far
	push	offset _scsi_device_interrupt
	push	0
	jmp	_vec_wrap
_scsi_irq0	endp

	public	_scsi_irq1
_scsi_irq1	proc	far
	push	offset _scsi_device_interrupt
	push	1
	jmp	_vec_wrap
_scsi_irq1	endp
	
	public	_scsi_irq2
_scsi_irq2	proc	far
	push	offset _scsi_device_interrupt
	push	2
	jmp	_vec_wrap
_scsi_irq2	endp
	
	public	_scsi_irq3
_scsi_irq3	proc	far
	push	offset _scsi_device_interrupt
	push	3
	jmp	_vec_wrap
_scsi_irq3	endp
	
	public	_scsi_irq4
_scsi_irq4	proc	far
	push	offset _scsi_device_interrupt
	push	4
	jmp	_vec_wrap
_scsi_irq4	endp
	
	public	_scsi_irq5
_scsi_irq5	proc	far
	push	offset _scsi_device_interrupt
	push	5
	jmp	_vec_wrap
_scsi_irq5	endp
	
	public	_scsi_irq6
_scsi_irq6	proc	far
	push	offset _scsi_device_interrupt
	push	6
	jmp	_vec_wrap
_scsi_irq6	endp
	
	public	_scsi_irq7
_scsi_irq7	proc	far
	push	offset _scsi_device_interrupt
	push	7
	jmp	_vec_wrap
_scsi_irq7	endp
	
	public	_scsi_irq8
_scsi_irq8	proc	far
	push	offset _scsi_device_interrupt
	push	8
	jmp	_vec_wrap
_scsi_irq8	endp
	
	public	_scsi_irq9
_scsi_irq9	proc	far
	push	offset _scsi_device_interrupt
	push	9
	jmp	_vec_wrap
_scsi_irq9	endp
	
	public	_scsi_irqa
_scsi_irqa	proc	far
	push	offset _scsi_device_interrupt
	push	0ah
	jmp	_vec_wrap
_scsi_irqa	endp
	
	public	_scsi_irqb
_scsi_irqb	proc	far
	push	offset _scsi_device_interrupt
	push	0bh
	jmp	_vec_wrap
_scsi_irqb	endp
	
	public	_scsi_irqc
_scsi_irqc	proc	far
	push	offset _scsi_device_interrupt
	push	0ch
	jmp	_vec_wrap
_scsi_irqc	endp
	
	public	_scsi_irqd
_scsi_irqd	proc	far
	push	offset _scsi_device_interrupt
	push	0dh
	jmp	_vec_wrap
_scsi_irqd	endp
	
	public	_scsi_irqe
_scsi_irqe	proc	far
	push	offset _scsi_device_interrupt
	push	0eh
	jmp	_vec_wrap
_scsi_irqe	endp
	
	public	_scsi_irqf
_scsi_irqf	proc	far
	push	offset _scsi_device_interrupt
	push	0fh
	jmp	_vec_wrap
_scsi_irqf	endp

_TEXT	ENDS
END
