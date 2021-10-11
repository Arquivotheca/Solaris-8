#ifndef _BIOSMAP_H
#define	_BIOSMAP_H

#ident "@(#)biosmap.h   1.10   97/03/25 SMI"

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc
 * All Rights Reserved.
 */

/*
 * BIOS Data Area Map:
 *
 * This file contains a structure definition that maps the BIOS data
 * area at 0x40:0x00.  We also include a macro definition of a pointer
 * to this area ("bdap") so that fields may be accessed without
 * introducing extraneous local variables.
 */

#ifndef __lint
typedef	unsigned char u_char;
typedef	unsigned int u_int;
typedef	unsigned long u_long;
#endif

#include <dostypes.h>
#pragma pack(1)

struct biosmap
{
	u_int  ComPort	   [4];	/* 00 I/O addresses for serial ports */
	u_int  LptPort	   [3];	/* 08 I/O addresses for parallel ports */
	u_int  ExtDataSeg;	/* 0E Segment addr of extended data */

	u_int  BootFloppy   :1;	/* 10 Diskette available for boot */
	u_int  FpuPresent   :1;	/*    Math co-processor is present */
	u_int  MousePresent :1;	/*    Pointing device is present */
	u_int		    :1;
	u_int  InitVideoMode:2;	/*    Initial video mode: */
	u_int  DiskDriveCnt :2;	/*    Number of disk drives (less 1) */
	u_int		    :1;
	u_int  SerialPorts  :3;	/*    Number of serial ports */
	u_int		    :2;
	u_int  ParallelPorts:2;	/*    Number of parallel ports */

	u_char MfgrTest;	/* 12 Reserved for Manufaturer testing */
	u_int  RealMemSize;	/* 13 Size of realmode memory (klicks) */
	u_int  ErrCodes;	/* 15 Error codes */
	u_char KeyShiftFlgs[2];	/* 17 Keyboard shift flags */
	u_char AltKey;		/* 19 Work area for alt/numeric keys */
	u_int  KeyBufPtrs  [2];	/* 1A Next/last input char pointers */
	u_int  KeyBuffer  [16];	/* 1E Keyboard buffer */

	u_int  Recalibrate0 :1;	/* 3E Recalibrate diskette 0 */
	u_int  Recalibrate1 :1;	/*    Recalibrate diskette 1 */
	u_int		    :5;
	u_int  FloppyInt    :1;	/*    Diskette interrupt pending */
	u_int  FloppyMotor0 :1;	/*    Diskette 0 spinning */
	u_int  FloppyMotor1 :1;	/*    Diskette 1 spinning */
	u_int		    :2;
	u_int  DriveSelect  :2;	/*    Drive select state */
	u_int		    :1;
	u_int  WriteOp	    :1;	/*    Write/format operation if set */
	u_char FloppyTimeOut;	/* 40 Diskette motor timeout count */
	u_char FloppyError  :5;	/* 41 Diskette error code */
	u_char CtrlFailure  :1;	/*    Diskette controller failure */
	u_char SeekFailure  :1;	/*    Diskette seek failure */
	u_char NotReady	    :1;	/*    Diskette not ready flag */
	u_char CtrlStatus  [7];	/* 42 Diskette controller status */

	u_char VideoMode;	/* 49 Video mode setting */
	u_int  Columns;		/* 4A Number of columns on screen */
	u_int  CurPageSize;	/* 4C Current page size (bytes) */
	u_int  CurPageAddr;	/* 4E Current page address */
	u_int  CursorPos   [8];	/* 50 Cursor position for each page */
	u_int  CursorType;	/* 60 Cursor type (6845 scan lines) */
	u_char CurPageIndex;	/* 62 Current display page */
	u_int  VideoPort;	/* 63 I/O port number for current mode */
	u_char CurModeSelect;	/* 65 Current mode select register */
	u_char CurPallete;	/* 66 Current pallete value */

	void far *OptionROM;	/* 67 Ptr to next option ROM */
	u_char LastInt;		/* 6B Last interrupt type */
	u_long TimerCount;	/* 6C Timer count */
	u_char TimerOvflow;	/* 70 24-hour timer rollover */
	u_char CtlBreak;	/* 71 Ctrl/break flag */
	u_int  ResetFlag;	/* 72 Reset/reboot flag */

	u_char DiskStatus;	/* 74 Status of last fixed disk op */
	u_char HardDrives;	/* 75 Number of hard drives (ide) */
	u_char DiskControl;	/* 76 Hard disk control byte */
	u_char DiskPortOffset;	/* 77 Hard disk port offset */
	u_char LptTimeOut  [4];	/* 78 Parallel port timeouts */
	u_char ComTimeOut  [4];	/* 7C Serial port timeouts */
	u_int  KeyStartOff;	/* 80 Offset to start of keyboard buf */
	u_int  KeyEndOff;	/* 82 OFfset to end of keyboard buffer */

	u_char VidNoRows;	/* 84 Video No of rows */
	u_int  VidpixsPerChar;	/* 85 Video pixels per chararcter */
	u_char CGAcursorEmul:1;	/* 87 CGA Cursor Emulation */
	u_char MonoChrome   :1; /*    Monochrome monitor attached */
	u_char		    :6;
	u_char VideoSwitches;	/* 88 Video switches */
	u_char VideoSaveArea1;	/* 89 Video save area 1 */
	u_char VideoSaveArea2;	/* 8A Video save area 2 */
	u_char		    :2;	/* 8B reserved */
	u_char FdXferRate   :2;	/*    Diskette transfer rate */
	u_char FdStepRate   :2;	/*    Last selected step rate */
	u_char FdDataRate   :2;	/*    Last selected data rate */
	u_char HdStatusReg;	/* 8C Copy of fixed disk status reg */
	u_char HdErrorReg;	/* 8D Copy of fixed disk error reg */
	u_char HdIntrFlag;	/* 8E Fixed disk interrupt flag	*/
	u_char Fd0ChangeLine:1;	/* 8F Diskette supports change line */
	u_char Fd0Multirate :1;	/*    Multirate diskette if set	*/
	u_char Fd0Determined:1;	/*    Diskette type determined */
	u_char		    :1;
	u_char Fd1ChangeLine:1;	/*    Same stuff for 2nd floppy ... */
	u_char Fd1Multirate :1;
	u_char Fd1Determined:1;
	u_char		    :1;
	u_char MediaType   [2];	/* 90 FD media type info (per drive) */
	u_char MediaWrkArea[2];	/* 92 FD media work area (per drive) */
	u_char FdTrackNum  [2];	/* 94 FD track number (per drive) */

	u_char  KeyE1	    :1;	/* 96 Last key code was 0xE1 */
	u_char KeyE0	    :1;	/*    Last key code was 0xE0 */
	u_char RiteCtlDown  :1;	/*    Control key is active */
	u_char RiteAltDown  :1;	/*    Alt key is active */
	u_char FunctionKeys :1;	/*    101/102 keyboard installed */
	u_char ForceNumLock :1;	/*    Force num lock at boot */
	u_char FirstID	    :1;	/*    Last code was first ID */
	u_char ReadID	    :1;	/*    Read ID in progress */
	u_char ScrollLock   :1;	/* 97 Scroll lock LED on */
	u_char NumLock	    :1;	/*    Num lock LED on */
	u_char CapsLock	    :1;	/*    Caps lock LED on */
	u_char		    :1;
	u_char AckReceived  :1;	/*    Keyboard ACK received */
	u_char ResendRecvd  :1;	/*    Resend command received */
	u_char LEDUpdate    :1;	/*    LED update in progress */
	u_char KeyboardErr  :1;	/*    Keyboard error flag */

	void far *UserWaitFlag;	/* 98 Ptr to user wait flag */
	u_long WaitCount;	/* 9C Wait count word */
	u_int  Int15_86	    :1;	/* A0 INT 0x15/0x86 condition */
	u_int		    :6;
	u_int  WaitTimeOut  :1;	/*    Wait time elapsed flag */
	u_char LocalAreaNet[7]; /* A1 Local area network bytes */
	u_long VideoParamBlk;	/* A8 Video Parameter control block ptr	*/
	u_char Reserved   [34]; /* AC Reserved */
	u_int  Clock;		/* CE Clock - days since 1980 */
	u_char Reserved2  [48]; /* D0 Reserved */
	u_char PrintStatus;	/* 100 Print screen status byte */
};

#pragma pack()
#define	bdap ((struct biosmap far *)0x00400000) /* Ptr to BIOS data area */
#endif
