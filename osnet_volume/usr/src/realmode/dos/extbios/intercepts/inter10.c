/*
 *	@(#)inter10.c	1.4
 */

#ifndef lint
static char *sccsid = "@(#)inter10.c	1.4";
#endif /* lint */

#include "chario.h"
#include "global.h"

#define STACKSIZE	1000	/* resident stack size in words */
u_short stack[STACKSIZE] = {0};
u_short start_stk[STACKSIZE] = {0};
u_short stacksize = STACKSIZE * sizeof(u_short);

int com_port = 1;
long oldvect = 0;
int tmpAX = 0;			/* ---- storage for _int86() ---- */

u_char
putser(char c)
{
	union _REGS inr;

	inr.h.ah = 0x01;
	inr.h.al = c;
	inr.x.dx = com_port;
	_int86(0x14, &inr, &inr);
	
	return inr.h.ah;
}

putnumber(u_short d, int base)
{
	char s[80];
	char *p = &s[79];
	int i;

	*p-- = '\0';		/* null terminate */
	do {
		i = d % base;
		d /= base;
		if (i > 9)
			*p-- = i - 10 + 'A';
		else
			*p-- = i + '0';
	} while (d);
	
	/* p has been backed up one two many chars */
	for (p++; *p != '\0'; p++)
	  putser(*p);
}

/*
 * Use the follow escape sequence for row & col
 *	\033[%d;%dH
 * this is a vt100 attribute.
 * NOTE: The vt100 is 1 based for row and column. The pc's row and column
 * are 0 based. That's why we're adding 1 to the row and column.
 */
putcurmotion (u_char row, u_char col)
{
	putser('\033'); putser('[');
	putnumber((u_short)row + 1, 10);
	putser(';');
	putnumber((u_short)col + 1, 10);
	putser('H');
}

char serial_clear_str[] = "\033[;H\033[2J";

/*
 *[]------------------------------------------------------------[]
 * | CAUTION:							|
 * | Do not change the value of any of these arguments if you	|
 * | expect to pass on the function. These args live on the 	|
 * | stack which is passed onto the next guy and you can change	|
 * | his outlook on life.					|
 *[]------------------------------------------------------------[]
 */
resmain(u_short ax, u_short bx, u_short cx, u_short dx, u_short si, u_short di, u_short es, u_short ds, u_short bp)
{
	union _REGS inr;
	union {
		struct {
			u_short l;
			u_short h;
		} s;
		u_char _far *cp;
		u_short _far *sp;
	} fp;
	u_short _far *sp;
	char _far *p;
	char *lp;
	int i, Rtn;
	long savevec;
	extern int sssave, sosave;

	/* ----
	 * Each of the functions except 13h will do a passon to the BIOS
	 */
	Rtn = 0;

	inr.x.ax = ax;

	/* ----
	 * look at the function number in ah and perform the task
	 * requested.
	 */
	switch (inr.h.ah) {
	      case 0x00:
		/* ----
		 * int 10h func 00h:
		 * reset the video screen.
		 * We'll just send the vt100 escape sequence for that.
		 * NOTE: IBM standard modes do not clear the screen if the high
		 *       bit of AL is set.
		 */
		if ((inr.h.al & 0x80) == 0) {
			for (lp = serial_clear_str; *lp != '\0'; lp++)
			  putser(*lp);
		}
		break;

	      case 0x02:
	      	/* ----
		 * int 10h func 002h:
		 * set the cursor postition. row is in dh and column is
		 * in dl.
		 */
		/* ---- position the cursor ---- */
		inr.x.dx = dx;
 		putcurmotion(inr.h.dh, inr.h.dl);
		break;

	      case 0x09:
		/* ----
		 * int 10h func 09h:
		 * write character and attribute at cursor position.
		 * the character is written cx times.
		 */
		if ((cx >= 2000) && (inr.h.al == 0x20)) {
			/* ---- special case a screen clear used in ufsboot */
			for (lp = serial_clear_str; *lp != '\0'; lp++)
			  putser(*lp);
		}
		else {
			for (i = 0; i < cx; i++)
			  putser(inr.h.al);
		}
		break;
		
	      case 0x0e:
		/* ----
		 * int 10h func 0eh:
		 * output character to console. the character in question is in
		 * the al register.
		 */
		putser(inr.h.al);
		break;

	      case 0x13:
		/* ----
		 * int 10h func 13h:
		 * output string to console at row, col with attr.
		 * We just worry about sending the row and col stuff to the
		 * serial port along with the string.
		 */
		/* ----
		 * NOTE: Bit 0 of the al register indicates that we should
		 * update the cursor after writting the string.
		 * Currently I'm not doing this because it really screws
		 * up the display.
		 */

		/* ---- position the cursor on the serial line ---- */
		inr.x.dx = dx;
 		putcurmotion(inr.h.dh, inr.h.dl);

		/* ---- setup the far pointer to the string ---- */
		fp.s.l = bp;
		fp.s.h = es;
		p = fp.cp;

		/* ---- display string on serial port ---- */
		for (i = cx; i; i--) {
			putser(*p++);
		}

		/* ----
		 * Orignally I had just passed this function onto the BIOS
		 * until I discovered not all BIOS's are created equal.
		 * On our CompuAdd machine int10h func 13h behaves as I would
		 * expect. On my Dell machine int10h func 13h behaves a little
		 * different. It breaks down the function by first doing a
		 * 13h02h and then a 13h0eh for each characters. This had the
		 * effect of duplicating the each string. By breaking down
		 * the function myself I know what it going on.
		 */
		getvec(0x10, &savevec);
		setvec(0x10, oldvect);

		/* ---- set the cursor ---- */
		inr.x.dx = dx;
		inr.h.ah = 2;
		inr.x.bx = bx;
		_int86(0x10, &inr, &inr);

		/* ---- get the string pointer again ---- */
		p = fp.cp;
		for (i = cx; i; i--) {
			inr.h.ah = 0x0e;
			inr.h.al = *p++;
			inr.x.bx = bx;
			inr.h.bl = 0;
			_int86(0x10, &inr, &inr);
		}

		/* ---- restore our vector ---- */
		setvec(0x10, savevec);

		Rtn = 1;

		break;
	}

	return Rtn;
}
	
/*
 *[]------------------------------------------------------------[]
 * | Code and data after this point is part of the non-resident	|
 * | portion.These limits are defined by the names "enddat" and	|
 * | "initmain" so do not change anything between here and	|
 * | initmain.							|
 *[]------------------------------------------------------------[]
 */
char enddat = 0;

initmain()
{
	unsigned short codesize, datasize, totalsize;
	unsigned short codeseg;
	extern newvect();
	
	putstr("Intercept10 initmain\r\n");

	if (!init_serial()) {
		putstr("Failed to initialize COM ports\r\n");
		return -1;
	}
	putstr("COM port "); puthex(com_port); putstr("\r\n");
	
	/* Read the int10 vector into oldvect */
	getvec(0x10, &oldvect);
	
	/* Calculate resident code size in bytes */
	codesize = (unsigned short)initmain;
	
	/* Calculate resident data size in bytes */
	datasize = (unsigned short)&enddat;
	
#ifdef DEBUG
	putstr("datasize ");
	puthex(datasize);
	putstr(", endstr ");
	putstr(endstr);
	putstr("\r\n");
#endif
	
	/* Calculate total resident size in K */
	totalsize = (((codesize + 15) >> 4) + ((datasize + 15) >> 4) + 63) >> 6;
	
	if (memsize() < totalsize) {
		printf("Not enough memory to install Intercept10\n");
		return (-1);
	}
	codeseg = reserve(totalsize);
	
	/* ---- pass the segment along that holds the data ---- */
	hidata(codeseg);
	
	/* Copy code and data parts into high memory */
	bcopy(MK_FP(codeseg, 0),  MK_FP(mycs(), 0), codesize + datasize);
	
	putstr("Intercept10 BIOS extension installed at ");
	puthex(codeseg);
	putstr(" resident size ");
	puthex(totalsize);
	putstr(".\r\n\n");
	
	/* Intercept int13 calls */
	setvec(0x10, newvect, codeseg);
	
	/* Return last device number to extended BIOS monitor */
	return -1;
}

init_serial()
{
	union _REGS inr;
	char *lp;

	do {
		inr.h.ah = 00;		/* Initialize */
		/* ---- One speed for now ---- */
		inr.h.al = S9600 | PARITY_NONE | STOP_1 | DATA_8;
		inr.x.dx = com_port;
		_int86(0x14, &inr, &inr);

		if (putser(' ') == 0x80)
		  com_port--;
		else
		  break;
	} while (com_port != -1);

	if (com_port != -1) {
		for (lp = serial_clear_str; *lp != '\0'; lp++)
		  putser(*lp);
		return 1;
	}
	else
	  return 0;
}
