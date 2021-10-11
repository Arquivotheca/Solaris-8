/*
 * Copyright (c) 1993, 1994 Sun Microsystems, Inc. All rights reserved.
 */

#ident	"@(#)bioserv.h	1.8	94/05/23 SMI\n"

/*
 * Solaris Primary Boot Subsystem - Support Library Header
 *
 *   File name:		bioserv.h
 *
 *   Description:	this file contains declarations and definitions used
 *			by the set of functions that provide system services
 *			during the Solaris primary boot.
 *
 *			These services are implemented through INT calls to
 *			the IBM PC ROM BIOS.
 *
 */

#ifndef _BIOSERV_H
#define _BIOSERV_H

#ifdef FARDATA
#define _FAR_ _far
#else
#define _FAR_
#endif

#ifdef FARCODE
#define _FARC_ _far
#else
#define _FARC_
#endif

extern short  tmpAX, tmpDX;
extern void  putchar ( char );
extern void  putstr ( char * );
extern void  _FARC_ prtstr_pos ( char _FAR_ *, short, short, short, short );
extern void  _FARC_ prtstr_attr ( char _FAR_ *, short, short, short, short, short );
extern void  printf ( char _FAR_ *, ... );
extern void  vprintf ( char _FAR_ *, ... );
extern char _FAR_ * sprintf ( char _FAR_ *, char _FAR_ *, ... );

extern void  _FARC_ clr_screen ( );
extern void  _FARC_ clr_screen_attr ( short );
extern short _FARC_ ask_page ( );
extern void  _FARC_ set_page ( short );
extern short _FARC_ read_cursor ( short );
extern void  _FARC_ set_cursor ( short, short, short );
extern void  _FARC_ wait_key ( );
extern short _FARC_ read_key ( );
extern short _FARC_ nowait_read_key ( );
extern short _FARC_ check_key_input ( );
extern short _FARC_ flush_kb ( );
extern short _FARC_ reset_disk ( short );
extern short _FARC_ ask_disk ( short, char _FAR_ * );
/*	read_disk() always uses far buffer pointer	*/
extern short _FARC_ read_disk ( short, short, short, short, short, char _far * );
extern long  _FARC_ ask_time ( );
extern long  _FARC_ pause_ms ( short );

extern long _FARC_ strlen ( char _FAR_ * );
extern long _FARC_ strcmp ( char _FAR_ *, char _FAR_ * );
extern long _FARC_ strncmp ( char _FAR_ *, char _FAR_ *, short );
extern long _FARC_ strcspn ( char _FAR_ *, char _FAR_ * );
extern void _FARC_ bcopy ( char _FAR_ *, char _FAR_ *, long );
extern void _FARC_ farbcopy ( char _far *, char _far *, long );
/* *ALWAYS* far! use farbcopy for inter-segment transfers... */

extern void _FARC_ testpt ( char _FAR_ * );
extern void _FARC_ bzero ( char _FAR_ *, short );
extern char _FAR_ * _FARC_ strcat ( char _FAR_ *, char _FAR_ * );
extern char _FAR_ * _FARC_ strncat ( char _FAR_ *, char _FAR_ *, short );
extern char _FAR_ * _FARC_ strcpy ( char _FAR_ *, char _FAR_ * );
extern char _FAR_ * _FARC_ strncpy ( char _FAR_ *, char _FAR_ *, short );
extern char _FAR_ * _FARC_ memset ( char _FAR_ *, short, long );

extern short _FARC_ atoi ( char _FAR_ * );
extern long  _FARC_ atol ( char _FAR_ * );
extern char _FAR_ * _FARC_ itoa ( short, char _FAR_ * );
extern char _FAR_ * _FARC_ xtoa ( unsigned long, char _FAR_ * );

extern short _FARC_ rs_shift ( short, short );
extern long  _FARC_ rl_shift ( long, short );
extern short _FARC_ ls_shift ( short, short );
extern long  _FARC_ ll_shift ( long, short );
extern long  _FARC_ us_div ( unsigned long, unsigned short );
extern short _FARC_ us_mod ( unsigned long, unsigned short );
extern long  _FARC_ ul_mul ( unsigned long, unsigned long );

extern long open ( char _FAR_ * );
extern long read ( long, char _FAR_ *, long );
extern void seek ( long, long );       /* second long is actually an off_t */
extern long close ( long );

/* <<<<<<<<<<    standard DOS device names    >>>>>>>>>> */
#define Drive_A 0x0		/* first floppy diskette drive */
#define Drive_B 0x1		/* second floppy diskette drive */
#define Drive_C 0x80		/* first hard disk drive */
#define Drive_D 0x81		/* second hard disk drive */

struct KEYCODES {
	char keycode;
	char scancode;
};

union key_t {
	short retcode;
	struct KEYCODES k;
};

struct FDCODES {
	char nsectors;		/* only floppy returns number of sectors read */
	char fd_rc;
};

union fdrc_t {			/* only used for floppy diskette reads. */
	short retcode;
	struct FDCODES f;
};


typedef struct {                    /* a typical DOS directory entry */
               char fname[8];
               char ext[3];
               char attr;
               char res[10];
               unsigned short uptime;
               unsigned short update;
               unsigned short start;
               unsigned long  size;
               } DOSdirent;

#endif            /* _BIOSERV_H */
