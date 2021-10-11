;
; Copyright (c) 1998, by Sun Microsystems, Inc.
; All rights reserved.
;
;       Multi-threaded general purpose script for the
;	Symbios 53C825/875 host bus adapter chips.
;
; ident	"@(#)scr.ss 1.4     98/07/23 SMI"

	ARCH 825A

	ABSOLUTE NBIT_ICON = 0x10	; CON bit in SCNTL1 register

;
; Scatter/Gather DMA instructions for datain and dataout
;
	ENTRY	do_list_end
	ENTRY	di_list_end

;       SCSI I/O entry points.  One of these addresses must be loaded into the
;       DSA register to initiate SCSI I/O.

	ENTRY start_up
	ENTRY resel_m
	ENTRY ext_msg_out
	ENTRY clear_ack
	ENTRY continue
	ENTRY errmsg
	ENTRY abort
	ENTRY dev_reset

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

start_up:
	SELECT ATN FROM 0, REL(resel_m)

; after selection, next phase should be msg_out or status
	INT PASS(NINT_ILI_PHASE), WHEN NOT MSG_OUT

msgout:
	MOVE FROM PASS(NTOFFSET(nt_sendmsg)), WHEN MSG_OUT
	JUMP REL(command_phase), WHEN CMD
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
	MOVE 0x00 TO SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_OK)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; send an Abort or Bus Device Reset message and wait for the disconnect

dev_reset:
	MOVE 0x00 TO SCNTL2
	SELECT ATN FROM 0, REL(resel_m)
; after selection, next phase should be msg_out
	INT PASS(NINT_ILI_PHASE), WHEN NOT MSG_OUT

dev_reset_out:
	MOVE FROM PASS(NTOFFSET(nt_sendmsg)), WHEN MSG_OUT
	CLEAR ACK
	MOVE SCNTL2 & 0x7F TO SCNTL2
	WAIT DISCONNECT
	INT PASS(NINT_DEV_RESET)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The sync (SDTR) or wide (WDTR) message interrupt handler restarts here
; if the initiator needs to send an SDTR/WDTR message in response to the
; target's SDTR/WDTR.
;
; Set the ATN signal to let target know we've got a message to send
; and ack the last byte of its SDTR/WDTR message.

ext_msg_out:
	SET ATN
	CLEAR ACK
	JUMP REL(msg_out_loop), WHEN MSG_OUT
; not message out phase, assume target decided not to do sync i/o
; if this doesn't work, change it to treat this as illegal phase
	CLEAR ATN
	INT PASS(NINT_NEG_REJECT)

msg_out_loop:
	MOVE FROM PASS(NTOFFSET(nt_sendmsg)), WHEN MSG_OUT
	JUMP REL(ext_msg_out_chk), WHEN NOT MSG_OUT
	SET ATN				 ; target requested repeat
	JUMP REL(msg_out_loop)


ext_msg_out_chk:
; test whether the target accepted the SDTR message
; any phase besides MSG_IN means the sdtr message is okay
	JUMP REL(switch), WHEN NOT MSG_IN

; any message besides Message Reject means the SDTR message is okay
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
	JUMP REL(msgin2), IF NOT 0x07		; anything else is okay

; SDTR got Message Reject response
	MOVE 0x00 TO SXFER
	INT PASS(NINT_NEG_REJECT)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

resel_m:
	WAIT RESELECT REL(alt_sig_p1)
resel_m2:
	;
	; The 256 byte aligned dsa table address saved in SCRATCHB(bytes 1-3)
	; is copied to DSA reg and indexed by the reselecting targets
	; SCSI ID value.
	;
	MOVE SSID SHL SFBR
	MOVE SFBR SHL SFBR
	MOVE SFBR & 0x3C TO DSA0	 ; clear high and low bits
	MOVE SCRATCHB1 TO SFBR
        MOVE SFBR TO DSA1
        MOVE SCRATCHB2 TO SFBR
        MOVE SFBR TO DSA2
        MOVE SCRATCHB3 TO SFBR
        MOVE SFBR TO DSA3
	SELECT FROM 0x00, REL(Next_Inst) ; Load per ID regs from dsa table
Next_Inst:
	;
	; Clear the low byte of DSA so that the PASS(HBAOFFSET(struct member))
	; references which follow will work
	;
	MOVE 0x00 TO DSA0
	;
	INT PASS(NINT_MSGIN), WHEN NOT MSG_IN
	;
	; save the reselection Identify msg in dsa->g_rcvmsg.
	;
	MOVE FROM PASS(HBAOFFSET(g_rcvmsg)), WHEN MSG_IN
	CLEAR ACK
	;
	; Target will either continue in msg-in phase (tag q'ing) or
	; transistion to data or status phase.
	;
	; non-tq case: target switched to status phase.
	;
	INT PASS(NINT_RESEL), WHEN NOT MSG_IN	; Let UNIX driver grab it
	;
	; save the reselection Identify msg in dsa->g_tagmsg.
	; should be the 0x20 (tag msg).
	;
	MOVE FROM PASS(HBAOFFSET(g_tagmsg)), WHEN MSG_IN
	JUMP REL(Got_sdp), IF 0x02		; jump if save data pointers
	;
	; Check msg-in byte for 20, 21 or 22.
	JUMP REL(Got_tag), IF 0x20 AND MASK 0x01
	INT PASS(NINT_RESEL), IF NOT 0x22	; Let UNIX driver grab it
Got_tag:
	CLEAR ACK
	MOVE FROM PASS(HBAOFFSET(g_tagmsg)), WHEN MSG_IN
Got_sdp:
	CLEAR ACK
	INT PASS(NINT_RESEL)			; Let UNIX driver grab it

alt_sig_p1:
; either the chip got selected, reselected, and the sig_p
; bit may or may not have been set
	MOVE SCNTL1 & NBIT_ICON TO SFBR		; test the connected bit
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
	MOVE CTEST2 TO SFBR		; clear sig_p bit, if set
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
	JUMP REL(dataout_gotos), IF DATA_OUT
	JUMP REL(datain_gotos), IF DATA_IN
	JUMP REL(status_phase), IF STATUS
	JUMP REL(command_phase), IF CMD
	JUMP REL(errmsg_out), WHEN MSG_OUT
	INT PASS(NINT_ILI_PHASE)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

msgin:
; read the first byte
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
msgin2:
	JUMP REL(end), IF 0x00			; command complete message
	INT PASS(NINT_SDP_MSG), IF 0x02		; save data pointers
	JUMP REL(disc), IF 0x04			; disconnect message
	INT PASS(NINT_RP_MSG), IF 0x03		; restore data pointers
	INT PASS(NINT_MSGREJ), IF 0x07		; Message Reject
	JUMP REL(ext_msg), IF 0x01		; extended message
	JUMP REL(ignore_wide_residue), IF 0x23	; ignore wide residue
	INT PASS(NINT_UNS_MSG)			; unsupported message type

disc:
	MOVE 0x00 TO SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_DISC)

ext_msg:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_extmsg)), WHEN MSG_IN
	JUMP REL(wide_msg_in), IF 0x02
	JUMP REL(sync_msg_in), IF 0x03
	INT PASS(NINT_UNS_EXTMSG)

ignore_wide_residue:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN
	INT PASS(NINT_IWR)

sync_msg_in:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_syncin)), WHEN MSG_IN
; don't ack the last byte until after the interrupt handler returns
	INT PASS(NINT_SDTR), IF 0x01

; unsupported extended message
	INT PASS(NINT_UNS_EXTMSG)

wide_msg_in:
	CLEAR ACK
	MOVE FROM PASS(NTOFFSET(nt_widein)), WHEN MSG_IN
; don't ack the last byte until after the interrupt handler returns
	INT PASS(NINT_WDTR), IF 0x03

; unsupported extended message
	INT PASS(NINT_UNS_EXTMSG)

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

command_phase:
	MOVE FROM PASS(NTOFFSET(nt_cmd)), WHEN CMD
	JUMP REL(msgin), WHEN MSG_IN
	JUMP REL(switch)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

status_phase:
	MOVE FROM PASS(NTOFFSET(nt_status)), WHEN STATUS
	JUMP REL(switch), WHEN NOT MSG_IN
	MOVE FROM PASS(NTOFFSET(nt_rcvmsg)), WHEN MSG_IN


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

end:
	MOVE 0x00 TO SCNTL2
	CLEAR ACK
	WAIT DISCONNECT
	INT PASS(NINT_OK)


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; The data in and data out scatter/gather dma lists are set up by
; the driver such that the last entry is first in the table indirect
; array. In other words if the s/g list contains a single segment then
; only the first entry in the list is used. If the s/g list contains
; two entries then the second entry is used first, etc.. The jump table
; below skip over the unused entries. This way when a phase mismatch
; interrupt occurs I can easily compute how far into the list processing
; has proceeded and reset the pointers and the scratch register to
; properly restart the dma.

dataout_gotos:
	MOVE SCRATCHA0 TO SFBR
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

dataout_17:	MOVE FROM PASS(NTOFFSET(nt_data[16])), WHEN DATA_OUT
dataout_16:	MOVE FROM PASS(NTOFFSET(nt_data[15])), WHEN DATA_OUT
dataout_15:	MOVE FROM PASS(NTOFFSET(nt_data[14])), WHEN DATA_OUT
dataout_14:	MOVE FROM PASS(NTOFFSET(nt_data[13])), WHEN DATA_OUT
dataout_13:	MOVE FROM PASS(NTOFFSET(nt_data[12])), WHEN DATA_OUT
dataout_12:	MOVE FROM PASS(NTOFFSET(nt_data[11])), WHEN DATA_OUT
dataout_11:	MOVE FROM PASS(NTOFFSET(nt_data[10])), WHEN DATA_OUT
dataout_10:	MOVE FROM PASS(NTOFFSET(nt_data[9])), WHEN DATA_OUT
dataout_9:	MOVE FROM PASS(NTOFFSET(nt_data[8])), WHEN DATA_OUT
dataout_8:	MOVE FROM PASS(NTOFFSET(nt_data[7])), WHEN DATA_OUT
dataout_7:	MOVE FROM PASS(NTOFFSET(nt_data[6])), WHEN DATA_OUT
dataout_6:	MOVE FROM PASS(NTOFFSET(nt_data[5])), WHEN DATA_OUT
dataout_5:	MOVE FROM PASS(NTOFFSET(nt_data[4])), WHEN DATA_OUT
dataout_4:	MOVE FROM PASS(NTOFFSET(nt_data[3])), WHEN DATA_OUT
dataout_3:	MOVE FROM PASS(NTOFFSET(nt_data[2])), WHEN DATA_OUT
dataout_2:	MOVE FROM PASS(NTOFFSET(nt_data[1])), WHEN DATA_OUT
dataout_1:	MOVE FROM PASS(NTOFFSET(nt_data[0])), WHEN DATA_OUT
do_list_end:
	MOVE 0 TO SCRATCHA0
	JUMP REL(switch)

;
; data in processing
;

datain_gotos:
	MOVE SCRATCHA0 TO SFBR
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

datain_17:	MOVE FROM PASS(NTOFFSET(nt_data[16])), WHEN DATA_IN
datain_16:	MOVE FROM PASS(NTOFFSET(nt_data[15])), WHEN DATA_IN
datain_15:	MOVE FROM PASS(NTOFFSET(nt_data[14])), WHEN DATA_IN
datain_14:	MOVE FROM PASS(NTOFFSET(nt_data[13])), WHEN DATA_IN
datain_13:	MOVE FROM PASS(NTOFFSET(nt_data[12])), WHEN DATA_IN
datain_12:	MOVE FROM PASS(NTOFFSET(nt_data[11])), WHEN DATA_IN
datain_11:	MOVE FROM PASS(NTOFFSET(nt_data[10])), WHEN DATA_IN
datain_10:	MOVE FROM PASS(NTOFFSET(nt_data[9])), WHEN DATA_IN
datain_9:	MOVE FROM PASS(NTOFFSET(nt_data[8])), WHEN DATA_IN
datain_8:	MOVE FROM PASS(NTOFFSET(nt_data[7])), WHEN DATA_IN
datain_7:	MOVE FROM PASS(NTOFFSET(nt_data[6])), WHEN DATA_IN
datain_6:	MOVE FROM PASS(NTOFFSET(nt_data[5])), WHEN DATA_IN
datain_5:	MOVE FROM PASS(NTOFFSET(nt_data[4])), WHEN DATA_IN
datain_4:	MOVE FROM PASS(NTOFFSET(nt_data[3])), WHEN DATA_IN
datain_3:	MOVE FROM PASS(NTOFFSET(nt_data[2])), WHEN DATA_IN
datain_2:	MOVE FROM PASS(NTOFFSET(nt_data[1])), WHEN DATA_IN
datain_1:	MOVE FROM PASS(NTOFFSET(nt_data[0])), WHEN DATA_IN
di_list_end:
	MOVE 0 TO SCRATCHA0
	JUMP REL(switch)
