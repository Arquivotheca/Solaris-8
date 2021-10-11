;
; Copyright (c) 1997, by Sun Microsystems, Inc.
; All rights reserved.
;
;
; ident "@(#)pciconf.s	1.5	97/10/20 SMI\n"
; 
;	Realmode driver utility routines for PCI configuration register I/O.
;
; Interrupt vector for PCI BIOS calls
PCI_BIOS_INT		equ	1Ah

; Function and sub-function values for PCI BIOS calls
PCI_FUNCTION_ID		equ	0B1h
PCI_BIOS_PRESENT	equ	1
FIND_PCI_DEVICE		equ	2
READ_CONFIG_BYTE	equ	8
READ_CONFIG_WORD	equ	9
READ_CONFIG_DWORD	equ	0Ah
WRITE_CONFIG_BYTE	equ	0Bh
WRITE_CONFIG_WORD	equ	0Ch
WRITE_CONFIG_DWORD	equ	0Dh

; PCI BIOS call return codes
SUCCESSFUL		equ	0

; Data size override instruction
DATA16	EQU	66H
;

	.MODEL SMALL, C, NEARSTACK
	.386


	.CODE			;code segment begins here

;
;   Function name:	is_pci
;
;   Calling Syntax:	int is_pci(void);
;
;   Description:	returns 1 for PCI-bus system, 0 for non-PCI.
;
PUBLIC  is_pci

is_pci:

	push	bx
	mov	ah, PCI_FUNCTION_ID
	mov	al, PCI_BIOS_PRESENT
	int	PCI_BIOS_INT

	jc	is_pci_no
	cmp	ah, SUCCESSFUL
	jne	is_pci_no
	cmp	dl, 'P'
	jne	is_pci_no
	cmp	dh, 'C'
	jne	is_pci_no
	db	DATA16
	shr	dx, 16
	cmp	dl, 'I'
	jne	is_pci_no
	cmp	dh, ' '
	jne	is_pci_no
	mov	ax, 1
	jmp	is_pci_cmn
is_pci_no:
	xor	ax, ax
is_pci_cmn:
	pop	bx

	ret


;
;   Function name:	pci_find_device
;
;   Calling Syntax:	int pci_find_device(ushort vendor_id, ushort device_id,
;					    ushort index, ushort *id);
;
;   Description:	Look for the 'index'th instance of a device with the
;			given vendor/device IDs (index is 0-based).
;			returns 1 if device is found and 0 if not.
;			Returns bus/device/function numbers in *id if found.
;
PUBLIC  pci_find_device

pci_find_device:

	push	bp
	mov	bp, sp
	push	bx
	push	si
	push	es
	mov	ah, PCI_FUNCTION_ID
	mov	al, FIND_PCI_DEVICE
	mov	dx, 4[bp]		; vendor_id
	mov	cx, 6[bp]		; device_id
	mov	si, 8[bp]		; index
	int	PCI_BIOS_INT

	jc	pci_find_no
	cmp	ah, SUCCESSFUL
	jne	pci_find_no
	mov	si, 10[bp]
	mov	[si], bx
	mov	ax, 1
	jmp	pci_find_cmn
pci_find_no:
	xor	ax, ax
pci_find_cmn:
	pop	es
	pop	si
	pop	bx
	pop	bp

	ret


;
;   Function name:	pci_read_config_byte
;
;   Calling Syntax:	int pci_read_config_byte(ushort id, ushort offset,
;						 unchar *result);
;
;   Description:	returns 1 if byte is read and 0 if not.
;			'offset' is the byte offset in configuration space.
;			Returns byte at *result.
;
PUBLIC  pci_read_config_byte

pci_read_config_byte:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	push	es
	mov	ah, PCI_FUNCTION_ID
	mov	al, READ_CONFIG_BYTE
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	int	PCI_BIOS_INT

	jc	read_byte_no
	cmp	ah, SUCCESSFUL
	jne	read_byte_no
	mov	bx, 8[bp]		; result
	mov	[bx], cl
	mov	ax, 1
	jmp	read_byte_cmn
read_byte_no:
	xor	ax, ax
read_byte_cmn:
	pop	es
	pop	di
	pop	bx
	pop	bp

	ret


;
;   Function name:	pci_read_config_word
;
;   Calling Syntax:	int pci_read_config_word(ushort id, ushort offset,
;						 ushort *result);
;
;   Description:	returns 1 if word is read and 0 if not.
;			'offset' is the byte offset in configuration space.
;			Returns word at *result.
;
PUBLIC  pci_read_config_word

pci_read_config_word:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	push	es
	mov	ah, PCI_FUNCTION_ID
	mov	al, READ_CONFIG_WORD
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	test	di, 1
	jnz	read_word_no
	int	PCI_BIOS_INT

	jc	read_word_no
	cmp	ah, SUCCESSFUL
	jne	read_word_no
	mov	bx, 8[bp]		; result
	mov	[bx], cx
	mov	ax, 1
	jmp	read_word_cmn
read_word_no:
	xor	ax, ax
read_word_cmn:
	pop	es
	pop	di
	pop	bx
	pop	bp

	ret


;
;   Function name:	pci_read_config_dword
;
;   Calling Syntax:	int pci_read_config_dword(ushort id, ushort offset,
;						  ulong *result);
;
;   Description:	returns 1 if dword is read and 0 if not.
;			'offset' is the byte offset in configuration space.
;			Returns dword at *result.
;
PUBLIC  pci_read_config_dword

pci_read_config_dword:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	push	es
	mov	ah, PCI_FUNCTION_ID
	mov	al, READ_CONFIG_DWORD
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	test	di, 3
	jnz	read_dword_no
	int	PCI_BIOS_INT

	jc	read_dword_no
	cmp	ah, SUCCESSFUL
	jne	read_dword_no
	mov	bx, 8[bp]
	db	DATA16
	mov	[bx], cx		; result
	mov	ax, 1
	jmp	read_dword_cmn
read_dword_no:
	xor	ax, ax
read_dword_cmn:
	pop	es
	pop	di
	pop	bx
	pop	bp

	ret

;
;   Function name:	pci_write_config_byte
;
;   Calling Syntax:	int pci_write_config_byte(ushort id, ushort offset,
;						 unchar value);
;
;   Description:	returns 1 if byte is written and 0 if not.
;			'offset' is the byte offset in configuration space.
;
PUBLIC  pci_write_config_byte

pci_write_config_byte:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	mov	ax, PCI_FUNCTION_ID*256+WRITE_CONFIG_BYTE
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	mov	cl, 8[bp]		; value

	int	PCI_BIOS_INT

	jc	write_byte_no
	cmp	ah, SUCCESSFUL
	jne	write_byte_no
	mov	ax, 1
	jmp	write_byte_cmn
write_byte_no:
	xor	ax, ax
write_byte_cmn:
	pop	di
	pop	bx
	pop	bp

	ret


;
;   Function name:	pci_write_config_word
;
;   Calling Syntax:	int pci_write_config_word(ushort id, ushort offset,
;						 ushort value);
;
;   Description:	returns 1 if word is written and 0 if not.
;			'offset' is the byte offset in configuration space.
;
PUBLIC  pci_write_config_word

pci_write_config_word:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	mov	ax, PCI_FUNCTION_ID*256+WRITE_CONFIG_WORD
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	mov	cx, 8[bp]		; value

	int	PCI_BIOS_INT

	jc	write_word_no
	cmp	ah, SUCCESSFUL
	jne	write_word_no
	mov	ax, 1
	jmp	write_word_cmn
write_word_no:
	xor	ax, ax
write_word_cmn:
	pop	di
	pop	bx
	pop	bp

	ret


;
;   Function name:	pci_write_config_dword
;
;   Calling Syntax:	int pci_write_config_dword(ushort id, ushort offset,
;						 ulong value);
;
;   Description:	returns 1 if dword is written and 0 if not.
;			'offset' is the byte offset in configuration space.
;
PUBLIC  pci_write_config_dword

pci_write_config_dword:

	push	bp
	mov	bp, sp
	push	bx
	push	di
	mov	ax, PCI_FUNCTION_ID*256+WRITE_CONFIG_DWORD
	mov	bx, 4[bp]		; id
	mov	di, 6[bp]		; offset
	db	DATA16			; force 32 bit transfer on next instr
	mov	cx, 8[bp]		; value

	int	PCI_BIOS_INT

	jc	write_dword_no
	cmp	ah, SUCCESSFUL
	jne	write_dword_no
	mov	ax, 1
	jmp	write_dword_cmn
write_dword_no:
	xor	ax, ax
write_dword_cmn:
	pop	di
	pop	bx
	pop	bp

	ret

	END
