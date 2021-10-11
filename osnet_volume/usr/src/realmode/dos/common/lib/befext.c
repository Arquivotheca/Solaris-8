/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *    Solaris x86 realmode device driver management:
 *
 *    This file implements the server (framework) side of the extended real-
 *    mode driver (".bef") interface.  Three routines make up this interface:
 *
 *        LoadBef()  ...  Loads a realmode driver into memory
 *        FreeBef()  ...  Unloads a driver
 *        CallBef()  ...  Invokes a loaded driver at one of its entry points
 */

#ident "@(#)befext.c	1.8	95/06/09 SMI\n"
#include <befext.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dos.h>

static void far *befbuf = 0;
static char far *lpt;
void far *_BEFXp;
int _BEFXmsg;

#define	PARASIZE 16
#define	roundup(x, y) (((unsigned long)(x)+(y)-1) & ~((y)-1))

/* Build-in assembler doesn't recognize "dw" directive, so we roll our own! */
#define	dw(O) jmp short $+O

static char const *msgtab[] = {
	/*
	 *  Driver error codes, in plain English ...
	 */

	"driver failure",		   /* .. BEF_FAIL		    */
	"no device at specified address",  /* .. BEF_PROBEFAIL		    */
	"invalid callback sequence",	   /* .. BEF_CALLBACK		    */
	"invalid dispatch function",	   /* .. BEF_BADFUNC		    */
	"error in BIOS configuration",	   /* .. BEF_BADBIOS		    */
};

#define	tabsize (sizeof (msgtab)/sizeof (msgtab[0]))

char far *
LoadBef(char *file)
{
	/*
	 *  Load a realmode driver:
	 *
	 *  This routine reads the realmode driver (".bef" file) containined
	 *  in the input "file" into a dynamically allocated buffer and returns
	 *  the load point to the caller.  If we're unable to load the driver,
	 *  or if after loading it we can't find the appropriate magic numbers,
	 *  we print an error message and return a null pointer.
	 */

	int fd = -1;
	struct stat st;

	_BEFXmsg = 1;
	if (befbuf) FreeBef(); /* Free up any leftover .bef buffer!	    */

	if (((fd = open(file, O_RDONLY+O_BINARY)) < 0) || fstat(fd, &st)) {
		/*
		 *  If we can't find the file, we won't be able to load it!
		 *  Caller is expecting us to print all error messages.
		 */

		printf("\n*** Can't open %s ***\n", file);

	} else if (befbuf = _fmalloc((unsigned)st.st_size + PARASIZE)) {
		/*
		 *  We just allocated a buffer that should be large enough
		 *  to hold the .bef file.  Read the entire .bef file into
		 *  this buffer.
		 */

		unsigned sink;
		lpt = (char far *)roundup(befbuf, PARASIZE);

		if (!_dos_read(fd, lpt, (unsigned)st.st_size, &sink)) {
			/*
			 *  The .bef file is now in memory.  Check to make
			 *  sure it looks like a real device driver.
			 */

			if (*(short far *)lpt == BEF_SIGNATURE) {
				/*
				 *  So far so good.  We don't check for the
				 *  presence of a .bef extension magic number
				 *  because we don't know what the caller wants
				 *  to do with this thing!
				 */

				_asm {
					/*
					 *  Adjust "BEFXlpt" pointer so that
					 *  "probe & load" entry point is at
					 *  offset 0.  This makes it easier for
					 *  the caller to locate the bef ex-
					 *  tension magic number and for us to
					 *  call into the driver later.
					 */

					push  bx
					push  es
					les   bx, dword ptr [lpt]
					mov   ax, es
					add   ax, es:[bx+8]
					shr   bx, 4
					add   ax, bx
					mov   word ptr [lpt+2], ax
					mov   word ptr [lpt], 0
					pop   es
					pop   bx
				};

				close(fd);
				return (lpt);

			} else {
				/*
				 *  This doesn't look like a realmode driver
				 *  to me!  Print error message and fall thru
				 *  to common error exit.
				 */

				printf("\n*** No signature on %s ***\n", file);
			}

		} else {
			/*
			 *  We were unable to read the .bef file, even though
			 *  it opened successfully.  This is very strange!
			 */

			printf("\n*** Can't read %s ***\n", file);
		}

	} else {
		/*
		 *  Caller is hogging all the memory (or the driver is unusally
		 *  large).  We can't do anything without buffer big enough to
		 *  hold the driver.
		 */

		printf("\n*** Can't get %d-byte buffer for %s ***\n",
							    st.st_size, file);
	}

	if (fd >= 0) close(fd);
	return (0);
}

void
FreeBef()
{
	/*
	 *  Unload a realmode driver:
	 *
	 *  This routine may be used to unload realmode drivers loaded by
	 *  "LoadBef".  We only allow one driver to be loaded at any given
	 *  time.
	 */

	if (befbuf) {
		/*
		 *  There's nothing to do unless we have a ".bef" file loaded.
		 *  If we do, free up the buffer that contains it and wipe out
		 *  the "BEFXbuf" pointer.
		 */

		_ffree(befbuf);
		befbuf = 0;
	}
}

int
CallBef(int op, struct bef_interface _far *bif)
{
	/*
	 *  Call into driver:
	 *
	 *  This routine is used to call into a driver loaded by "LoadBef".
	 *  The caller specifies the "op"eration the driver is to perform and
	 *  a pointer to the callback interface structure ("bif").
	 *
	 *  Returns the value of %ax when the driver exits.  If an error is
	 *  detected, this will be one of the "BEF_*" error codes listed in
	 *  <befext.h>, and this routine will print an appropriate error
	 *  message before delivering the error code to the caller.
	 */

	static int x;
	x = BEF_LOADFAIL;

	if (befbuf == 0) {
		/*
		 *  Can't call a driver that isn't loaded.  Generate message
		 *  and deliver error return.
		 */

		printf("\n*** Can't call driver, no .bef file ***\n");

	} else if (op && (*(long far *)(lpt+BEF_EXTMAGOFF) != BEF_EXTMAGIC)) {
		/*
		 *  This is an older realmode driver, one that doesn't support
		 *  the new ".bef" extensions.  The only function valid for
		 *  old drivers of this sort is BEF_PROBEINSTAL, but caller
		 *  is requesting something else!
		 */

		printf("\n*** Driver does not support .bef extensions ***\n");

	} else switch (op) {
		/*
		 *  Now lets make sure that the caller is asking for a valid
		 *  driver "op"tion ...
		 */

		case BEF_LEGACYPROBE:
		case BEF_INSTALLONLY:
			/*
			 *  Yep, we know about legacy probes and install only.
			 *  These are implemented via the .bef extension entry.
			 */

			x = BEF_EXTENTRY;
			break;

		case BEF_PROBEINSTAL:
			/*
			 *  The old "probe & install" function is supported
			 *  at the initial entry point (offset 0).
			 */

			x = 0;
			break;

		default:
			/*
			 *  Everything else is garbage ...
			 */

			printf("\n*** Invalid .bef function code (%d) ***\n",
									    op);
			break;
	}

	if (x >= 0) {
		/*
		 *  The "x" register now contains the entry point offset that
		 *  corresponds to the driver "op"tion the caller is trying
		 *  to apply. *  Invoke the driver at this offset.
		 *
		 *  NOTE: Variables local to this block are declared static
		 *	  so that we can find them in a callback routine after
		 *	  the driver has pushed an indeterminate number of C
		 *	  stack frames.  This is the primary reason why we only
		 *	  allow one ".bef" file to be loaded at a time!
		 */

		static struct bef_interface _far *bfx;
		static struct bef_interface bof;
		static char _far *epa;

		epa = lpt + x;
		bfx = bif;

		_asm {
			/*
			 *  Call into the driver:
			 *
			 *  If, you're still not convinced of the ugliness of
			 *  the x86 segmentation scheme, the following code may
			 *  persuade you.  What we'd really like to do here is
			 *  pass the address of the callback vector at "bif" to
			 *  the driver via "ES:DI", but it's not quite that
			 *  simple.  The problem is that the driver will set
			 *  up its own %ds register, and we have to restore the
			 *  server's (i.e, this program's) data segment pointer
			 *  before we can enter the callback routine -- and, of
			 *  course, we have to restore the client's (driver's)
			 *  data segement on the way back out!
			 */

			pusha;			   Save some registers
			mov   cs:dsx, ds;	   .. Segment registers, too
			mov   cs:esx, es
			mov   cs:ssx, ss
			mov   cs:spx, sp
			lea   di, word ptr [bof];  ES:DI will point to "bof"
			lea   ax, cs:cbres;	   .. whose resource and node
			mov   word ptr [di+2], ax; .. pointers must be made to
			mov   word ptr [di+4], cs; .. refer to cbres and cbnod,
			lea   ax, cs:cbnod;	   .. below.
			mov   word ptr [di+6], ax
			mov   word ptr [di+8], cs
			les   bx, bfx;		   Make sure versions match!
			mov   ax, word ptr es:[bx+0]
			mov   word ptr [di+0], ax
			mov   ax, ds
			mov   es, ax;		   ES:DI = &bof;
			lea   dx, word ptr [_BEFXp]
			mov   ax, op
			call  epa;		   call out
			jmp   short fin

		dsx:	dw    (0);	Save areas for segment regs
		esx:	dw    (0)
		ssx:	dw    (0)
		spx:	dw    (0)

		/*
		 *  The two routines that follow, "cbres" and "cbnod" are the
		 *  callback "glue" routines that fix up the %ds register on
		 *  the way in and out of the "resource" and "node" callbacks,
		 *  respectively.
		 */

		cbres:	push  bp;		Push a C stack frame
			mov   bp, sp
			push  bx;		Save work registers
			push  ds
			push  es
			mov   ds, cs:dsx;	Restore server's %ds
			les   bx, bfx
			push  word ptr [bp+18];	Push resource routine's args,
			push  word ptr [bp+16];	.. length of value buffer
			push  word ptr [bp+14]
			push  word ptr [bp+12]; .. resource value buffer ptr
			push  word ptr [bp+10]
			push  word ptr [bp+8];	.. resource name pointer
			push  word ptr [bp+6]
			call  dword ptr es:[bx+2]
			add   sp, 14
			jmp   don

		cbnod:	push  bp;		Push a C stack frame
			mov   bp, sp
			push  bx;		Save work registers
			push  ds
			push  es
			mov   ds, cs:dsx; 	Restore server's %ds
			les   bx, bfx
			push  word ptr [bp+6];	Push "node" routine's argument
			call  dword ptr es:[bx+6]
			add   sp, 2

		don:	pop   es;		Common exit code for callback
			pop   ds;		.. routines: restores registers
			pop   bx;		.. and returns to driver
			pop   bp
			retf

		/*
		 *  Driver exits to this code.  This little eplilog has to be
		 *  the last piece of assembler code to keep the C compiler
		 *  happy (it's not expecting us to be jumping around like a
		 *  crazy monkey).
		 */

		fin:	cli;			JIC driver didn't do it!
			mov   ds, cs:dsx;	Restore segment regs
			mov   es, cs:esx
			mov   ss, cs:ssx
			mov   sp, cs:spx
			mov   x, ax;		Set return code
			popa
		}

		if ((x < 0) && _BEFXmsg) {
			/*
			 *  Driver detected an error.  Print the corresponding
			 *  error message before returning to caller.
			 */

			int msgx = ~x;
			if (msgx >= tabsize) msgx = 0;
			printf("\n*** %s ***\n", msgtab[msgx]);
		}
	}

	return (x);
}
