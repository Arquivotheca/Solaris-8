/*
 *	@(#)inter16.c	1.4
 */

#ifndef lint
static char *sccsid = "@(#)inter16.c	1.4";
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

/*
 *[]------------------------------------------------------------[]
 * | CAUTION:							|
 * | Do not change the value of any of these arguments if you	|
 * | expect to pass on the function. These args live on the 	|
 * | stack which is passed onto the next guy and you can change	|
 * | his outlook on life.					|
 *[]------------------------------------------------------------[]
 */
int start_dbg = 0;
resmain(u_short ax, u_short bx, u_short cx, u_short dx, u_short si, u_short di, u_short es, u_short ds, u_short bp)
{
	union _REGS inr;
	int Rtn;
	long savevector;

	inr.x.ax = ax;
	Rtn = 0;

	/* ----
	 * look at the function number in ah and perform the task
	 * requested.
	 */
	switch (inr.h.ah) {
	      case 0x00:
		/* ----
		 * This requires a little be of work to complete
		 * this function. We need to loop checking for characters
		 * on both the serial line and the keyboard. Once a character
		 * has been detected will read it and return that value.
		 * The tricky part is that our newvect code isn't reentrant
		 * and the way to get around that is to reinstall the orignal
		 * interrupt vector, do the keyboard request, and then
		 * restore our vector.
		 */
		do {
			inr.h.ah = 0x03;		/* Get port status */
			inr.h.al = 0x00;
			inr.x.dx = com_port;
			_int86(0x14, &inr, &inr);
			if (inr.h.ah & SERIAL_DATA) {
				inr.h.ah = 0x02;	/* Read character */
				inr.h.al = 0x00;
				inr.x.dx = com_port;
				_int86(0x14, &inr, &inr);
				if (!(inr.h.ah & 0x80)) {
					ax = inr.x.ax;
					Rtn = 1;
				}
			}
			else {
				getvec(0x16, &savevector);
				setvec(0x16, oldvect);
				inr.h.ah = 0x01;
				_int86(0x16, &inr, &inr);
				if (!(inr.x.flag & ZERO_FLAG)) {
					inr.h.ah = 0x00;
					_int86(0x16, &inr, &inr);
					ax = inr.x.ax;
					Rtn = 1;
				}
				setvec(0x16, savevector);
			}
		} while (Rtn == 0);
		break;

	      case 0x01:
		/* ----
		 * Check for a charater on the serial ports. For once the
		 * BIOS folks got something right it would seem. The Zero
		 * flag is set if no keystroke is available. Therefore
		 * if we find a character on the serial port we just return
		 * without passing onto the next vector. If we don't find
		 * a character we jump to the next vector and let them set
		 * the flag.
		 */
		inr.h.ah = 0x03;		/* Get port status */
		inr.h.al = 0x00;
		inr.x.dx = com_port;
		_int86(0x14, &inr, &inr);
		if (inr.h.ah & SERIAL_DATA)
		  Rtn = 1;
		else
		  Rtn = 0;	/* sets the carry and zero bits in flags */
		
		break;
		
	      default:
		/* ---- pass the function requestion onto the BIOS ---- */
		Rtn = 0;
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
	
	putstr("Intercept16 initmain\r\n");

	if (!init_serial()) {
		putstr("Failed to init COM ports\r\n");
		return -1;
	}
	putstr("COM port "); puthex(com_port); putstr("\r\n");
	
	/* Read the int16 vector into oldvect */
	getvec(0x16, &oldvect);
	
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
		printf("Not enough memory to install Intercept16\n");
		return (-1);
	}
	codeseg = reserve(totalsize);

	/* ---- pass the segment along that holds the data ---- */
	hidata(codeseg);
	
	/* Copy code and data parts into high memory */
	bcopy(MK_FP(codeseg, 0),  MK_FP(mycs(), 0), codesize + datasize);
	
	/* Intercept int16 calls */
	setvec(0x16, newvect, codeseg);
	
	putstr("Intercept16 BIOS extension installed at ");
	puthex(codeseg);
	putstr(" resident size ");
	puthex(totalsize);
	putstr(".\r\n\n");
	
	/* Return last device number to extended BIOS monitor */
	return -1;
}

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

	if (com_port != -1)
	  return 1;
	else
	  return 0;
}

