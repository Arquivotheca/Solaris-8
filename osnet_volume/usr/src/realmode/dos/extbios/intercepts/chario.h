/*
 *[]------------------------------------------------------------[]
 * | This file contains all character i/o related structures	|
 * | and defines.						|
 * |								|
 * | Project: Boot Hill						|
 * | Author : Rick McNeal					|
 * | Date   : 2-Nov-1994					|
 *[]------------------------------------------------------------[]
 */

struct _char_io_ {
	struct _char_io_	*next;		/* next i/o member */
	char	*name;				/* Name for stats */
	int	in, out, errs;			/* Simple stats */
	int	flags;				/* control bits */
	char	*cookie;			/* for driver use */
	char	(*getc)(struct _char_io_ *);	/* returns character */
	void	(*putc)(struct _char_io_ *, char); /* outputs one character */
	int	(*avail)(struct _char_io_ *);	/* returns 1 if char avail */
	void	(*clear)(struct _char_io_ *);	/* clear screen */
	void	(*set)(struct _char_io_ *, int, int); /* set cursor pos. */
};
typedef struct _char_io_ _char_io_t, *_char_io_p;

#define CHARIO_IGNORE		0x0001	/* don't output to this dev */
#define CHARIO_DISABLED		0x0002	/* error occured and ports not used */

/*
 * Use this macro when debugging the output side of your driver. This
 * will prevent any printf's from your driver causing an infinite loop
 */
#define PRINT(p, x) \
{ p->flags |= CHARIO_IGNORE; Print x; p->flags &= ~CHARIO_IGNORE; }

/*[]------------------------------------------------------------[]
   | Defines for the serial port				|
  []------------------------------------------------------------[]*/
/* ---- Bits 5-7 define baud rate ---- */
#define S110		0x00
#define S150		0x20
#define S300		0x40
#define S600		0x60
#define S1200		0x80
#define S2400		0xa0
#define S4800		0xc0
#define S9600		0xe0

/* ---- Bits 3 & 4 are parity ---- */
#define PARITY_NONE	0x10
#define PARITY_ODD	0x08
#define PARITY_EVEN	0x18

/* ---- Bit 2 is stop bit ---- */
#define STOP_1		0x00
#define STOP_2		0x04

/* ---- Bits 0 & 1 are data bits ---- */
#define DATA_8		0x03
#define DATA_7		0x02
#define DATA_6		0x01
#define DATA_5		0x00

/*[] ---- Line Status ---- []*/
#define SERIAL_TIMEOUT	0x80
#define SERIAL_XMITSHFT	0x40
#define SERIAL_XMITHOLD	0x20
#define SERIAL_BREAK	0x10
#define SERIAL_FRAME	0x08
#define SERIAL_PARITY	0x04
#define SERIAL_OVERRUN	0x02
#define SERIAL_DATA	0x01
