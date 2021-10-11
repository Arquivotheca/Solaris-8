/*
 * Copyright (c) 1992-1994,1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _I386_SYS_BOOTLINK_H
#define	_I386_SYS_BOOTLINK_H

#pragma ident	"@(#)bootlink.h	1.9	98/01/08 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 *	Structure for BIOS int call requests. Register values set by
 *	the caller and modified to the value after the int call by the
 *	int service routine.
 */

struct int_pb {
	ushort	intval, 	/* INT value to make int x instruction */
		ax, bx,		/* input and returned */
		cx, dx,
		bp, es,
		si, di, ds;
};

/*
 *	Structure for int 15h BIOS call with 32 bit registers. Register values
 *	set by the caller and modified to the value after the int call
 *	by the int service routine.
 */

struct int_pb32 {
	ulong	eax, ebx,	/* 32 bits registers input and returned */
		ecx, edx,
		esi, edi;
	ushort	bp, es,
		ds;
};

/*
 *	More complete structure for BIOS int call requests. A pointer
 *	to one of these structures will be passed to do_int(), so that
 *	do_int() can be re-entrant.
 */
struct real_regs {
	union {
		ulong	eax;
		struct {
			ushort	ax;
		} word;
		struct {
			unchar	al;
			unchar	ah;
		} byte;
	} eax;
	union {
		ulong	edx;
		struct {
			ushort	dx;
		} word;
		struct {
			unchar	dl;
			unchar	dh;
		} byte;
	} edx;
	union {
		ulong	ecx;
		struct {
			ushort	cx;
		} word;
		struct {
			unchar	cl;
			unchar	ch;
		} byte;
	} ecx;
	union {
		ulong	ebx;
		struct {
			ushort	bx;
		} word;
		struct {
			unchar	bl;
			unchar	bh;
		} byte;
	} ebx;
	union {
		ulong	ebp;
		struct {
			ushort bp;
		} word;
	} ebp;
	union {
		ulong	esi;
		struct {
			ushort si;
		} word;
	} esi;
	union {
		ulong	edi;
		struct {
			ushort di;
		} word;
	} edi;
	union {
		ulong	esp;
		struct {
			ushort sp;
		} word;
	} esp;

	ushort	cs;
	ushort	ss;
	ushort	ds;
	ushort	es;
	ushort	fs;
	ushort	gs;

	ulong	eflags;
	ushort	ip;
};

/* a "paragraph" is 16 bytes */
#define	PARASIZE	16

/*
 *	Defines for changing FLAGS register components.
 */
#define	CARRY_FLAG	0x1
#define	ZERO_FLAG	0x40

#ifdef	__cplusplus
}
#endif

#endif	/* _I386_SYS_BOOTLINK_H */
