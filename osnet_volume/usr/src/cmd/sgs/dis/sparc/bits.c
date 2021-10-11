/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)bits.c	1.9	98/10/05 SMI"


#include	"dis.h"
#include	"extn.h"

static long getword(void);
static void fillbuff(void);

#define	FAILURE 0
#define	MAXERRS	1  /* maximum # of errors allowed before */
				/* abandoning this disassembly as a	*/
				/* hopeless case			*/

static short errlev = 0;	/* to keep track of errors encountered 	*/
				/* during the disassembly, probably due	*/
				/* to being out of sync.		*/

#define	OPLEN	35	/* maximum length of a single operand	*/
				/* (will be used for printing)		*/

static	char	operand[4][OPLEN];	/* to store operands as they	*/
					/* are encountered		*/
static	char	symarr[4][OPLEN];

static	GElf_Sxword	start;	/* start of each instruction	*/
				/* used with jumps		*/

static int	bytesleft = 0;	/*  how many unread bytes are in buffer */

#define	TWO_8	256
#define	TWO_16	65536
#define	MIN(a, b)	((a) < (b) ? (a) : (b))

int           nextword_avail = 0; /* Unimp 0 == 0 !!! So we may NOT
                                   * nextword as its own flag !
								   * See #4023529
								   */
unsigned long nextword = 0;
unsigned long prevword = 0;
char	nextobject[30];

static long getword(void);

/*
 *	dis_text ()
 *
 *	disassemble a text section
 */

void
dis_text(GElf_Shdr * shdr)
{
	/* the following arrays are contained in tables.c	*/
	/* extern	struct	instable	opcodetbl[256]; */
	unsigned long   iii;
	char *sss;

	/* initialization for each beginning of text disassembly	*/

	bytesleft = 0;

	/*
	 * An instruction is disassembled with each iteration of the
	 * following loop.  The loop is terminated upon completion of the
	 * section (loc minus the section's physical address becomes equal
	 * to the section size) or if the number of bad op codes encountered
	 * would indicate this disassembly is hopeless.
	 */

	/* #4023529 : clear nextword availability flag 
	 * so that a word is not taken from a prev. dis'ed
	 * file :
	 */
	nextword_avail = 0; 
	for (loc = shdr->sh_addr;
		((loc-shdr->sh_addr) < shdr->sh_size) && (errlev < MAXERRS);
		printline()) {
		start = loc;
		(void) sprintf(operand[0], "");
		(void) sprintf(operand[1], "");
		(void) sprintf(operand[2], "");
		(void) sprintf(operand[3], "");
		(void) sprintf(symarr[0], "");
		(void) sprintf(symarr[1], "");
		(void) sprintf(symarr[2], "");
		(void) sprintf(symarr[3], "");

		/* look for C source labels */
		if (Lflag && debug)
			looklabel(loc);

		line_nums();

		prt_offset();			/* print offset */

		if (nextword_avail) {
			iii = nextword;
			loc += 4;
			nextword_avail = nextword = 0;
			sprintf(object, "%s%s", object, nextobject);
			nextobject[0] = '\0';
		} else
			iii = getword();

		sss = disassemble(iii, loc - 4);
		strcpy(mneu, sss);

		prevword = iii;		/* copy current instruction to prev */

		}  /* end of for */

	if (errlev >= MAXERRS) {
		(void) printf("dis: %s: %s: section probably not "
		    "text section\n", fname, sname);
		(void) printf("\tdisassembly terminated\n");
		errlev = 0;
		return;
	}
}


/*
 *	get1byte()
 *
 *	This routine will read the next byte in the object file from
 *	the buffer (filling the 4 byte buffer if necessary).
 *
 */


void
get1byte(void)
{
	if (bytesleft == 0) {
		fillbuff();
		if (bytesleft == 0) {
			(void) fprintf(stderr, "\ndis:  premature EOF\n");
			exit(4);
		}
	}
	cur1byte = ((unsigned short) bytebuf[4-bytesleft]) & 0x00ff;
	bytesleft--;
	(oflag > 0)?	(void) sprintf(object, "%s%.3o ", object, cur1byte):
			(void) sprintf(object, "%s%.2x ", object, cur1byte);
	loc++;
	if (trace > 1)
		(void) printf("\nin get1byte object<%s> cur1byte<%.2x>\n",
		    object, cur1byte);
}

long
getnextword(void)
{
	short   byte0, byte1, byte2, byte3;
	int	i, j, bytesread;
	union {
		char    bytes[4];
		long    word;
	} curword;

	curword.word = 0;
	for (i = 0, j = 4 - bytesleft; i < bytesleft; i++, j++)
		curword.bytes[i] = bytebuf[j];
	if (bytesleft < 4) {
		bytesread = bytesleft;
		fillbuff();
		if ((bytesread + bytesleft) < 4) {
			(void) fprintf(stderr, "\ndis:  premature EOF\n");
			exit(4);
		}
		for (i = bytesread, j = 0; i < 4; i++, j++) {
			bytesleft--;
			curword.bytes[i] = bytebuf[j];
		}
	}
	byte0 = ((short)curword.bytes[0]) & 0x00ff;
	byte1 = ((short)curword.bytes[1]) & 0x00ff;
	byte2 = ((short)curword.bytes[2]) & 0x00ff;
	byte3 = ((short)curword.bytes[3]) & 0x00ff;
	if (trace > 1)
		(void) printf("\nin getword object<%s>>^H word<%.8lx>\n",
				object, curword.word);
	(oflag > 0)?    (void) sprintf(nextobject, "%.3o %.3o %.3o %.3o ",
				byte0, byte1, byte2, byte3):
			(void) sprintf(nextobject, "%.2x %.2x %.2x %.2x ",
				byte0, byte1, byte2, byte3);
	nextword = curword.word;
	nextword_avail = 1;
	return (curword.word);
}

/*
 *	getword()
 *	This routine will read the next 4 bytes in the object file from
 *	the buffer (filling the 4 byte buffer if necessary).
 *
 */

static long
getword(void)
{
	short	byte0, byte1, byte2, byte3;
	int	i, j, bytesread;
	union {
		char	bytes[4];
		long	word;
	} curword;

	curword.word = 0;
	for (i = 0, j = 4 - bytesleft; i < bytesleft; i++, j++)
		curword.bytes[i] = bytebuf[j];
	if (bytesleft < 4) {
		bytesread = bytesleft;
		fillbuff();
		if ((bytesread + bytesleft) < 4) {
			(void) fprintf(stderr, "\ndis:  premature EOF\n");
			exit(4);
		}
		for (i = bytesread, j = 0; i < 4; i++, j++) {
			bytesleft--;
			curword.bytes[i] = bytebuf[j];
		}
	}
	byte0 = ((short)curword.bytes[0]) & 0x00ff;
	byte1 = ((short)curword.bytes[1]) & 0x00ff;
	byte2 = ((short)curword.bytes[2]) & 0x00ff;
	byte3 = ((short)curword.bytes[3]) & 0x00ff;
	if (oflag > 0)
		(void) sprintf(object, "%s%.3o %.3o %.3o %.3o ",
		    object, byte0, byte1, byte2, byte3);
	else
		(void) sprintf(object, "%s%.2x %.2x %.2x %.2x ",
		    object, byte0, byte1, byte2, byte3);
	loc += 4;
	if (trace > 1)
		(void) printf("\nin getword object<%s>> word<%.8lx>\n",
		    object, curword.word);
	return (curword.word);
}


/*
 *	fillbuff()
 *
 *	This routine will read 4 bytes from the object file into the
 *	4 byte buffer.
 *	The bytes will be stored in the buffer in the correct order
 *	for the disassembler to process them. This requires a knowledge
 *	of the type of host machine on which the disassembler is being
 *	executed (AR32WR = vax, AR32W = maxi or 3B, AR16WR = 11/70), as
 *	well as a knowledge of the target machine (FBO = forward byte
 *	ordered, RBO = reverse byte ordered).
 *
 */

static void
fillbuff(void)
{
	int i = 0;

	while (p_data != NULL && i < 4) {
		bytebuf[i] = *p_data;
		bytesleft = i+1;
		i++;
		p_data++;
	}

	switch (bytesleft) {
	case 0:
	case 4:
		break;
	case 1:
		bytebuf[1] = bytebuf[2] = bytebuf[3] = 0;
		break;
	case 2:
		bytebuf[2] = bytebuf[3] = 0;
		break;
	case 3:
		bytebuf[3] = 0;
		break;
	}
/*
 * NOTE		The bytes have been read in the correct order
 *		if one of the following is true:
 *
 *		host = AR32WR  and  target = FBO
 *			or
 *		host = AR32W   and  target = RBO
 *
 */
#if !M32
#if (RBO && AR32WR) || (FBO && AR32W)
	bytebuf[0] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[1] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[2] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[3] = (char)(tlong & 0x000000ffL);
#endif

#if (FBO && AR32WR) || (RBO && AR32W)
	bytebuf[0] = (char)(tlong & 0x000000ffL);
	bytebuf[1] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[2] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[3] = (char)((tlong >> 24) & 0x000000ffL);
#endif

#if RBO && AR16WR
	bytebuf[0] = (char)((tlong >>  8) & 0x000000ffL);
	bytebuf[1] = (char)(tlong & 0x000000ffL);
	bytebuf[2] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[3] = (char)((tlong >> 16) & 0x000000ffL);
#endif
#if FBO && AR16WR
	bytebuf[0] = (char)((tlong >> 16) & 0x000000ffL);
	bytebuf[1] = (char)((tlong >> 24) & 0x000000ffL);
	bytebuf[2] = (char)(tlong & 0x000000ffL);
	bytebuf[3] = (char)((tlong >>  8) & 0x000000ffL);
#endif
#endif
}
