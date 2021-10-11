/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Menu Interface (main routines):
 *
 *    This file contains most of the code required to implement the Solaris
 *    x86 realmode menu interface.  This is a simple "mark-and-select" type 
 *    interface, very similar to the sort of thing used in systems based
 *    around dumb terminals.  This is no coincedence.  The whole idea is
 *    to provide a "user friendly" interface that will work over a serial
 *    line!
 *
 *    Serial operation occurs when the calling program is running on the
 *    boot code's DOS emulator with console I/O redirected through a serial
 *    port.  The emulator intercepts the "int 10h" calls issued below and
 *    translates cursor movement commands into the appropriate ANSI escape
 *    sequences.
 * 
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)menu.c	1.11	95/05/04 SMI\n"
#define	__IMPLEMENTATION__

#include <dostypes.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <menu.h>

char *_MenuErr_;
static char mname[13];
struct menu far *_MenuCache_[2];

#define	ATR_NORMAL  0x07    // "Normal" (white on black) char attribute
#define	ATR_HILITE  0x70    // Highlighted (black on white) char attribute
#define ATR_INTENSE 0x7F    // Really intense highlighting!

#define MAX_LIST 16383      // Maximum number of elements in a list

static char row, col;       // Current currsor position!
static char page;           // Display page number
static char mode;           // Display mode

#define	ESC        0x1B     // <esc>, the cancel key!
#define	ENTER      0x0D     // <cr>, the "done" key
#define	TAB        0x09     // Tab and right-arrow advance input selector
#define RARROW     0xCD
#define	BACKTAB    0x8F     // Backtab, backspace, and left-arrow retard
#define	BACKSPACE  0x08     // .. the input selector
#define	LARROW     0xCB
#define	UARROW     0xC8     // Up-arrow and page-up scroll up
#define	UPAGE      0xC9
#define	DARROW     0xD0     // Down-arrow and page-down scroll down
#define	DPAGE      0xD1
#define	HOME       0xC7     // Home/end keys scroll to top/bottom of list
#define	END        0xCF
#define	SPACE      0x20     // Space bar is list selection toggle
#define DELETE     0xD3     // Delete to erase string input
#define	REPAINT    0x1A     // Ctl-Z repaints the screen
#define	HELP       0xBB     // F1 is the help key
#define BEL        0x07     // Beep for invalid input
#define CTLP       0x10     // Alternate help key

// Some simple macros ...
#define sign(x)        (((x) < 0) ? -1 : 1)
#define min(x,y)       (((x) < (y)) ? (x) : (y))
#define islist(x)      (((x) | 1) == 'M')
#define hidecursor()   _SetCurPos_(25, 25)

#define isactive(slp)                                                         \
(	/*                                                                        \
	 *  Returns TRUE if the widget described by "slp" will accept input       \ 
	 */                                                                       \
	                                                                          \
	((unsigned)(slp)->num <= 32) && (bitmap & ACTIVATE((slp)->num))           \
)

#define	WidgetLoc(p)                                                          \
(	/*                                                                        \
	 *  Locate the ScreenLoc structure for the currently active widget in     \
	 *  menu at "p".  Returns a null pointer if there is no active widget.    \
	 */                                                                       \
	                                                                          \
    (struct ScreenLoc far *)                                                  \
		((((p)->WTinput > 0) && ((p)->WTinput <= (p)->WTcount))               \
        	    ? (&(p)->buffer[(p)->WTindex]                                 \
			                 + (((p)->WTinput-1) * sizeof(struct ScreenLoc))) \
                : (char far *)0)                                              \
)

void
_SetCurPos_ (int Row, int Col)
{	/*
	 *  Set cursor position:
	 *
	 *  This routine is used to position the cursor prior for reading or
	 *  writing text.  The real work is done by the BIOS, but we do keep
	 *  track of the current cursor position in the global "row" and "col"
	 *  bytes so we won't have to re-read it later.
	 */
	 
	row = Row;   // Save new cursor position in global variables.
	col = Col;

	_asm {
	;	/*
	;	 *  Call into BIOS to position the cursor on the screen.  If we're
	;	 *  running under the second level boot, this call is intercepted
	;	 *  so that cursor movement commands may be translated into the
	;	 *  appropriate ANSI escape sequences for use with serial consoles.
	;	 */

		push  bx
		mov   bh, page   ; Pick up video page number!
		mov   ah, 02h    ; int 10/2 is the cursor positioning command
		mov   dh, row
		mov   dl, col
		int   10h
		pop   bx
	}
}

static void
setup ()
{	/*
	 *  Menu system initialization:
	 *
	 *  This routine is called upon the first invocation of "DisplayMenu"
	 *  or "ClearScreen".  Its job is to set up a number of internal data
	 *  structures used by the menu system.
	 */

	static int first = 1;

	if (first) {
		/*
		 *  Routine is a no-op unless this is the first time we've been
		 *  called.  In this case, we have to read out the current video
		 *  page number (all menu processing will take place on this page).
		 */

		_asm {
		;	/*
		;    *  Issue BIOS call to determine video mode.  We save both
		;	 *  the mode and the display page.
		;	 */

			push  bx
			mov   ah, 0Fh
			int   10h
			mov   page, bh   ; Save video page!
			mov   mode, al   ; Save video mode!
			pop   bx
		}

		// Make sure cache is marked empty.  Microsoft C compiler can't
		// .. seem to get static initialization of far pointers correct!
		_MenuCache_[0] = _MenuCache_[1] = GROUND;
		first = 0;
	}
}

static struct menu far *
find (char *menu, int flag)
{	/*
	 *  Search menu cache:
	 *
	 *  This routine searches the menu cache for the entry associated with
	 *  given "menu".  If we find the entry, we return its address; other-
	 *  wise we call _ParseMenu_ to read the menu from disk.
	 *
	 *  The menu cache is organized as a doubly-linked list and is searched
	 *  linearly.  The menu processor moves the most currently used menu to
	 *  the front of the list however, making it easier to find the least
	 *  recently used menu when we are forced to flush the cache.  If the
	 *  detach "flag" is non-zero, we remove the target menu struct from 
	 *  the cache list before returning it to the caller.
	 */

	struct menu far *mup;  // Pointer to next menu struct

	for (mup = _MenuCache_[0]; mup != GROUND; mup = mup->next) {
		/*
		 *  Check to see if the requested menu is already in the cache.  If
		 *  it is, we won't have to read it back in again!
		 */

		if (!_fstrcmp(menu, mup->name)) {
			/*
			 *  This is it!  Return menu pointer ("mup" register) to caller.
			 */

			if (flag) {
				/*
				 *  Caller wants to re-order the cache list.  We help out
				 *  here by removing the menu struct from the cache list.
				 */

				mup->next->prev = mup->prev;
				mup->prev->next = mup->next;
			}

			return(mup);
		}
	}

	if ((mup = _ParseMenu_(menu)) && !flag) {
		/*
		 *  The menu was not in the cache, but ParseMenu was able to read
		 *  it in from disk.  If the caller does NOT want this menu uncached,
		 *  stick it at the end of the cache list.
		 */

		mup->next = GROUND;
		mup->prev = _MenuCache_[1];
		mup->next->prev = mup->prev->next = mup;
	}

	return(mup);
}

static void
PutChar (int c, int atr)
{	/*
	 *  Write char to screen:
	 *
	 *  This routine writes the character "c", with the given display
	 *  "atr"ibutes, at the current cursor position on the screen.  It
	 *  does NOT advance the cursor after writing!
	 */

	_asm {
	;	/*
	;	 *  The real work occurs in the BIOS.  All we have to do is get
	;	 *  get the right parameters in the proper argument registers!
	;	 */

		push  bx;
		mov   ax, c
		mov   ah, 9h
		mov   bx, atr
		mov   bh, page    ; Load video page number
		mov   cx, 1       ; Only one character please!
		int   10h
		pop   bx
	} 
}

static void
PutStr (struct ScreenLoc far *slp, int atr, char far *cp)
{	/*
	 *  Write a character string to the screen:
	 *
	 *  This routine writes the string at "*cp", with the given display
	 *  "atr"ibutes, at the screen position indicated by the contents of
	 *  the ScreenLoc structure at "*slp".  If the string to be output
	 *  is shorter than "slp->width", we pad on the right with blanks.
	 */

	int j = slp->width;
	_SetCurPos_(slp->row, slp->col);

	while (*cp && j--) {
		/*
		 *  Print output characters until we either run out of text or we
		 *  exhaust the output widget!
		 */ 

		PutChar(*cp++, atr);
		_SetCurPos_(row, col+1);
	}

	if (j > 0) _asm {
	;	/*
	;	 *  If we ran out of text before reaching the end of the output
	;	 *  widget, fill on the right with blanks.  We can do this with 
	;	 *  a single BIOS call!
	;	 */

		push  bx
		mov   ax, 920h
		mov   bx, atr
		mov   bh, page   ; Video page number
		mov   cx, j      ; %cx gives repeat count
		int   10h
		pop   bx
	}

	hidecursor();  // Erase the cursor before returnning
}

static struct widget *
getwidget (va_list ap, struct ScreenLoc far *slp)
{	/*
	 *  Extract widget pointer from argument list:
	 *
	 *  This routine locates the "n"th widget pointer in the "DisplayMenu"
	 *  argument list ("ap") and returns it to the caller.
	 */

	int n;
	struct widget *wp;
	static struct widget dummy = {0};

	if ((slp->type == 'B') && slp->data) {
		/*
		 *  Button widgets with screen-encoded labels may not have widget
		 *  data in the arg list, and even if it was there we wouldn't use
		 *  it.  Nevertheless, the caller is expecting a valid address so
		 *  return a pointer to the dummy widget struct!
		 */

		wp = &dummy;

	} else for (n = 0; n < slp->num; n++) {
		/*
		 *  Keep extracting widget pointers from the argument list until
		 *  we reach the "n"th one.
		 */

		wp = va_arg(ap, struct widget *);
	}

	return(wp);
}

static int
GetChar (struct menu far *mup, va_list ap)
{	/*
	 *  Read keystroke:
	 *
	 *  This routine performs low-level input processing.  It reads a 
	 *  character (unbuffered) from the keyboard and returns it to the caller.  
	 *  DOS 2-byte encodings for special function keys (e.g, arrow keys)
	 *  are converted to 1-byte codes with the high order bit set.
	 */

	int j = 2, c = 0;
	struct widget *wp;
	struct ScreenLoc far *slp = 0;

	if ((mup->WTinput > 0) && (slp = WidgetLoc(mup)) && (slp->type == 'S')) {
		/*
		 *  We're about to accept text input into a string widget.  We need
		 *  to know where the buffer is for this -- it's address is appears
		 *  in the "in" field of the corresponding widget struct argument.
		 */

		wp = getwidget(ap, slp);

	} else {
		/*
		 *  This isn't a string widget, so we clear the screen loc pointer
		 *  to prevent confusion later.
		 */

		slp = 0;
	}

	while (j-- && !c) {
		/*
		 *  For certain function keys (e.g, arrows) DOS delivers a 2-byte
		 *  code sequence, the first byte of which is zero.  Hence, we
		 *  loop until we get a non-zero input character.
		 */

 redo:  c = _ReadByte_();  // Get next user-keyed input byte

		if (j == 0) {
			/*
			 *  If this is the second byte of a 2-byte encoding, turn on
			 *  the high order bit of the return value.
			 */

			if (((c |= 0x80) == DELETE) && (slp != 0)) {
				/*
				 *  We just read a "DEL" key.  If we're reading text input
				 *  ("slp != 0"), this means clear the input buffer and reset
				 *  the cursor to the 1st input byte.
				 */  

				PutStr(slp, ATR_HILITE, " ");
				_SetCurPos_(slp->row, slp->col);
				_fmemset(wp->in, 0, slp->width+1);
				PutChar(' ', ATR_NORMAL);
				slp->x = 0;
				j = 1;

				goto mkdo;  // Should be "goto redo", but there's a compiler
			}               // .. bug that prevents it from compiling!!

		} else if (slp) switch (c) {
			/*
			 *  Next char is not an escape sequence, and we're trying to fill
			 *  an input buffer.  Stuff the character into the next buffer
			 *  location ...
			 */

			case BACKSPACE:
			{	/*
				 *  .. Unless it's a backspace, of course!  In this case we
				 *  have to back the cursor up and wipe out any text that
				 *  might have existed before.
				 */

				if (slp->x > 0) {
					/*
					 *  Don't bother to back up if we're already at the first
					 *  column of the input field!  Otherwise backing up con-
					 *  sists of re-blanking the current cursor, then wiping
					 *  out the prevous character and printing the cursor in
					 *  its place!
					 */

					_SetCurPos_(slp->row, slp->col + slp->x);
					wp->in[--(slp->x)] = '\0';
					PutChar(' ', ATR_HILITE);

					_SetCurPos_(slp->row, slp->col + slp->x);
					PutChar(' ', ATR_NORMAL);
 mkdo:				hidecursor();
				}

 				goto redo;   // Don't return backspace -- get next char!
			}

			case CTLP:
			{	/*
				 *  Alternate help key.  Set official value and bail out.
				 */

				c = HELP;
				break;
			}

			default:
			{	/*
				 *  Make sure the next character is printable, and if so
				 *  drop it into the input buffer.
				 */

				if ((c >= ' ') && (c <= '~')) {
					/*
					 *  Next character is a likely candidate for inclusion 
					 *  in the input buffer.  Store it at the next buffer lo-
					 *  cation and echo it to the screen.
					 */

					if (slp->x >= slp->width) slp->x = slp->width-1;
					_SetCurPos_(slp->row, slp->col + slp->x);
					PutChar(c, ATR_HILITE);
					wp->in[slp->x++] = c;
					wp->in[slp->x] = 0;
					j = ' ';

					// Check to make sure we don't run off end of string!
					if (slp->x >= slp->width) { slp->x--; j = c; }
					_SetCurPos_(slp->row, slp->col + slp->x);
					PutChar(j, ATR_NORMAL);
					hidecursor();
				}

				break;
			}

		} else if ((c == '?') || (c == CTLP)) {
			/*
			 *  If we're not reading a text widget, a question mark is
			 *  interpreted as a plea for help!
			 */

			c = HELP;
		}
	}

	return ((c == HELP) ? _GetHelp_(mname) : ((c == REPAINT) ? 0 : c));
}

static void
putwidget (struct menu far *mup, struct ScreenLoc far *slp,
                                 struct widget *wp,
                                 int hi)
{	/*
	 *  Write widget to screen:
	 *
	 *  This routine formats the widget specified by the "slp" and "wp"
	 *  pointers and writes it to the screen over the menu defined by "mup".
	 *  If the "hi" intensity flag is non-zero we write the widget data in
	 *  inverse video.
	 */

	int j, n = ' ';

	switch (slp->type) {
		/*
		 *  As far as output goes, there are really only two types
		 *  of widgets:  Strings (including buttons) and lists of
		 *  strings.  Process according to type.
		 */

		case 'B': case 'S':
		{	/*
			 *  Simple string widgets.  Locate the output string and write
			 *  it to the screen with the current "atr"ibute.
			 */

			PutStr(slp, (hi ? ATR_HILITE : ATR_NORMAL),
                        ((slp->data > 0) ? &mup->buffer[slp->data]
			                             : (char far *)wp->out));

			if (slp->type == 'S') {
				/*
				 *  We have to dink with the input buffers when we (de)activate
				 *  string widgets.  The "slp->x" field gives the current state
				 *  of the widget: Negative if it's inactive, Non-negative if
				 *  it's active.
				 */

				if (hi != 0) {
					/*
					 *  Caller wants to activate the string.  It may be
					 *  active already, but we re-draw the cursor in any
					 *  case.
					 */

					if (slp->x < 0) {
						/*
						 *  Widget is currently inactive, so we must activate
						 *  it.  This consists of initializing the input buf-
						 *  fer and caculating the initial input position.
						 */

						if (wp->in != (char far *)wp->out) {
							/*
							 *  Input buffer differs from output.  Copy the
							 *  contents of the output buffer into the input
							 *  buffer, making sure to null terminate the
							 *  data.
							 */

							_fstrncpy(wp->in, wp->out, slp->width);
							wp->in[slp->width] = 0;
						}

						if ((slp->x = _fstrlen(wp->in)) >= slp->width) {
							/*
							 *  Locate the end of the input string, and put
							 *  the initial cursor there.  If the string fills
							 *  the buffer, the cursor shows the last char
							 *  in un-inverse video!
							 */

							slp->x = slp->width - 1;
							n = wp->in[slp->x];
						}
					}

					_SetCurPos_(slp->row, slp->col + slp->x);
					PutChar(n, ATR_NORMAL);
					hidecursor();

				} else {
					/*
					 *  Deactivate a string widget.
					 */

					slp->x = -1;
				}
			}

			break;
		}

		case 'L': case 'M':
		{	/*
			 *  Lists of strings:  Write up to "depth" output strings
			 *  from the "wp->out" list in successive rows, same column.
			 */

			struct ScreenLoc slx = *slp;
			n = 0;

			if ((slx.x < 0) || (slx.x >= wp->size)) {
				/*
				 *  Window index has been reset.  Assume we'll open the 
				 *  scrolling window over the first element of the list.
				 */  

				slx.data = wp->size; // Assume we'll have to resize!

				if (hi) {
					/*
					 *  If this is the active input widget ("hi" register
					 *  is non-zero) check the input flags to see if there's
					 *  an entry already marked.  If so, open the windo on
					 *  that element!
					 */

					for (j = 0; j < wp->size; j++) {
						/*
						 *  Step thru the input list looking for selected
						 *  list items.
						 */

						if (wp->in[j]) {
							/*
							 *  Yep, here it is!  Set data and window indexes
							 *  so that selected item appears somewhere in the
							 *  current window!
							 */

							slp->data = slx.data = (j / slx.depth) * slx.depth;
							slp->x = slx.x = (slx.data - j);
							break;
						}
					}
				}
			}

			if (slx.data >= wp->size) {
				/*
				 *  The "data" field of the ScreenLoc structure gives the first
				 *  output string to appear in the list (this value can be 
				 *  changed by scrolling).  If the output list has shrunk since
				 *  we last displayed it, we may be starting with an an element
				 *  that's beyond the end!  If so, reset the first element 
				 *  indicator to the beginning of the list.
				 */

				slx.data = slp->data = 0;
				slx.x = slp->x = 0;
			}

			while (n < slx.depth) {
				/*
				 *  Step thru the output list displaying the next "depth" 
				 *  strings beginning with "wp->out[slx.data]".  Any selected
				 *  list elements we encounter (including the current item if
				 *  this is the 1st active widget) are highlighted.
				 */

				char far *cfp;
				int atr = (((n++ == slx.x) && hi) ? ATR_HILITE : ATR_NORMAL);

				if (slx.data < wp->size) {
					/*
					 *  There's real data to be displayed at this location.
					 *  Set "cfp" register to point at the text.
					 */

					cfp = ((char far * far *)wp->out)[slx.data];

					if ((slx.type == 'M') && (wp->in[slx.data] != 0)) {
						/*
						 *  If caller wants to highlight a field that's
						 *  already highlighted, make it INTENSE!
						 */

						atr = ((atr == ATR_HILITE) ? ATR_INTENSE : ATR_HILITE);
					}

				} else {
					/*
					 *  If list is shorter than window, search the background
					 *  widgets looking for a string we can use to fill in
					 *  the hole.
					 */

					cfp = ""; // JIC we can't find background text!
					slp = (struct ScreenLoc far *)&mup->buffer[mup->BGindex];

					for (j = mup->BGcount; j--; slp++) {
						/*
						 *  We're only intereseted in background widgets on
						 *  the same row as the current list item starting
						 *  at or before the current column.
						 */

						int x = slx.col - slp->col;
						char far *cxp = &mup->buffer[slp->data];

						if ((slx.row == slp->row) && (x >= 0)
						                          && (_fstrlen(cxp) > x)) {
							/*
							 *  This string fits the bill.  Use it in place
							 *  of the "worst case" null string that we assumed
							 *  above.
							 */

							cfp = cxp + x;
							break;
						}
					}
				}
 
				PutStr(&slx, atr, cfp);
				slx.data += 1;
				slx.row++;
			}
		}
	}
}

static void
toggle (struct menu far *mup, struct ScreenLoc far *slp,
                              struct widget *wp, 
                              int hi)
{	/*
	 *  Toggle selected widget:
	 *
	 *  This routine rewrites the currently selected widget as identified
	 *  by the "slp" and "wp" pointers.  If the "hi"intensity flag is set,
	 *  we write the selected widget value in inverse video.
	 *
	 *  If the selected widget as a list type ('L' or 'M'), we only toggle
	 *  the currently selected item of this list.
	 */

	int x = (hi ? ATR_HILITE : ATR_NORMAL);
	struct ScreenLoc slx = *slp;
	
	if (islist(slx.type)) {
		/*
		 *  If target widget is a list type, we need only toggle the current
		 *  member (given by the "x" offset).
		 */

		if (wp->size > 0) {
			/*
			 *  List is not emtpy, which means it's OK to display the data
			 *  contained in the "out" array.  The entry we want is given
			 *  by the "x" field of the screen loc struct.
			 */

			wp->out = ((void far* far*)wp->out)[slx.data + slx.x];

			if ((slx.type == 'M') && wp->in[slx.data+slx.x]) {
				/*
				 *  If we're trying to highlight a field that's already
				 *  highlighted, make it really intense!
				 */

				x = (hi ? ATR_INTENSE : ATR_HILITE);
			}

		} else {
			/*
			 *  If the list is empty, there is no current member to be
			 *  highlighted, so we just highlight some spaces instead!
			 */

			wp->out = (void far *)" ";
		}

		slx.row += slx.x;
		PutStr(&slx, x, (char far *)wp->out);

	} else {
		/*
		 *  This is a button or string whose label may be encoded in the menu 
		 *  template.  Use the "putwidget" routine to calculate output and
		 *  do the appropriate stuff.
		 */

		putwidget(mup, slp, wp, hi);
	}
}

int
DisplayMenu (char *menu, ...)
{	/*
	 *  Display output and recieve input thru a menu:
	 *
	 *  This is the primary external interface to this module.  The Caller
	 *  provides the name of a "menu" file which is used to control screen
	 *  I/O.  We first paint the screen with the background information taken
	 *  from the menu file, then write any caller-specified output to the
	 *  screen.  We then sit in an input loop, interpreting cursor movement
	 *  and item selection keys until the user presses ENTER.  At that point
	 *  we return a "widget number" to the caller to indicate which of the
	 *  various items on the screen was selected as input by the user.
	 *
	 *  Variable output data and input buffers are specified by the caller
	 *  via pointers to "widget" structures.  There may be any number of
	 *  these, so a va_list is used to access them.
	 */

	va_list ap;
	struct menu far *mup;
	unsigned long bitmap = 0;
	int wipe = strcmp(menu, mname);

	setup();
	va_start(ap, menu);

	if (!(mup = find(menu, 1))) {
		/*
		 *  The requested menu was not in the cache and we were unable to
		 *  read it from disk.  There should be an error message in the
		 *  "_MenuErr_" buffer at this point.  Clear the screen, write this 
		 *  message, then hold the screen so the user can read it!
		 */

		ClearScreen(); 
		printf("\n*** Error in %s menu ***\n", menu);
		printf("%s\n\n<ENTER> to continue ", _MenuErr_);
		while (_ReadByte_() != '\n');
		return(-1);
	}

	/*
	 *  Move the menu to the front of the cache list, marking it "most
	 *  recently used".
	 */

	mup->prev = GROUND;
	mup->next = _MenuCache_[0];
	mup->next->prev = mup->prev->next = mup;

	if (mup->WTcount) {
		/*
		 *  If menu has widgets, caller must provide an active widget map.
		 *  Load it from the argument list.
		 */

		bitmap = va_arg(ap, unsigned long);
	}

	for (;; wipe = 1) {
		/*
		 *  Basic input/output loop.  We stay inside this loop until the user
		 *  presses the ENTER or ESC keys.  We re-iterate the loop when-
		 *  ever the REPAINT (or HELP) key is pressed.
		 */

		int j, k;
		int col, input = 0;
		struct widget wx, *wp;
		struct ScreenLoc far *slp;

		if (wipe) {
			/*
			 *  Background needs to be re-displayed.  Clear the screen and
			 *  output all of the "ScreenLoc" structures in the "BG" list.
			 */

			wipe = 0;
			ClearScreen();
			slp = (struct ScreenLoc far *)&mup->buffer[mup->BGindex];

			for (j = mup->BGcount; j--; slp++) {
				/*
				 *  For background "ScreenLoc"s, the "data" field of the
				 *  ScreenLoc structure gives the background text string's
				 *  location in the variable-length buffer.  Use the 
				 *  "PutStr" routine to print this text at the specified
				 *  screen position.
				 */

				PutStr(slp, ATR_NORMAL, &mup->buffer[slp->data]);
			}
		}

		hidecursor();
		strcpy(mname, menu);
		slp = (struct ScreenLoc far *)&mup->buffer[mup->WTindex];

		for (j = 1; j <= mup->WTcount; (j++, slp++)) {
			/*
			 *  Now write all output widgets to the screen.   We have to
			 *  do this every time we're called, even if we're using the
			 *  same menu as last time.  This is because the widget values
			 *  can change between calls.
			 */

			wp = getwidget(ap, slp);
			if (!input && isactive(slp)) input = j; 

			putwidget(mup, slp, wp, 0);
		}

		if (mup->WTinput && (slp = WidgetLoc(mup)) && isactive(slp)) {
			/*
			 *  If we have an input widget left over from the last time we
			 *  displayed this menu -- and said widget is still active --
			 *  make this the input widget.
			 */

			input = mup->WTinput;
		}

		if (input != 0) {
			/*
			 *  Redisplay the active input widget in inverse-video!
			 */

			mup->WTinput = input;
			slp = WidgetLoc(mup);
			putwidget(mup, slp, getwidget(ap, slp), 1);
		}

		while (j = GetChar(mup, ap)) switch (j) {
			/*
			 *  Read and process the next input character.  The "GetChar"
			 *  routine returns zero if the caller presses the REPAINT
			 *  (Ctl-Z) key.
			 */ 

			case ESC:
			{	/*
				 *  Pressing the cancel (escape) key causes DisplayMenu to
				 *  return 0.  Caller is responsible for undoing any input
				 *  that may have occured (i.e, restoring "string" and "M-
				 *  list" input buffers to their orignial state).
				 */

				return(0);
			}

			case ENTER: 
			{	/*
				 *  Pressing enter key causes DisplayMenu to return the
				 *  current widget number (or zero if there is none!).
				 */

				if ((slp = WidgetLoc(mup)) && (slp->type == 'L')) {
					/*
					 *  If the input widget is a single-select list, mark the
					 *  current entry "selected" before returning it to the
					 *  caller.
					 */

					wp = getwidget(ap, slp);
					if (wp->size <= 0) goto bep;
					wp->in[slp->data + slp->x] = 1;
				}

				return(slp ? slp->num : 0);
			}

			case TAB: case RARROW:
			{	/*
				 *  Tab and right-arrow advance the cursor forward to the
				 *  next input widget (if there is one).
				 */

				j = 1;
				goto adv;
			}

			case BACKTAB: case LARROW:
			{	/*
				 *  Backtab and left-arrow move the cursor backward to the
				 *  previous input widget (if any).
				 */

				j = -1;
				goto adv;
			}

			case 'w': case 'e': case 'r': case 's': case 'd': case 'f':
			case 'W': case 'E': case 'R': case 'S': case 'D': case 'F':
			{	/*
				 *  Left-hand key pad:  These keys function as an alternate
				 *  set of arrow keys.  This is NOT simply a convience for
				 *  lefties: It can save your bacon if you've got a serial 
				 *  console that does not generate ANSI escape codes for its
				 *  arrow keys!  Layout is like this:
				 *
				 *         +-----------+ +-----------+ +-----------+
				 *         |           | |    UP     | |           |
				 *         |  HOME (w) | | ARROW (e) | |   END (r) |
				 *         |           | |           | |           |
				 *         +-----------+ +-----------+ +-----------+
				 *
				 *         +-----------+ +-----------+ +-----------+
				 *         |   LEFT    | |   DOWN    | |   RIGHT   |
				 *         | ARROW (s) | | ARROW (d) | | ARROW (f) |
				 *         |           | |           | |           |
				 *         +-----------+ +-----------+ +-----------+
				 *
				 *  Page up is "shift-HOME", page down is "shift-end".
				 */

				if (!(slp = WidgetLoc(mup)) || (slp->type != 'S')) {
					/*
					 *  Since the left-hand keypad is made up of real text
					 *  keys, we can only interpret them as cursor movement
					 *  commands when we're not sitting on an active string
					 *  widget!
					 */

					static char *cp, keys[] = "werdWERD";
					static int inc[] = { -MAX_LIST,-1,MAX_LIST,1,-2,-1,2,1 };
			
					if (cp = strchr(keys, j)) {
						/*
						 *  Most of these keys are list scrollers.  Set the
						 *  proper cursor increment and fall into the common
						 *  "scr"olling code.
						 */

						j = inc[cp - keys];
						goto scr;

					} else {
						/*
						 *  The "s" and "f" keys are widget selectors.  Set
						 *  the increment value and fall into the common
						 *  cursor "adv"ancement code.
						 */

						j = ((j == 'S') ? -1 : 1);
						goto adv;
					}
				}

				break;     // String widget:  Use TAB to get off!
			}

			case SPACE: case BACKSPACE:
			{	/*
				 *  Space and backspace are context sensitive keys.  They 
				 *  behave diffenetly depending on the current input widget's 
				 *  type ...
				 */

				if (slp = WidgetLoc(mup)) {
					/*
					 *  ... assuming, of course, that there is a current input
					 *  input widget!
					 */

					if (slp->type == 'M') {
						/*
						 *  For multi-select lists, these keys toggle selection
						 *  of the current list element.  Note that the current
						 *  list item is always highlighted.  If selected, it
						 *  it remains highlighted (at lower intensity) when
						 *  the cursor moves off of it!
						 */

						int x = slp->data + slp->x;
						wp = getwidget(ap, slp);
						wp->in[x] = (wp->in[x] == 0);
						wx = *wp;

						toggle(mup, slp, &wx, 1);
						break;

					} else if (slp->type == 'S') {
						/*
						 *  For input string widgets, space and backspace 
						 *  are treated like any other key (well, almost).
						 */

						break;
					}
				}

				/*
				 *  In any other context (inlcuding no input widget), space
				 *  and backspace work like tab and backtab, respectively.
				 */

				j = (j == SPACE) ? 1 : -1;

 adv:           if (slp = WidgetLoc(mup)) { 
					/*
					 *  There's at least one active widget on the screen, 
					 *  which means the cursor advancing keys have something
					 *  to do!  Toggle the currently selected input widget,
					 *  find the next one, and toggle it on.
					 */

					// Unhighlight the previous input widget ...
					wx = *getwidget(ap, slp);
					toggle(mup, slp, &wx, 0);

					do {
						/*
						 *  Cycle forward or backward (depending on the value
						 *  of the "j" register) thru the widget list until
						 *  we find the next active widget.
						 */

						if ((mup->WTinput+=j) < 1) mup->WTinput = mup->WTcount;
						else if (mup->WTinput > mup->WTcount) mup->WTinput = 1;
						slp = WidgetLoc(mup);

					} while (!isactive(slp));

					// Highlight the widget we just selected!
					wx = *getwidget(ap, slp);
					toggle(mup, slp, &wx, 1);
				}

				break;
			}

			case HOME: case END:
			{	/*
				 *  Home and END keys scroll to begging or end of a list
				 *  widget (type "L" or "M").
				 */

				j = MAX_LIST * sign(-(j == HOME));
				goto scr;
			}

			case UARROW: case UPAGE:
			{	/*
				 *  Up-arrow and page-up keys scroll the current input list
				 *  upward (i.e, to lower "x" offsets).
				 */

				j = -((j == UPAGE) + 1);
				goto scr;
			}

			case DARROW: case DPAGE:
			{	/*
				 *  Down-arrow and page-down keys scroll the current input
				 *  list downward (i.e, to higher "x" offsets).
				 */

				struct ScreenLoc far *sxp;
				j = +((j == DPAGE) + 1);

 scr:           if ((slp = WidgetLoc(mup)) && islist(slp->type)) {
					/*
					 *  Scrolling keys are only valid when the current input
					 *  widget is a list of some sort.  If so, move the sel-
					 *  ected list item index ("x" field of ScreenLoc struct)
					 *  up or down the specified number of positions
					 */

					wp = getwidget(ap, slp);
					if (abs(j) == 2) j = (slp->depth * sign(j));

					if ((slp->x += j) < 0) {
						/*
						 *  Scrolled off the top of the list.  We will have
						 *  to repaint the screen after adjusting the scroll
						 *  indexes.
						 */

						if (slp->data == 0) {
							/*
							 *  If we're already on the top-most page, we 
							 *  can't scroll back -- select the first item
							 *  in the list instead.
							 */

							j -= slp->x; slp->x = 0;
							if (!j) goto bep;

						} else {
							/*
							 *  Back up to previous page and set current line
							 *  index ("slp->x") to first item on that page.
							 */

							if ((slp->data += j) < 0) slp->data = 0;
							slp->x = 0;
						}

						putwidget(mup, slp, wp, 1); // Re-display the list

					} else if (((slp->data + slp->x) >= wp->size)
					                            || (slp->x >= slp->depth)) {
						/*
						 *  We fell off the bottom of the list.  Again, we
						 *  may have to repaint the screen after adjusting
						 *  the indexes.
						 */

						if (slp->data >= (k = (wp->size - slp->depth))) {
							/*
							 *  We're already at the last page of the list,
							 *  so we can't advance beyond that.  Move the
							 *  selected item to the end of the page instead.
							 */

							k = (wp->size - slp->data) - 1;
							if ((slp->x -= j) == k) goto bep;
							slp->x = k;

						} else {
							/*
							 *  Scroll into next page and select the last
							 *  list item on that page.
							 */

							if ((slp->data += j) > k) slp->data = k;
							slp->x = min(slp->depth, (wp->size-slp->data))-1;
						}

						putwidget(mup, slp, wp, 1); // Re-display the list

					} else {
						/*
						 *  If we're just selecting another item from the
						 *  same list page, there's no need to re-display
						 *  the entire widget.  Toggling the old and new
						 *  items is enough!
						 */

						slp->x -= j; wx = *wp; toggle(mup, slp, &wx, 0);
						slp->x += j; wx = *wp; toggle(mup, slp, &wx, 1);
					}

					break;

				} else if ((sxp = slp) && (abs(j) == 1)) {

					// Unhighlight the previous input widget ...
					wx = *getwidget(ap, slp);
					toggle(mup, slp, &wx, 0);

					do {
						/*
						 *  Cycle forward or backward (depending on the value
						 *  of the "j" register) thru the widget list until
						 *  we find the next active widget that's not on the
						 *  same row as the current one!
						 */

						if ((mup->WTinput+=j) < 1) mup->WTinput = mup->WTcount;
						else if (mup->WTinput > mup->WTcount) mup->WTinput = 1;
						slp = WidgetLoc(mup);

					} while (!isactive(slp) ||
							  ((slp != sxp) && (slp->row == sxp->row)));

					// Highlight the widget we just selected!
					wx = *getwidget(ap, slp);
					toggle(mup, slp, &wx, 1);
					break;
				}

				goto bep;    // Invalid context for these keys!
			}

			default:
			{	/*
				 *  Anything else must be input text for a string widget.
				 *  Let's make sure the context is correct.
				 */

 				if (!(slp = WidgetLoc(mup)) || (slp->type != 'S')
				                            || (j < ' ') || (j > '~')) {
					/*
					 *  Text characters are only valid when the active input
					 *  is from a "S"tring widget.  We beep if this is not
					 *  the case.
					 */

 bep:				putchar(BEL);
				}

				break;
			}
		}
	}
}

void ResetMenu (char *menu, int wn)
{	/*
	 *  Reset a menu:
	 *
	 *  This routine may be used to reset a cached menu struct to it's
	 *  initial state.  This consists primarily of clearing the "WTinput"
	 *  field of the menu struct so that the default input widget reverts
	 *  to its initial state!
	 */

	int j;
	struct menu far *mup;
	struct ScreenLoc far *slp;

	if (mup = find(menu, 0)) {
		/*
		 *	Found the menu to be reset.  Set the input widget number to the
		 *  value provided by the caller and reset all list widgets.
		 */

		slp = (struct ScreenLoc far *)&mup->buffer[mup->WTindex];
		mup->WTinput = wn; // Reset input widget index

		for (j = mup->WTcount; j--; slp++) {
			/*
			 *  Search widget list looking for list widgets ...
			 */

			if (islist(slp->type)) {
				/*
				 *  .. The list and window indexes ("data" and "x" fields,
				 *  respectively) for all list widgets must be cleared to
				 *  zero!
				 */

				slp->data = 0;
				slp->x = -1;
			}
		}
	}
}

void
ClearScreen ()
{	/*
	 *  Clear the screen:
	 *
	 *  This routine may be used to clear after completing menu-driven I/O
	 *  processing.  It's also used internally by "DisplayMenu" to clear
	 *  the screen prior to switching menus.
	 */

	setup();
	row = col = 0;
	memset(mname, 0, sizeof(mname));

	_asm {
	;	/*
	;	 *  The actual screen clearing operation is performed by the
	;	 *  BIOS.  This may be intercepted and converted into a '\f'
	;	 *  character if output is redirected to a serial terminal.
	;	 */

		xor   ah, ah
		mov   al, mode
		int   10h
	}
}
