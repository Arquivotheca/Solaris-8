/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc.  All rights reserved.
 *
 *
 * Solaris Primary Boot Subsystem - Common header
 *===========================================================================
 *    File name: common.h
 *
 */
 #ident	"@(#)common.h	1.7	99/07/20 SMI\n"


/* Handy macros for pointer arithmetic */

#define MK_FP(seg, off) (void __far *) \
		( (((unsigned long)(seg)) << 16) | (unsigned long)(off))

#define FP_OFF(fp) (((unsigned long)(fp)) & 0xFFFF)
#define FP_SEG(fp) ((((unsigned long)(fp)) >> 16) & 0xFFFF)

/* Routines defined in this directory */

ushort mycs(void);
ushort myds(void);
void intr_disable(void);
void intr_enable(void);
void outb(ushort port, unchar data);
void outw(ushort port, ushort data);
ushort inb(ushort port);
unsigned short inw(ushort port);
void bcopy(char __far *dest, char __far *src, short bytecount);
ushort __ntohs(ushort s);
char __far *get_sysconf(void);
int is_eisa(void);
int kbchar(void);
void putnum(short n);
void puthex(ushort data);
void put2hex(unchar data);
void put1hex(unchar data);
void putstr(char *s);
void putptr(void __far *p);
void milliseconds(unsigned short millis);
void hang(void);
void reboot(void);
