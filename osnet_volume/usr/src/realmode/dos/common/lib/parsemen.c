/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  Menu Interface (template parser):
 *
 *    This file contains the menu template parse used by the main menuing
 *    routines in "menu.c".  Seperating this code from the primary menu driver
 *    makes it possibel to pre-compile menu templates.  Note that all DOSism
 *    are contained to this file (and to "readbyte.s"), so that programs using
 *    pre-compiled menus don't need DOS file support!
 *
 *    This file also contains the DOS version of the menu subsystem keystroke
 *    reader, "_ReadByte_".
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)parsemen.c	1.3	95/05/04 SMI\n"
#define	__IMPLEMENTATION__

#include <dostypes.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <menu.h>

#define MAX_ROWS 24           // Maximum number of rows to display
static char line[256];        // Input line buffer
static int maxbufs = 16;      // Max menu buffers

extern char *_MenuErr_;	      // Ptr to error message text.
extern char *gettext(char *); // Language translater

char *_MenuDir_ = 0;
static int timeout = 0;
extern struct menu far *_MenuCache_[];

struct workbuf {
	/*
	 *  Work buffer descriptor:
	 *
	 *  The menu parser, below, converts data from the menu file into the
	 *  "menu" struct defined in the include file.  To do so, it must dynam-
	 *  ically build a number of variable-length lists which are ultimately 
	 *  copied into the "menu" record's data "buffer".  Each such list is 
	 *  described by a workbuf struct with the following format:
	 */

    char far *buf;            // Current contents live here
    int maxsize;              // Current maximum size of work buffer
    int size;                 // Actual size of work buffer
};

static int
resize (struct workbuf *wbp, int len)
{	/*
	 *  Resize a working buffer:
	 *
	 *  This routine is called when the menu file parser decides to place
	 *  "len" bytes into the working buffer at "wbp".  If the buffer is
	 *  not currently large enough to hold the extra data, we resize it
	 *  to that it is big enough.
	 *
	 *  Returns zero (with the "_MenuErr" pointer set) if we run out of 
	 *  memory.
	 */

	while ((wbp->size + len) > wbp->maxsize) {
		/*
		 *  Loop until work buffer is big enough to hold the requested data.
		 *  If we don't yet have a buffer (maxsize == 0), we malloc one;
		 *  otherwise we realloc the one we've got.  Note that the buffer
		 *  itself resides in "far" memory.
		 */

		if (!(wbp->buf = (wbp->maxsize ? _frealloc(wbp->buf, wbp->maxsize <<= 1)
		                               : _fmalloc(wbp->maxsize = 256)))) {
			/*
			 *  No memory for the new buffer.  This is a fatal error!
			 */

			_MenuErr_ = "no memory";
			return(0);
		}
	}

	return(1);
}

struct menu far *
_ParseMenu_ (char *menu)
{	/*
	 *  Read and parse menu files:
	 *
	 *  This routine is used by DisplayMenu to load menu descriptions.  It
	 *  locates the indicated "menu" file, reads it line-by-line, and builds
	 *  up a corresponding "menu" structure whose address is returned to
	 *  the caller.  Returns a null pointer (with "_MenuErr_" pointing to an
	 *  error message) if the parse fails.
	 *
	 *  Because we don't know the required size of the menu structure until
	 *  after we've parsed the entire menu file, we keep all extracted data
	 *  in one of three "workbuf" structs:
	 *
	 *     "wt"  ...  Holds "ScreenLoc" structs for all widget specs.
	 *     "bg"  ...  Holds "ScreenLoc" structs for background text
	 *     "txt" ...  Holds all text strings taken from the file
	 */

	int j, k;
	FILE *fp;
	char *cp, flag = 0;
	int col, row = 0, lc = 0;
	struct menu far *mup = 0;
	struct ScreenLoc far *slp;
	static struct workbuf bg, wt, txt;

	if (!_MenuDir_) {
		/*
		 *  If we don't know menu directory yet, build one dynamically by
		 *  translating the default menu directory via gettext.
		 */

		static char mdir[32];
		sprintf(_MenuDir_ = mdir, "%s\\%s", ROOT_DIR, gettext(MENU_DIR));
	}

	_MenuErr_ = (char *)0;
	bg.size = wt.size = txt.size = 0;
	sprintf(line, "%s\\%s", _MenuDir_, menu);

	if ((strlen(menu) >= sizeof(mup->name)) || ((cp = strchr(menu, '.')) 
					   && ((strlen(cp) > 4) || (cp != strrchr(menu, '.'))))) {
		/*
		 *  Menu name has to be a legal DOS filename.  The only validity
		 *  checking we do, however, is to insure that it's not too long
		 *  and that it has no more than one dot in it!
		 */

		_MenuErr_ = "invalid menu name";
		return(0);
	} 

	if (!(fp = fopen(line, "r"))) {
		/*
		 *  We can't do anything without a menu file!
		 */

		_MenuErr_ = "can't open menu file";
		return(0);
	}

	while (!_MenuErr_ && fgets(line, sizeof(line), fp)) {
		/*
		 *  Read and parse each line of the menu file.  Error handling isn't
		 *  very sophisticated:  We give up upon detecting a syntax error in
		 *  the menu template!
		 */

		char *cp = line; // Points to next input char
		lc += 1;         // Line count for used in error messages.

		if ((line[0] != '%') || (toupper(line[1] != 'X'))) {
			/*
			 *  This is not a comment line, which means it may contain
			 *  background text, widget specifiers, or both.  Extract 
			 *  this data and convert it to ScreenLoc form.
			 */

			if (!strchr(line, '\n')) {
				/*
				 *  The parser is pretty stupid.  It will get very confused
				 *  if the terminating newline doesn't make it into the input
				 *  buffer!
				 */

				sprintf(_MenuErr_ = line, "line %d too long\n", lc);
				break;
			}

			if (row > (MAX_ROWS-1)) {
				/*
				 *  No more than 24 lines are allowed per menu.  If the
				 *  output row number exceeds this, we've got a problem!
				 */

				_MenuErr_ = "menu file too long";
				break;
			}

			for (col = 0; *cp && !_MenuErr_; cp++) {
				/*
				 *  Parse out next ScreenLoc structure.  With each iteration
				 *  we extract a widget specifier, a background text string,
				 *  or both.  The "cp" register advances thru the menu text,
				 *  "col" gives the next ScreenLoc column position.
				 */

				struct ScreenLoc far *sxp = 0;
				char *xp, far *np;

				while (*(xp = cp) && isspace(*cp)) {
					/*
					 *  Skip over any leading whitespace, advancing the
					 *  column indicator as we go.  Note that this is the
					 *  only place we do tab expansion!
					 */

					if (*cp++ == '\t') col = ((col + 8) & ~7);
					else col += 1;
				}

				while (cp = strchr(cp, '%')) {
					/*
					 *  Scan forward to the next widget specifier.  The
					 *  presence of a percent sign does not neccessarily
					 *  mean we have a specifier, tho -- there's always
					 *  comments to deal with!
					 */

					if (!resize(&wt, sizeof(struct ScreenLoc))) return(0);
					sxp = (struct ScreenLoc far *)&wt.buf[wt.size];
					_fmemset(sxp, 0, sizeof(*sxp));
					wt.size += sizeof(*sxp);
					*cp++ = 0;

					if ((sxp->type = toupper(*cp++)) == 'X') {
						/*
						 *  If this is a comment specifier, mark the line
						 *  terminated at the '%'!
						 */

						wt.size -= sizeof(*sxp);
						*cp-- = 0;
						sxp = 0;
					} 

					if (sxp->type != '%') break;
					wt.size -= sizeof(*sxp);
					sxp = 0;
					cp--;

					memcpy(cp-1, cp, strlen(cp)+1);
				}

				if (!sxp) {
					/*
					 *  No widget specifier on this line.  Trim any white
					 *  space from the right side of the background text.
					 */

					for (cp = strchr(xp, 0); isspace(*--cp); *cp = 0);
				}

 				if ((j = strlen(xp)) && resize(&txt, (j+1))
				                     && resize(&bg, sizeof(struct ScreenLoc))) {
					/*
					 *  Well, there is some background text -- it's "j" bytes
					 *  long and starts at "xp".  Copy this text into the "txt"
					 *  work buffer and build a temporary ScreenLoc struct
					 *  in the "bg" buffer to describe it.  Both work buffers
					 *  are copied into the "menu" struct later.
					 */

					slp = (struct ScreenLoc far *)&bg.buf[bg.size];
					_fmemset(slp, 0, sizeof(*slp));
					bg.size += sizeof(*slp);

					slp->row = row; slp->col = col;
					slp->width = j;
					col += j;

					for (np = txt.buf; np < &txt.buf[txt.size]; np += k+1) {
						/*
						 *  Background text tends to be highly redundant.  If
						 *  we there's a copy of the string we're looking for
						 *  in the text buffer already, use it rather than
						 *  allocate a new one.
						 */

						if ((j == (k = _fstrlen(np))) && !_fstrcmp(np, xp)) {
							/*
							 *  Well what do you know!  We just saved "j" bytes
							 *  of memory.
							 */

							slp->data = (np - txt.buf);
							break;
						}
					}

					if (np >= &txt.buf[txt.size]) {
						/*
						 *  So much for that idea.  Looks like we have no 
						 *  choice but to copy the text string from the menu
						 *  file to the text buffer.
						 */

						_fstrcpy(&txt.buf[txt.size], xp);
						slp->data = txt.size;
						txt.size += (j+1);
					}
				}

				if (sxp != 0) {
					/*
					 *  We found a widget specifier behind the background
					 *  text.  Finish building the ScreenLoc structure that
					 *  describes it.
					 */

					if (((int)(sxp->num = strtol(cp, &cp, 10)) <= 0)
					|| (*cp++ != ':')
					|| ((int)(sxp->width = strtol(cp, &cp, 10)) <= 0)) {
						/*
						 *  If we get here it means that either the widget
						 *  number "sxp->num" or the field width ("sxp->width")
						 *  is bogus (or missing altogether!).
						 */

						goto err;
					} 

					switch (sxp->type) {
						/*
						 *  Put the final touches on the ScreenLoc structure
						 *  based on widget type.
						 */

						case 'B': case 'S':
						{	/*
							 *  Buttons and string data may be specified in 
							 *  the arg list or in the menu file.  If the 
							 *  former, we've nothing more to do ...
							 */

							if (*cp == ':') {
								/*
								 *  ... but in the latter case we have to
								 *  copy the label into the "txt" buffer and
								 *  set the SreenLoc relocation index.  We
								 *  use the high order bit of "sxp->data" to
								 *  flag this for subsequent relocation --
								 *  we'll turn it off later.
								 */

								if (!(cp = strchr(xp = ++cp, '%'))) goto err;
								if (!resize(&txt, (j=cp-xp)+1)) return(0);
								_fmemcpy(&txt.buf[txt.size], xp, j);
								sxp->data = (txt.size | 0x8000);
								txt.buf[txt.size += j] = 0;
								txt.size++;
							}

							break;
						}

						case 'L': case 'M':
						{	/*
							 *  List widgets require a "depth" field.  Make
							 *  sure its there and that it's valid.
							 */

							if ((*cp++ = ':')
							&& ((int)(sxp->depth = strtol(cp, &cp, 10)) > 0)
							&& ((row + sxp->depth) <= MAX_ROWS)) {
								/*
								 *  The extracted depth looks reasonable so
								 *  bail out.  We fall throurgh into the 
								 *  error message generator if we can't find
								 *  a valid depth.
								 */

								sxp->x = -1;
								break;
							}

							/*FALLTHROUGH*/
						}

						default:
						{	/*
							 *  Bogus widget type; off to the common error
							 *  handler.
							 */

							goto err;
						}
					}

					// Ignore any excess stuff at end of widget spec!
					while (*cp && (*cp != '%')) cp++;
					sxp->row = row; sxp->col = col;
					col += sxp->width;

					if (!*cp && !_MenuErr_) {
						/*
						 *  All syntax errors land here.  The message isn't
						 *  very helpful, but it's better than nothing!
						 */

 err:					sprintf(line, "bad widget spec (%%%c), line %d",
						                              sxp->type,    lc);
						_MenuErr_ = line;
						break;
					}
				}
			}

			row += 1;   // Advance row specifier

		} else if ((lc == 1) && (line[2] == 'X')) {
			/*
			 *  If the first line is a comment beginng with "%XX" rather 
			 *  than "%X", it means the menu is not swappable.  Yeah, it's
			 *  a hack - but it works!
			 */

			flag = MF_RESIDENT;
		}
	}

	if (_MenuErr_ == 0) {
		/*
		 *  We were able to successfully parse the menu file, now build a
		 *  "menu" struct to hold all the data we extracted from it.  The
		 *  "j" register gives the size of this struct, "col" is the text
		 *  relocation factor.
		 */

		col = bg.size + wt.size + 1;
		j = col + txt.size + sizeof(struct menu) - 1;

		if (maxbufs <= 0) {
			/*
			 *  We've reached (or exceeded) our quota of menu buffers.  Find
			 *  the oldest non-resident menu structure in the menu cache (if
			 *  there is one) and use it to hold the newly extracted data.
			 */

			for (mup = _MenuCache_[1]; mup != GROUND; mup = mup->prev) {
				/*
				 *  Follow "prev" links thru the menu cache to locate the
				 *  oldest buffer that suits our needs.
				 */

				if (!(mup->flags & MF_RESIDENT)) {
					/*
					 *  Found a likely candidate.  Remove if from the cache
					 *  list and bail out.
					 */

					mup->next->prev = mup->prev;
					mup->prev->next = mup->next;
					break;
				}
			}
		}

		if (mup && (mup != GROUND)) {
			/*
			 *  We're about to re-use an existing menu buffer.  Make sure
			 *  it's big enough to hold all the data we want to put into
			 *  it ...
			 */

			if ((j > mup->length) || (flag && (j < mup->length))) {
				/*
				 *  If the existing buffer isn't big enough to hold the
				 *  new menu data, free it up and nullify the "mup" pointer.
				 *  This will force us to allocate a new buffer, below.
				 */

				_ffree(mup);
				mup = 0;

			} else {
				/*
				 *  If the existing buffer is larger than we need, change
				 *  the length register to reflect this so we don't forget
				 *  about the extra memory later!
				 */

				j = mup->length;
			}

		} else {
			/*
			 *  We couldn't find a resuable buffer in the cache.  We'll have
			 *  to allocate a new one, even if we've exceeded our quota!
			 */

			mup = 0;
		}

		if (mup || (maxbufs--, mup = (struct menu far *)_fmalloc(j))) {
			/*
			 *  The "mup" register now points to a menu buffer large enough
			 *  to hold all the data we extracted from the menu file.  All
			 *  we need do now is copy the extracted data into this buffer!
			 */

			_fmemset(mup, 0, sizeof(struct menu));
			_fstrcpy(mup->name, menu);
		    mup->flags = flag;
			mup->length = j;
			j = 1;

			if (bg.size) {
				/*
				 *  We have background text!  Copy the background ScreenLoc's
				 *  from the "bg" work buffer to the "menu" struct's variable
				 *  length data buffer and set BGcount accordingly!
				 */

				mup->BGcount = bg.size / sizeof(struct ScreenLoc);
				slp = (struct ScreenLoc far *)&mup->buffer[1];
				_fmemcpy(slp, bg.buf, bg.size);
				mup->BGindex = j;
				j += bg.size;

				for (lc = mup->BGcount; lc--; slp++) {
					/*
					 *  Now relocate the "data" indexes in all the background
					 *  ScreenLoc structs!
					 */

					slp->data += col;
				}
			}

			if (wt.size) {
				/*
				 *  We've got widget specifiers to copy in.  These go behind
				 *  the background ScreenLoc's (if any) - the "j" register
				 *  gives the proper position in the output buffer.
				 */

				mup->WTcount = wt.size / sizeof(struct ScreenLoc);
				slp = (struct ScreenLoc far *)(mup->buffer + j);
				_fmemcpy(slp, wt.buf, wt.size);
				mup->WTindex = j;
				j += wt.size;

				for (lc = mup->WTcount; lc--; slp++) {
					/*
					 *  Now relocate the "data" pointers of all widgets with
					 *  file-specified labels.  (This is where we turn off
					 *  the high-order flag bit noted previously).
					 */

					if (slp->data) slp->data = ((slp->data & 0x7FFF) + col);
				}
			}

			if (txt.size) {
				/*
				 *  Finally, copy the text itself into the "menu" struct.
				 *  Once again, the "j" register gives the poisition in the
				 *  menu buffer where the extracted data belongs.
				 */

				_fmemcpy(mup->buffer+j, txt.buf, txt.size);
			}
			
		} else {
			/*
			 *  Couldn't get memory for a new "menu" struct.  The Caller 
			 *  probably has too many resident templates!
			 */

			_MenuErr_ = "no more memory";
		}
	}

	fclose(fp);   // Close the menu file before returning to caller!
	return(mup);
}

int
_GetHelp_ (char *menu)
{	/*
	 *  Help processing:
	 *
	 *  This routine is called when the user presses the HELP key.  It prints
	 *  any help information available for the current screen context, and
	 *  holds it on the screen until the users enters a <CR>.
	 *
	 *  The help system is pretty stupid:  We allow only one help screen
	 *  per menu:  No scrolling, no paging, no hypertext links, no nothing!
	 *  Help is little more that "cat help-file" (the little more is that
	 *  we figure out the help-file name).
	 */

	int c;
	FILE *fp;

	// Build help file name before clearinig the screen!
	sprintf(line, "%s\\%s\\%s", ROOT_DIR, gettext(HELP_DIR), menu);
	ClearScreen(); _SetCurPos_(0, 0);

	if (fp = fopen(line, "r")) {
		/*
		 *  Found the help file!  Read it in and print it right back out.
		 *  Note that we don't bother to check the file size -- if it's too
		 *  long stuff will just scroll off the top of the screen!
		 */
		
		while (fgets(line, sizeof(line), fp)) {
			/*
			 *  Nor do we check that we were able to read all of the current
			 *  line (although this won't be a problem if the receiving
			 *  terminal is in line-wrap mode).
			 */

			if ((line[0] != '%') || (toupper(line[1]) != 'X')) {
				/*
				 *  Any line that begins with the characters "%X" is 
				 *  assumed to be a help file comment.  We don't print
				 *  these!
				 */

				printf("%s", line);
			}
		}

		fclose(fp);   // We're done with help file -- close it!

	} else {
		/*
		 *  No help file.  Give user the bad news ...
		 */

		printf(gettext("\n\n\t\t\t\tSorry, no help!\n\n"));
	}

	_SetCurPos_(23, 0);
	printf(gettext("<ENTER> to continue "));

	while(((c = _ReadByte_()) != '\r') && (c != 0x1B));
	_SetCurPos_(25, 25);
	return(0);
}

void SetMenuTimeout (int secs)
{	/*
	 *  Set menu timeout:
	 *
	 *    This is almost too simple for words.  The static "timeout" word
	 *    gives the number of seconds that "_ReadByte_" is willing to wait
	 *    before returning <ENTER>.
	 */

	timeout = secs;
}

int _ReadByte_ ()
{	/*
	 *  Read next keystroke:
	 *
	 *    This routine polls the keyboard for "timeout" seconds (indefinately
	 *    if "timeout" is less than or equal to zero).  If a key is struck
	 *    during that time, the corresponding ASCII value is returned to the
	 *    caller.  DOS encodings of multi-byte keystroke sequences (e.g, arrow
	 *    keys) are used.
	 *
	 *    If the timeout expires before a key is entered, we return <ENTER>
	 */

	int c; // return value
	time_t deadline = ((timeout > 0) ? (time(0) + timeout) : 0);

	while (deadline && !kbhit()) if (time(0) >= deadline) {
		/*
		 *  Deadline expired before a keystroke appeared.  Deliver a carriage
		 *  return (<ENTER> key) as return value.
		 */

		return('\r');
	}

	_asm {
	;	/*
	;	 *  Use the "unbuffered/no echo" version of the DOS "getchar" system
	;	 *  call to read the next keystroke.
	;	 */

		mov   ah, 7h
		int   21h
		xor   ah, ah
		mov   c, ax
	}

	return(c);
}
