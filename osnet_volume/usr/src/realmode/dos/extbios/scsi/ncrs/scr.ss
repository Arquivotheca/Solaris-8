;
;       Multi-threaded general purpose script for NCR 53C8xx and 53C710
;
; NOTE: The purpose of register SCNTL2 is different for the 710
; and all the other chips but storing a zero into the 53c710's
; SCNTL2 register just before selecting and just before
; disconnecting doesn't hurt anything.

; NOTE: NCR blew it by moving the location of the CTEST2 register.
; They doubly blew it by making the new location of the CTEST2
; register the same as the old (53c710) location of the DMA FIFO
; register. Otherwise to clear the SIGP bit I could have just read
; both locations. But I can't just blindly read the wrong location
; on the 53c710 because reading the DMA FIFO location would cause
; a fifo underflow interrupt.
;
; To handle this situation the driver indicates via SCRATCHA0
; (SCRATCH0) whether this is a 53c710 chip. This SCRIPTS program
; tests the SCRATCHA0 register to determine which location to
; read to clear the SIGP bit.
;
; Copyrighted as an unpublished work.
; (c) Copyright 1993 Sun Microsystems, Inc.
; All rights reserved.
;
; RESTRICTED RIGHTS
;
; These programs are supplied under a license.  They may be used,
; disclosed, and/or copied only as permitted under such license
; agreement.  Any copy must contain the above copyright notice and
; this restricted rights notice.  Use, copying, and/or disclosure
; of the programs is strictly prohibited unless otherwise provided
; in the license agreement.
;
; Copyright (c) 1997 by Sun Microsystems, Inc.
;
; ident "@(#)scr.ss 1.1	97/07/21 SMI" 


	ARCH 810

	ABSOLUTE NBIT_ICON = 0x10	; CON bit in SCNTL1 register

	ABSOLUTE NBIT_IS710 = 0x01	; 710 flag bit in SCRATCHA0 register
	ABSOLUTE NREG_CTEST2_710 = 0x16	; offset of CTEST2 on 53c710 chip

;
; Scatter/Gather DMA instructions for datain and dataout
;
	entry	do_list
	entry	di_list

;       SCSI I/O entry points.  One of these addresses must be loaded into the
;       DSA register to initiate SCSI I/O.

	ENTRY start_up
	ENTRY resel_m
	ENTRY sync_out
	ENTRY clear_ack
	ENTRY continue
	ENTRY errmsg
	ENTRY abort
	ENTRY dev_reset


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

start_up:
	MOVE 0x00 to SCNTL2
	SELECT ATN FROM 0, REL(resel_m)

; after selection, next phase should be msg_out or status
	JUMP REL(status_phase), WHEN STATUS
	INT PASS(NINT_ILI_PHASE), WHEN NOT MSG_OUT

msgout:
	MOVE FROM PASS(NTOFFSET(nt_sendmsg)), WHEN MSG_OUT
	JUMP REL(switch), WHEN NOT MSG_OUT
; target requested repeat, set atn in case it's an extended msg
	SET ATN
	JUMP REL(msgout)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The sync (SDTR) message interrupt handler restarts here if the
; initiator and target have both succesfully exchanged SDTR messages.

clear_ack:
	CLEAR ACK
	JUMP REL(switch)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Restart here after phase mismatch interrupt, clear ATN in case the
; interrupt occurred during the msg_out phase.

continue:
	CLEAR ATN
	JUMP REL(switch)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Send error message to the target. Usually the target will change
; phase immediately. But if in data in or data out phase, or part
; way through a command or message in the phase change will happen
; at the end of the current phase.

errmsg:
	SET ATN
	CLEAR ACK
	JUMP REL(errmsg_out), WHEN MSG_OUT
; not message out phase, the target will change phase later
	JUMP REL(switch)

errmsg_out:
	MOVE FROM PASS(NTOFFSET(nt_errmsg)), WHEN MSG_OUT
	JUMP REL(switch) , WHEN NOT MSG_OUT
; target requested repeat
	JUMP REL(errmsg_out)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; Send an abort message to a target that's attempting an invalid
; reconnection.

abort:
	SET ATN
	CLEAR ACK
	INT PASS(NINT_ILI_PHASE), WHEN NOT MSG_OUT

abort_out:
	MOVE FROM PASS(NTOFFSET(nt_errmsg)), WHEN MSG_OUT
	JUMP REL(abort_done), WHEN NOT MSG_OUT
	SET ATN
	JUMP REL(abort_out)

abort_done:
	MOVE 0x00 to SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_OK)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; send an Abort or Bus Device Reset message and wait for the disconnect

dev_reset:
	MOVE 0x00 to SCNTL2
	SELECT ATN FROM 0, REL(resel_m)
; after selection, next phase should be msg_out
	INT PASS(NINT_ILI_PHASE), WHEN NOT MSG_OUT

dev_reset_out:
	MOVE FROM PASS(NTOFFSET(nt_sendmsg)), WHEN MSG_OUT
	JUMP REL(dev_reset_done), WHEN NOT MSG_OUT
; target requested repeat, set atn in case it's an extended msg
	SET ATN
	JUMP REL(dev_reset_out)

dev_reset_done:
	MOVE 0x00 to SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_DEV_RESET)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The sync (SDTR) message interrupt handler restarts here if the
; initiator needs to send an SDTR message in response to the target's
; SDTR.
;
; set the ATN signal to let target know we've got a message to send
; and ack the last byte of its sdtr message

sync_out:
	SET ATN
	CLEAR ACK
	JUMP REL(sdtr_out), WHEN MSG_OUT
; not message out phase, assume target decided not to do sync i/o
; if this doesn't work, change it to treat this as illegal phase
	CLEAR ATN
	INT PASS(NINT_SDTR_REJECT)

sdtr_out:
	MOVE FROM PASS(NTOFFSET(nt_syncout)), WHEN MSG_OUT
	JUMP REL(sync_out_chk), WHEN NOT MSG_OUT
	SET ATN				 ; target requested repeat
	JUMP REL(sdtr_out)


sync_out_chk:
; test whether the target accepted the SDTR message
; any phase besides MSG_IN means the sdtr message is okay
	JUMP REL(switch), WHEN NOT MSG_IN

; any message besides Message Reject means the SDTR message is okay
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
	JUMP REL(msgin2), IF NOT 0x07		; anything else is okay

; SDTR got Message Reject response
	MOVE 0x00 to SXFER
	INT PASS(NINT_SDTR_REJECT)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

resel_m:
	WAIT RESELECT REL(alt_sig_p1)
resel_m2:
	INT PASS(NINT_MSGIN), WHEN NOT MSG_IN
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
	INT PASS(NINT_RESEL)		; SCSI ID is in SSID register
					; Let UNIX driver grab it


alt_sig_p1:
; either the chip got selected, reselected, and the sig_p
; bit may or may not have been set
	MOVE SCNTL1 & NBIT_ICON to SFBR		; test the connected bit
	JUMP REL(alt_sig_p2), IF NBIT_ICON	; jump if connected


; if here safe to assume sig_p was set and not connected
	INT PASS(NINT_SIGPROC)		; system processor set sig_p bit.

alt_sig_p2:
; Bus initiated interrupt occurred if here --
; connected bit is on, sig_p might be on indicating a selection
; or reselection occurred during the jump caused by the sig_p bit.
; In any event reset the sig_p bit to be safe and process the
; selection or reselection.  The chip will handle this with a
; single WAIT RESELECT command, jumping to the correct
; location depending on whether a selection or reselection occurred.

	MOVE SCRATCHA0 & NBIT_IS710 to SFBR	; is this a 53c710 chip
	JUMP REL(is_8xx), IF NOT NBIT_IS710	; jump if not
	MOVE REG(NREG_CTEST2_710) to SFBR	; clear sig_p bit, if set
	JUMP REL(retry_wait)
is_8xx:
	MOVE CTEST2 to SFBR		; clear sig_p bit, if set
retry_wait:
	WAIT RESELECT REL(alt_sig_p3)	; retry the wait

; if here, reselection occurred when sig_p was set
; process reselection
	JUMP REL(resel_m2)


alt_sig_p3:
; if here, selection occurred and sig_p may or may not have
; been set.  But process selection no matter what.
	INT PASS(NINT_SELECTED)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;       Every phase comes back to here.
switch:
	JUMP REL(msgin), WHEN MSG_IN
	JUMP REL(command_phase), IF CMD
	JUMP REL(dataout_gotos), IF DATA_OUT
	JUMP REL(datain_gotos), IF DATA_IN
	JUMP REL(status_phase), IF STATUS
	JUMP REL(errmsg_out), WHEN MSG_OUT
	INT PASS(NINT_ILI_PHASE)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

msgin:
; read the first byte
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
msgin2:
	JUMP REL(end), IF 0x00			; command complete message
	JUMP REL(ext_msg), IF 0x01		; extended message
	INT PASS(NINT_SDP_MSG), IF 0x02       	; save data pointers
	INT PASS(NINT_RP_MSG), IF 0x03		; restore data pointers
	JUMP REL(disc), IF 0x04			; disconnect message
	INT PASS(NINT_MSGREJ), IF 0x07       	; Message Reject
	INT PASS(NINT_UNS_MSG)			; unsupported message type

disc:
	MOVE 0x00 to SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_DISC)

ext_msg:
;;;
;;; BUG: the defined extended messages can be 2, 3, or 5  bytes long
;;;
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_extmsg)), WHEN MSG_IN
	JUMP REL(wide_msg_in), IF 0x02
	JUMP REL(sync_msg_in), IF 0x03
	INT PASS(NINT_UNS_EXTMSG)

sync_msg_in:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_syncin)), WHEN MSG_IN
	JUMP REL(intr)
wide_msg_in:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_widein)), WHEN MSG_IN

intr:
; don't ack the last byte until after the interrupt handler returns
	INT PASS(NINT_SDTR), IF 0x01
	INT PASS(NINT_WDTR), IF 0x03

; unsupported extended message
	INT PASS(NINT_UNS_EXTMSG)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

command_phase:
	MOVE FROM PASS(NTOFFSET(nt_cmd)), WHEN CMD
	JUMP REL(switch)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

status_phase:
	MOVE FROM PASS(NTOFFSET(nt_status)), WHEN STATUS
	INT PASS(NINT_MSGIN), WHEN NOT MSG_IN
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

end:
	MOVE 0x00 to SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_OK)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The data in and data out scatter/gather dma lists are set up by
; the driver such that they're right justified in the table indirect
; array. In other words if the s/g list contains a single segment then
; only the last entry in the list is used. If the s/g list contains
; two entries then the last two entries are used, etc.. The jump table
; below skip over the unused entries. This way when a phase mismatch
; interrupt occurs I can easily compute how far into the list processing
; has proceeded and reset the pointers and the scratch register to
; properly restart the dma.

dataout_gotos:
	MOVE SCRATCHA1 to SFBR
	INT PASS(NINT_TOOMUCHDATA), IF 0
	JUMP REL(dataout_1), IF 1
	JUMP REL(dataout_2), IF 2
	JUMP REL(dataout_3), IF 3
	JUMP REL(dataout_4), IF 4
	JUMP REL(dataout_5), IF 5
	JUMP REL(dataout_6), IF 6
	JUMP REL(dataout_7), IF 7
	JUMP REL(dataout_8), IF 8
	JUMP REL(dataout_9), IF 9
	JUMP REL(dataout_10), IF 10
	JUMP REL(dataout_11), IF 11
	JUMP REL(dataout_12), IF 12
	JUMP REL(dataout_13), IF 13
	JUMP REL(dataout_14), IF 14
	JUMP REL(dataout_15), IF 15
	JUMP REL(dataout_16), IF 16
	JUMP REL(dataout_17), IF 17
	INT PASS(NINT_TOOMUCHDATA)

do_list:
dataout_17:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[0])), WHEN DATA_OUT
dataout_16:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[1])), WHEN DATA_OUT
dataout_15:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[2])), WHEN DATA_OUT
dataout_14:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[3])), WHEN DATA_OUT
dataout_13:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[4])), WHEN DATA_OUT
dataout_12:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[5])), WHEN DATA_OUT
dataout_11:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[6])), WHEN DATA_OUT
dataout_10:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[7])), WHEN DATA_OUT
dataout_9:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[8])), WHEN DATA_OUT
dataout_8:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[9])), WHEN DATA_OUT
dataout_7:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[10])), WHEN DATA_OUT
dataout_6:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[11])), WHEN DATA_OUT
dataout_5:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[12])), WHEN DATA_OUT
dataout_4:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[13])), WHEN DATA_OUT
dataout_3:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[14])), WHEN DATA_OUT
dataout_2:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[15])), WHEN DATA_OUT
dataout_1:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[16])), WHEN DATA_OUT
	MOVE 0 TO SCRATCHA1
	JUMP REL(switch)

;
; data in processing
;

datain_gotos:
	MOVE SCRATCHA1 to SFBR
	INT PASS(NINT_TOOMUCHDATA), IF 0
	JUMP REL(datain_1), IF 1
	JUMP REL(datain_2), IF 2
	JUMP REL(datain_3), IF 3
	JUMP REL(datain_4), IF 4
	JUMP REL(datain_5), IF 5
	JUMP REL(datain_6), IF 6
	JUMP REL(datain_7), IF 7
	JUMP REL(datain_8), IF 8
	JUMP REL(datain_9), IF 9
	JUMP REL(datain_10), IF 10
	JUMP REL(datain_11), IF 11
	JUMP REL(datain_12), IF 12
	JUMP REL(datain_13), IF 13
	JUMP REL(datain_14), IF 14
	JUMP REL(datain_15), IF 15
	JUMP REL(datain_16), IF 16
	JUMP REL(datain_17), IF 17
	INT PASS(NINT_TOOMUCHDATA)

di_list:
datain_17:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[0])), WHEN DATA_IN
datain_16:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[1])), WHEN DATA_IN
datain_15:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[2])), WHEN DATA_IN
datain_14:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[3])), WHEN DATA_IN
datain_13:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[4])), WHEN DATA_IN
datain_12:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[5])), WHEN DATA_IN
datain_11:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[6])), WHEN DATA_IN
datain_10:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[7])), WHEN DATA_IN
datain_9:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[8])), WHEN DATA_IN
datain_8:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[9])), WHEN DATA_IN
datain_7:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[10])), WHEN DATA_IN
datain_6:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[11])), WHEN DATA_IN
datain_5:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[12])), WHEN DATA_IN
datain_4:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[13])), WHEN DATA_IN
datain_3:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[14])), WHEN DATA_IN
datain_2:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[15])), WHEN DATA_IN
datain_1:	MOVE FROM PASS(NTOFFSET(nt_curdp.nd_data[16])), WHEN DATA_IN
	MOVE 0 TO SCRATCHA1
	JUMP REL(switch)
