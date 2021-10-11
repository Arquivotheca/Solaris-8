/*
 *  Copyright (c) 1997, by Sun Microsystems, Inc.
 *  All rights reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)befdebug.h	1.9	97/10/20 SMI"
 */

/*
 *	Header file for common definitions for befdebug source files.
 */

#ifndef _BEFDEBUG_H_
#define	_BEFDEBUG_H_

/* This is where DEBUG should be defined when required */
/* #define	DEBUG */

#include <rmscnet.h>
#include <rmscscsi.h>

/*
 * When debugging we always want "Static" to be null.
 */
#ifdef DEBUG
#ifdef Static
#undef Static
#endif
#define	Static
#endif

#define	TEST_NONE	0
#define	TEST_PROBE	1
#define	TEST_INSTALL	2
#define	SAVE_DATA	3

#define	NAME_SIZE 16
struct node_data {
	char name[NAME_SIZE];
	ushort len;
	ulong val[MaxTupleSize];
};

/*
 *	This structure is used to encapsulate as much as possible of the
 *	data that is written into an installed befdebug by a new befdebug.
 *	Any changes to this structure, other than changing MAX_NODES, might
 *	prevent passing data to an old version.
 *
 *	user_debug_flag is for use with debugging drivers and is invoked
 *	by driver developers using the befdebug -d option.  At present we
 *	do not support different levels or classes of user debugging.
 *
 *	dev_debug_flag is for use with Dprintf for debugging befdebug
 *	itself when DEBUG is defined.  Note that the definition is kept in
 *	the structure at all times so that a non-DEBUG befdebug can write
 *	data into an installed DEBUG version and vice versa.
 */
#define	MAX_NODES	100
struct befdebug_data {
	ushort table_max;
	ushort struct_size;
	ushort table_size;
	ushort function;
	ushort exercised;
	ushort node_called;
	ushort misc_flags;
	ushort pause_count;		/* For controlling output */
	ushort user_debug_flag;		/* Used with -d option */
	ushort dev_debug_flag;		/* Used with #ifdef DEBUG */
	struct node_data node_tab[MAX_NODES];
};

/* Bits defined for misc_flags */
#define	HIDE_FLAG	1
#define	IGNORE_REVARP	2
#define	IGNORE_WHOAMI	4

#ifdef DEBUG
#ifndef DEBUG_FLAG
#define	DEBUG_FLAG	0
#endif

#define	MODULE_DEBUG_FLAG bd.dev_debug_flag
#endif

#define	UDprintf(x)	if (bd.user_debug_flag) printf x

extern struct befdebug_data bd;

extern char *prog_name;

extern int callback_state;
#define	CALLBACK_FORBIDDEN	0
#define	CALLBACK_ALLOWED	1

/* External routines in befdebug.c */
extern int far_strcmp(char far *, char far *);
extern void my_printf();
extern int char_from_driver(int);
extern void set_name(char *, char far *);

/* External routines in exercise.c */
extern void net_addr(unchar *);
extern void net_close(void);
extern void net_open(void);
extern int net_receive_packet(unchar *, ushort);
extern void net_send_packet(unchar *, ushort);
extern void test_bef(ushort, ushort, ushort);

/* External routines in netex.c */
extern void net_exercise(int, struct bdev_info far *, char far *);

/* External routines in node.c */
extern int node(int);
extern ushort node_count(void);
extern void node_reset(void);
extern int resource(int, char far *, DWORD far *, DWORD far *);

/* External routines in doswrap.c */
extern ushort dos_alloc(ushort, ushort *);
extern void dos_close(void *);
extern int dos_exec(char *, char *);
extern int dos_exit(unsigned short);
extern int dos_fprintf();
extern ushort dos_free(ushort);
extern char *dos_get_line(char *, int, void *);
extern unsigned short dos_get_psp(void);
extern unsigned short dos_get_size(unsigned short);
extern int dos_kb_char(void);
extern ushort dos_mem_adjust(ushort, ushort, ushort *);
extern int dos_memcmp(void far *, void far *, unsigned);
extern void dos_memcpy(void far *, void far *, unsigned);
extern void dos_memset(void far *, int, unsigned);
extern void dos_msecwait(unsigned long);
extern void *dos_open(char *, char *);
extern int dos_put_line(char *, void *);
extern int dos_strcmp(char *, char *);
extern void dos_strcpy(char *, char *);
extern void dos_usecwait(unsigned long);
extern int dos_write(int, char *, int);

/* DOS library routines */
extern void exit();
extern int printf();
extern int sprintf();
extern int strlen();
extern int strncmp();
extern char *strtok();

#endif /* _BEFDEBUG_H_ */
