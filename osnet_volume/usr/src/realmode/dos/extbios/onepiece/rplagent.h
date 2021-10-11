/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */
 
#ident	"@(#)rplagent.h	1.12	95/11/03 SMI\n"
 

/* #define SVR_INT_NUM		0xFA */

#define NameName	"name"
#define SlotName	"slot"
#define PortName	"port"
#define MemName		"mem"
#define IrqName		"irq"
#define DmaName		"dma"

#define InstallSpace(Res, Size) \
  DWORD Res##_Space[Size * Res##TupleSize] = { Size * Res##TupleSize }

#define	RINGBUFFER /* */

/* Request jump table indices */
#define ADAPTER_INIT  		0x00
#define ADAPTER_OPEN		0x01
#define ADAPTER_CLOSE		0x02
#define ADAPTER_SEND		0x03
#define ADAPTER_RECEIVE		0x04
#define ADAPTER_INFO		0x05

/* Methods of returning to service user */
#define IRET_RETURN			0
#define FAR_RETURN			1
#define NEAR_RETURN			2

/* Return codes from AdapterIdentify */
#define ADAPTER_TABLE_END			-1
#define ADAPTER_TABLE_OK			1
#define ADAPTER_NOT_FOUND			0
#define ADAPTER_FOUND				1
#define BOOT_ROM_INSTALLED			2

/* Different frame types */
#define FIND_FRAME			0x01
#define FOUND_FRAME			0x02
#define SENDFILE_FRAME		0x10
#define FILEDATA_FRAME		0x20
#define LOADERROR_FRAME		0x40
#define PRGALERT_FRAME		0x30

/* Distinguish different stages of booting */
#define RPL_TRAFFIC			0x10
#define INETBOOT_TRAFFIC	0x11

/* Distinguish different physical media */
#define MEDIA_ETHERNET		0x10
#define MEDIA_IEEE8025		0x11

struct WORDREGS {
	unsigned int ax;
	unsigned int bx;
	unsigned int cx;
	unsigned int dx;
	unsigned int si;
	unsigned int di;
	unsigned int cflag;
	};


/* byte registers */

struct BYTEREGS {
	unsigned char al, ah;
	unsigned char bl, bh;
	unsigned char cl, ch;
	unsigned char dl, dh;
	};


/* general purpose registers union -
 *  overlays the corresponding word and byte registers.
 */

union REGS {
	struct WORDREGS x;
	struct BYTEREGS h;
	};


/* segment registers */

struct SREGS {
	unsigned int es;
	unsigned int cs;
	unsigned int ss;
	unsigned int ds;
	};

