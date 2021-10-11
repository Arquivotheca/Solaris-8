/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * tty.h -- public definitions for err module
 */

#ifndef	_TTY_H
#define	_TTY_H

#ident "@(#)tty.h   1.15   97/08/27 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stdio.h>

#define	TTYCHAR_FKEY	0x0100
#define	TTYCHAR_FKEY_MSK 0xff
#define	FKEY(n)		((n) | TTYCHAR_FKEY)
#define	TTYCHAR_UP	0x1000
#define	TTYCHAR_DOWN	0x1001
#define	TTYCHAR_LEFT	0x1002
#define	TTYCHAR_RIGHT	0x1003
#define	TTYCHAR_PGUP	0x1004
#define	TTYCHAR_PGDOWN	0x1005
#define	TTYCHAR_HOME	0x1006
#define	TTYCHAR_END	0x1007
#define	TTYCHAR_BKTAB	0x1008

void init_tty(void);
void clear_tty(void);
void lico_tty(int line, int column);
void standout_tty(void);
void standend_tty(void);
void refresh_tty(int force);
int curco_tty(void);
int curli_tty(void);
int maxco_tty(void);
int maxli_tty(void);
int getc_tty(void);
int vprintf_tty(const char *fmt, va_list ap);
int printf_tty(const char *fmt, ...);
int iprintf_tty(const char *fmt, ...);
int viprintf_tty(const char *fmt, va_list ap);
int putc_tty(int c);
void iputc_tty(int c);
int bef_print_tty(int c);
void beep_tty(void);
char *keyname_tty(int c, int escmode);
void catcheye_tty(char *p);
void console_tty(void);

extern unsigned char *Cur_screen;
extern unsigned char *Ecur_screen;

extern int Script_line;		/* Input line counter */
extern FILE *Script_file;	/* Ptr to script file */
extern int Screen_active;	/* Non-zero if screen active */

extern int Bef_printfs_done_tty;

extern int done_init_tty;

#ifdef	__cplusplus
}
#endif

#endif	/* _TTY_H */
