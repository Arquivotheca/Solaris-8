/*
 *  Copyright (c) 1995 by Sun Microsystems, Inc.  All Rights Reserved.
 *
 *  X86 Realmode Device Driver Simulation:
 *
 *    This file contains routines that can be used to simulate the execution
 *    of a Solaris x86 realmode device driver.  It's purpose is to provide
 *    an environment in which to test the server (framework) side of the
 *    ".bef" interface.  Entry into this module is via exported routines
 *    defined in <befext.h>:
 *
 *        LoadBef()  ...  To "load" a driver into memory
 *        FreeBef()  ...  To unload a driver loaded via "LoadBef"
 *        CallBef()  ...  To invoke a driver at one of its three entry points
 *
 *    Driver simulation is controlled by a "bef script" file.  This is an
 *    ASCII file that associates devices with drivers and specifies how they
 *    should appear to be configured.  There are two types of "records" in
 *    the bef script:
 *
 *        Driver Records:  Records of this type identify the drivers to be
 *                         simulated.  A driver record is indicated by the
 *            first field on a line starting with an alpha character.  This
 *            field is assumed to contain the driver file name immediately
 *            followed by a colon (e.g, "elx.bef:").
 *
 *        Device Records:  One or more device records may follow a driver
 *                         record.  These represent instances of devices
 *            under the control of the current driver.  A device record is
 *            indicated when the first field of a line starts with a numeric
 *            character.  This field gives the I/O address at which the de-
 *            vice is assumed to reside, and must terminate with a colon
 *            (e.g, "0x3F8:").  Any number of additional fields may follow
 *            the I/O port specification.  These represent additional re-
 *            srouces required by the device.
 *
 *    Consider the following example:
 *
 *        elx.bef:
 *           0x330: irq=9 dma=1 mem=0xDC000[256]
 *
 *    This script entry indicates that a 3COM Eithernet card is assumed to
 *    reside at port 330, using irq number 9, dma channel number 1, and a
 *    256-byte I/O buffer at location 0xDC000.
 *
 *    The general form of a device resource specification is as follows:
 *
 *         <type>=<base>[<len>][{; | ,} ...]
 *
 *    Where <type> is the resource type (one of "irq", "mem", "dma", "slot", 
 *    or "name") and <base> is the "base address" of the corresponding re-
 *    source.  Base units for the various resource types are:
 *
 *          irq     ...  IRQ number
 *          dma     ...  DMA channel number
 *          slot    ...  ESCD Slot number (or -1 for next empty slot)
 *          mem     ...  Memory address
 *          name    ...  EISA device (board) name
 *          vid     ...  Vendor ID information
 *
 *    I/O ports are also resources and may be specified in the same manner,
 *    except that the "<type>=" part of the specification must be omitted.
 *    The base units for I/O ports are I/O addresses.
 *
 *    I/O and memory resources may also have an optional "len" specification,
 *    which gives the number of units to be reserved.  The len specification
 *    may be coded as "[<units>]" or "-<base+units-1>", where <units> is the
 *    number of units to be reserved.  The default len for memory and I/O
 *    reservations is 1.
 *
 *    Multiple resource assignments may be specified in the same field by
 *    separating them by one of {";", "*", "&"} or {",", "+", "|"}.  The
 *    former separators are used to indicate that all of the listed re-
 *    sources must be available before the device can be configured, the
 *    latter are used to indicate that any one of the listed resources can
 *    be used to configure the device.  These are called "additive" and
 *    "alternative" resource reservations, respectively.  One may mix 
 *    additive and alternative reservations by specifying them one after
 *    another, eg:
 *
 *          irq=1,3,5  irq=0,2,4
 *
 *    indicates that the device needs two IRQs numbered below 6, one of which
 *    must be odd and one of which must be even.
 *
 *    The "name" and "vid" resources are treated somewhat differently.  The
 *    base "unit" for name resources are actually the EISA device name.
 *    Normally, this name is obtained by a translation of the driver name taken
 *    from the Soalris realmode device database.  Use of the "name" resource
 *    allows the driver to change its default name assignment if the need 
 *    arises.
 *
 *    The base unit for "vid" resources is an ASCII string which is copied
 *    into the ESCD as-is.  Double quotes may be used to enclose white space
 *    in the vendor ID string if desired.
 *
 *    NOTE: It's easier to read this with tabs stops set at 4!
 */

#ident "@(#)befake.c	1.6	95/04/15 SMI\n"
#include <befext.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#define	BEFSCRIPT "befscrpt"  // Name of .bef script file
static FILE *fp = 0;          // File pointer open on script file
static int fx = 0;            // Non-zero when a script is active

#define SYNTAX (-101)         // Code for input syntax error

static char line[2048];       // Input buffer for scripts.
static char *nfp = 0;         // Next field pointer.
static int   ln;              // Next line number

int maxlen = 0;               // Max value of "len" field
DWORD _far *val;              // Ptr to RES_SET/RES_GET value buffer
DWORD len;                    // Length of value buffer (in longs)
int flg;                      // Resource flags

static void
resize (int n)
{	/*
	 *  Resize the value buffer:
	 *
	 *  This routine checks to see if the global value buffer (which is used
	 *  to pass arguments to the driver framework callbacks) is big enough
	 *  to hold "n" more arguments.  If not, we "realloc" it so that it is
	 *  large enough
	 */

	while (maxlen < (len + n)) {
		/*
		 *  If the callback value buffer isn't bigenough to hold "n" more
		 *  words, resize it now.
		 */

		if (!(val = ((maxlen == 0)
			? (DWORD far *)_fmalloc((maxlen = 16) * sizeof(DWORD))
		    : (DWORD far *)_frealloc(val, (maxlen <<= 1) * sizeof(DWORD))))) {
			/*
			 *  Realloc failure.  This is a pretty serious error (after all
			 *  we don't even have the ".bef" file loaded, and we're run-
			 *  ning out of "far" memory)!
			 */

			printf("\n*** No memory for callback parameter list ***\n");
			exit(100);
		}
	}
}

static char *
next_line ()
{	/*
	 *  Read next input line:
	 *
	 *  This routine reads the next line from the .bef script file, con-
	 *  catenates any continuation lines that may be inidicated, and returns
	 *  a pointer to the line buffer.  It returns a null pointer upon en-
	 *  countering end-of-file.
	 */

	char *cp = line;
	nfp = line;

	while (fgets(cp, &line[sizeof(line)]-cp, fp)) {
		/*
		 *  Read the next line into the global "line" buffer and advance
		 *  line number.  Set "xp" register to point to the end of the
		 *  line.
		 */

		char *xp = strchr(cp, '\0');
		ln += 1;

		if (xp[-1] == '\n') {
			/*
			 *  Line is properly terminated, which means it fit into the
			 *  input buffer.  It doesn't mean that it's not marked for
			 *  continuation, however.  Check that next ...
			 */

			xp -= (xp[-2] == '\r');  // Back up over newline char(s)
			*--xp = '\0';            // Remove newline char(s)

			if (!(xp = strrchr(cp, '\\')) || xp[1]) {
				/*
				 *  Line is not continued.  Return the line buffer address
				 *  to the caller.
				 */

				return(line);
			}
		}

		if ((&line[sizeof(line)] - (cp = xp)) < 8) {
			/*
			 *  The entire line will not fit in the line buffer (probably
			 *  because it has too many continuations, but not neccessarily).
			 *  Print an error message and truncate remaining text.
			 */

			int c, x = '\\';
			printf("\n*** %s: line %d too long\n", BEFSCRIPT, ln);

			for (c = x; (c != EOF) && (x == '\\'); ln++) {
				/*
				 *  Sink all characters up to the next newline that is NOT
				 *  preceeded by a backslash.  Outer loop counts input lines,
				 *  inner loop looks for the newline chars.
				 */

				while (((c = getc(fp)) != EOF) && (c != '\n')) {
					/*
					 *  The "x" register always holds the last character we
					 *  looked at (not counting DOS carriage returns).  If,
					 *  upon reaching the end of the line, x contains a 
					 *  backslash, we know the line is continued.
					 */

					x = ((c != '\r') ? c : x);
				}
			}

			*cp = 0;
			return(line);	// Return truncated line to caller.
		}
	}

	return(nfp = 0);        // We've reached EOF!
}

static char *
next_field ()
{	/*
	 *  Isolate next input field:
	 *
	 *  This routine scans the current input line looking for the next
	 *  (whitespace delimited) input field.  Upon finding an input field,
	 *  places a terminating null character behind the field's last character
	 *  and returns the address of the first character of the field to the
	 *  caller.  Returns a null pointer if there are no more input fields
	 *  on the current line.
	 */

	int qt = 0;
	char *cp = 0;

	if (nfp != 0) {
		/*
		 *  Process only up to the end of the current line ...
		 */

		while (*nfp && isspace(*nfp)) {
			/*
			 *  If we haven't found the first non-white character on this
			 *  line yet, advance the input pointer until we do find it.
			 */

			nfp += 1;
		}

		if (*(cp = nfp) && (*nfp != '#')) {
			/*
			 *  The "cp" register now points to the first character of the
			 *  next field.  Check each character between here and the 
			 *  termination character for this field.
			 */

			while (*nfp && (qt || !isspace(*nfp))) switch(*nfp) {
				/*
				 *  Terminator is either the null character at the end of
				 *  the line buffer or an unquoted whitespace character.
				 *  Everything else is part of the input field (maybe).
				 */

		    	case '"':
				{	/*
					 *  Next char is a quote mark.  Remove it from the input
					 *  buffer and flip the "qt" toggle.
					 */

					strcpy(nfp, nfp+1);
					qt ^= 1;
					break;
				}

				case '#':
				{	/*
					 *  Next character is a comment indicator.  If it's not
					 *  quoted, treat it as if it were the line terminator.
					 */

					if (qt) nfp += 1;
					else *nfp = 0;
					break;
				}


				case '\\':
				{	/*
					 *  Next character is escaped.  Remove the backslash
					 *  from the input buffer and check the following char.
					 */

					strcpy(nfp, nfp+1); 

					switch (*nfp) {
						/*
						 *  Translate escaped characters.  Most characters
						 *  translate to themselves.  Note that we fall thru
						 *  to the "default" arm of the outer case after the
						 *  translation is complete.
						 */

						case 'r': *nfp = '\r'; break;
						case 'n': *nfp = '\n'; break;
						case 'b': *nfp = '\b'; break;
						case 'f': *nfp = '\f'; break;
						case 't': *nfp = '\t'; break;
					}
				}

				default:
				{	/*
					 *  A "normal" character.  Include it in the current
					 *  field by incrementing the next field pointer.
					 */  

					nfp += 1;
					break;
				}
			}

			if (*nfp) *nfp++ = 0;  // Make sure field is null terminated
			return(cp);            // Return ptr to next field.
		}

		nfp = 0;                   // Clear next field ptr at end of line.
	}

	return((char *)0);             // No more fields on this line!
}

#define prev_field(cp)                                                        \
{   /*                                                                        \
	 *  Back up a field:                                                      \
     *                                                                        \
     *  This routine pushes the text field at "cp" back on the input queue.   \
	 *  It only works within a device clause.                                 \
     */                                                                       \
                                                                              \
	*strchr(nfp = (cp), 0) = ' ';                                             \
}

static char *
next_device ()
{	/*
	 *  Locate next device clause:
	 *
	 *  Each driver record may have multiple device clauses, each one de-
	 *  scribing a device at a particular bus address.  The start of a device
	 *  clause is indicated by a "bus address" field whose first character
	 *  must be numeric.  
	 *
	 *  This routine checks the next input field.  If its first character
	 *  is numeric, we assume that it's a bus address and return its address
	 *  to the caller.  If the first character of the next field is non-
	 *  numeric, we assume we're looking at the next driver record and re-
	 *  turn a null pointer.
	 */

	char *cp, *xp;

	do
	{	/*
		 *  Upon entry to this routine, the next field pointer ("nfp") is 
		 *  either:
		 *
		 *    (a).  Pointing at first field beyond driver name
		 *    (b).  Pointing at first field of a new line
		 */
     
		if (cp = next_field()) {
			/*
			 *  Check the next field to determine whether or not it starts
			 *  with a digit.  If so, we assume it's a device address ...
			 */

			if (isdigit(*cp)) {
				/*
				 *  This appears to be a device address field, so there should
				 *  be a colon immediately following it.  Scan for it just
				 *  to be sure.
				 */

				if (((*(xp = (strchr(cp, '\0') - 1)) == ':') && !(*xp = 0))
				|| ((xp = next_field()) && (*xp == ':') 
				&& (!xp[1] || ((!nfp || (nfp[-1] = ' ')) && (nfp = xp+1))))) {
					/*
					 *  Found the colon!  Remove it from the bus address
					 *  field and return bus address to caller.
					 */

					*xp = '\0';
					return(cp);
				}

				printf("\n*** %s: syntax error, line %d ***\n", BEFSCRIPT, ln);

			} else {
				/*
				 *  We assume that the caller does not invoke next_device()
				 *  until all resource fields for the current device have
				 *  been processed.  Thus, a non-digit in the first character
				 *  of the next field indicates the start of a driver record!
				 */

				prev_field(cp);
				break;
			}
		}

	} while (next_line());

	return(0);		// No more devices specified for current driver
}

static char *
next_driver ()
{	/*
	 *  Locate next driver record:
	 *
	 *  This routine searches the driver script file for the next driver
	 *  record and returns its name.  It returns a null pointer upon reaching
	 *  the end of the script file.
	 */

	char *cp, *xp;
	static int active = 0;

	if (active) {
		/*
		 *  If we've already found a driver, advance to input past all the
		 *  devices listed for the current driver before checking for the
		 *  next one.
		 */

		while (next_device());
	}

	while (active || (nfp = next_line())) {
		/*
		 *  Read each input line looking for a driver name.  Driver names
		 *  are distinquished by the fact that that they're followed by a
		 *  colon and their first character is alphanumeric!
		 */

		active = 0;	// Driver no longer active

		if ((cp = next_field()) && (isalpha(*cp))) {
			/*
			 *  Looks good so far.  Lets see if the current field is term-
			 *  inated by a colon ...
			 */

			if ((*(xp = (strchr(cp, '\0') - 1)) = ':')
			                   || ((xp = next_field()) && (*xp == ':'))) {
				/*
				 *  Yep, there's a colon following the next input field.
				 *  Make sure the driver name is null terminated, then con-
				 *  vert it to upper case before returning its address to
				 *  the caller.
				 */

				if (xp[1] != 0) {
					/*
					 *  No space between the colon and the next input field.
					 *  Adjust the next field pointer ("nfp") so it points
					 *  just past the separating colon.
					 */

					if (nfp--) *nfp = ' ';
					nfp = xp+1;
				}

				active++;			// Mark driver active
				*xp = '\0';
				for (xp = cp; *xp; *xp++ = toupper(*xp));

				return(cp);			// Return driver name
			}
		}
	}

	return((char *)0);	// Can't find driver script!
}

static int
find_bef (char *name)
{	/*
	 *  Find driver script:
	 *
	 *  This routine searches the .bef script file for an entry labeled with
	 *  the given "name".  It returns a non-zero value (and sets up the in-
	 *  ternal state to simulate this driver) if we find such an entry.
	 *
	 *  NOTE: Multiple entries with the same label may appear in the script
	 *        file.  If the application loads the same driver multiple times
	 *        in succession, subsequent entries in the script file will be
	 *        used to simulate it.
	 */

	int j;
	char *cp;

	for (j = 2; j-- > 0; (rewind(fp), ln = 0)) {
		/*
		 *  We start searching for the requested driver at the current file
		 *  position.  The entry we want may be ahead of us in the file,
		 *  however, so we rewind upon reaching the end of the file and scan
		 *  again from the beginning.
		 */

		while (cp = next_driver()) {
			/*
			 *  Check each driver name in the script file to see if it matches
			 *  the name provided by the caller.  If so, return success.
			 */

			if (!strcmp(cp, name)) return(1);
		}
	}

	return(0);	// Can't find the driver!
}

char far *
LoadBef (char *file)
{	/*
	 *  Load a realmode driver:
	 *
	 *  This routine simulates loading of a x86 realmode driver.  It searches
	 *  the .bef script file for the entry corresponding to the driver "file"
	 *  provided by the caller.  If such an entry exists, it sets up some
	 *  internal state information and returns a pointer to a dummy entry
	 *  point which contains a vaild .bef magic number.
	 *
	 *  A null pointer is returned if something goes wrong.
	 */

	if (fx != 0) {
		/*
		 *  Caller is trying to load a new .bef without first unloading the
		 *  previous one.  We'll be nice about this and unload the current
		 *  .bef on the caller's behalf, but we're going to complain about
		 *  it!
		 */

		printf("\n*** Loading .bef before freeing previous one ***\n");
		FreeBef();
	}

	if (fp || (fp = fopen(BEFSCRIPT, "r"))) {
		/*
		 *  We've successfully opened the simulator's database.  Now search
		 *  it looking for the entry describing the caller's driver.
		 *
		 *  NOTE: We only match against the final path component of the driver
		 *        name.  We can't simulate different directories containing
		 *        drivers with the same name!
		 */

		char *cp = strrchr(file, '\\');
		if (!cp++) cp = file;

		if (fx = find_bef(cp)) {
			/*
			 *  Found it!  Build a dummy driver text segment that contains
			 *  nothing more than a valid magic number and return a pointer
			 *  to this "segment" to the caller.
			 */

			static char beftxt[BEF_EXTMAGOFF + sizeof(long)];
			*(long *)&beftxt[BEF_EXTMAGOFF] = BEF_EXTMAGIC;
			return((char far *)beftxt);

		} else {
			/*
			 *  We don't have a simulation script for the specified driver.
			 *  Print an error message and bail out.
			 */

			printf("\n*** Can't find %s in %s ***\n", cp, BEFSCRIPT);
		}

	} else {
		/*
		 *  There's no script file (or at least, we can't open it)!  Print
		 *  error message and bail out!
		 */

		printf("\n*** Can't open %s ***\n", BEFSCRIPT);
	} 

	return((char far *)0);	// Error return
}

void
FreeBef ()
{	/*
	 *  Release driver resources:
	 *
	 *  Since we didn't load a real driver, we have no memory resources to
	 *  dispose of.  All we do here is make sure caller is obeying the
	 *  interface protocol.
	 */

	if (!fx) printf("\n*** Attempt to free unloaded .bef ***\n");
	fx = 0;
}

static int
GetRes (char *cp, int ag)
{	/*
	 *  Parse resource descriptor field:
	 *
	 *  This routine is used to convert the ASCII bus resource specification
	 *  at "cp" into a suitable binary form and store the result in the global
	 *  value buffer.  A non-zero "ag" argument is used to indicate aggre-
	 *  gate resources.  
	 *
	 *  Multiple resources of each type may be specified, and these may
	 *  be additive (all must be reserved) or alternate (any may be reserved).
	 *  Resource specifiers separated by "*", "&", or ";" indicate the former,
	 *  specifiers separated by "+", "|", or "," indicate the latter.
	 *
	 *  Routine returns a non-zero value upon detecting a syntax error.
	 */

	int n = 0;
	char *xp;

	for (len = flg = 0; *cp; cp = xp) {
		/*
		 *  Process to end of field (or until we detect a syntax error).  When
		 *  we're done, "len" will contain the number of long words placed in
		 *  the "val"ue buffer and "flg" will contain the reservation type
		 *  flag.
		 */

		resize(ag+2);                       // Make sure we have room!

		val[len++] = strtol(cp, &xp, 0);	// Get first value word
		if (cp == xp) return(SYNTAX);		// .. and verify it!

		if (ag != 0) {
			/*
			 *  If this is a range-type resource (e.g, "io" or "mem"), the
			 *  second value word may be specified in one of three ways:
			 */

			if (*xp == '-') {
				/*
				 *  XXX-YYY: The second value word gets the length, which we
				 *           obtain by subtracting XXX from YYY (and adding 1).
				 */

				val[len++] = strtol(xp+1, &xp, 0);
				if ((long)(val[len-1] -= (val[len-2]-1)) <= 0) return(SYNTAX);

			} else if (*xp == '[') {
				/*
				 *  XXX[NN]: The second value word gets the length, whish is
				 *           simply "NN".
				 */

				val[len++] = strtol(xp+1, &xp, 0);
				if (((long)val[len-1] <= 0) || (*xp++ != ']')) return(SYNTAX);

			} else {
				/*
				 *  XXX:     The second value word gets the default length 
				 *           of one.
				 */

				val[len++] = 1;
			}
		}

		if (*xp) switch (*xp++) {
			/*
			 *  Look for a resource separator.  The value of the separator
			 *  character tells us what the resouce flag should be:
			 */

			case '+':   // Alternate resources, use RES_ANY flag
			case '|':
			case ',':	if (!*xp || (n && !flg)) return(SYNTAX);
						flg = RES_ANY;
						break;

			case '*':	// Additive resources, use null flag word
			case '&':
			case ';':	if (!*xp || flg) return(SYNTAX);
						break;

			default:    return(SYNTAX);
		}

		val[len++] = 0; // No EISA resource flags!
	}

	return(0);
}

static int
GetNam (char *np)
{	/*
	 *  Process "name" resource
	 *
	 *  Syntax of the name resource differs from all the others.  The value
	 *  is assumed to be an 8-character EISA board ID which we convert to
	 *  the compressed form and palce in the global value buffer.
	 *
	 *  Returns a non-zero value if we detect a syntax error.
	 */

	int j = 0;

	while (j < 3) {
		// First 3 chars must be letters
		np[j] = toupper(np[j]);
		if (!isalpha(np[j++])) return(SYNTAX);
	}

	while (j < 7) {
		// Next 4 chars must be hext digits!
		if (!isxdigit(np[j++])) return(SYNTAX);
	}

	if (np[7] == 0) {
		/*
		 *  String length is OK.  Resize value buffer so it's big enough to
		 *  hold the name, then copy it in.
		 */

		resize((strlen(np)+sizeof(long)-1)/sizeof(long)); 
		_fstrcpy((char _far *)val, np);
		len = strlen(np);
		return(flg = 0);
	}

	return(SYNTAX);
}

static int
GetVid (char *vp)
{	/*
	 *  Process a "vid" resource:
	 *
	 *  Vendor ID's ("vid" resources) are just null-terminated (C-style)
	 *  ASCII strings that must be converted to "length+value" (Pascal-style)
	 *  format and copied into the "val"ue buffer.  The "len" word is then
	 *  made to contain the total length of the buffer in bytes rather than
	 *  words.
	 */

	int n = strlen(vp);
	resize((n + 1 + sizeof(DWORD) - 1)/sizeof(DWORD));

	len = n+1;
	*(char far *)val = n;
	_fmemcpy(((char far *)val)+1, vp, n);

	return(flg = 0);
}

int
CallBef (int op, struct bef_interface _far *bif)
{	/*
	 *  Simulate driver call:
	 *
	 *  This routine simulates the "CallBef" portion of the realmode driver
	 *  interface.  The caller provides the driver "op"eration to be performed,
	 *  and we process accordingly.
	 */

	int x;
	char *cp;

	switch (op) {

		case BEF_LEGACYPROBE:
		{	/*
			 *  Simulate ISA legacy probing:
			 *
			 *  To do this, we check each I/O address for which a device of
			 *  this type might reside.  As long as the framework says the
			 *  address is available, we'll assume that a device resides
			 *  there and will start assigning resources.
			 */

			while (cp = next_device()) {
				/*
				 *  The "cp" register points to the next I/O port where we
				 *  may find a device of the current type.  Use the "GetRes"
				 *  routine to convert the ASCII address specification to
				 *  binary.
				 */

				if (!GetRes(cp, 1) && !flg) {
					/*
					 *  The "val"ue buffer now contains the I/O address(es)
					 *  we want to probe.  Call back into the framework to
					 *  see if this port is available.
					 */

					char *xp;
					(*bif->node)(NODE_START);
					x = (*bif->resource)(RES_SET, "io", val, &len);

					while ((x == RES_OK) && (cp = next_field())) {
						/*
						 *  Reserve all bus resources listed in the current
						 *  device clause.  We bail out upon upon being denied
						 *  a reservation (or when we reach the end of the
						 *  current device clause).
						 *
						 *  The "x" register gives the result of the previous
						 *	reservation request.
						 */

						if (xp = strchr(cp, '=')) {
							/*
							 *  Find the name/value separator in the next
							 *  resource descriptor field.  Then use "GetRes"
							 *  (or "GetNam") to convert the value to binary.
							 */

							int z = SYNTAX;	// Assume conversion will fail
							*xp++ = '\0';	// stomp on the colon!
				
							if (!strcmp(cp, "name"))      z = GetNam(xp);
							else if (!strcmp(cp, "vid"))  z = GetVid(xp);
							else if (!strcmp(cp, "slot")) z = GetRes(xp, 0);
							else if (!strcmp(cp, "mem"))  z = GetRes(xp, 1);
							else if (!strcmp(cp, "dma"))  z = GetRes(xp, 0);
							else if (!strcmp(cp, "irq"))  z = GetRes(xp, 0);

							if (!(x = z)) {
								/*
								 *  No syntax errors in the resource de-
								 *  scription.  Call back to the framework
								 *  to obtain a reservation.
								 */

								flg |= RES_SET;
								x = (*bif->resource)(flg, cp, val, &len);
							}

						} else {
							/*
							 *  No name/value separator.  Treat this as a
							 *  syntax error in the .bef script file.
							 */

							x = SYNTAX;
						}
					}

					// Dispose of the current device node
					(*bif->node)(x ? NODE_FREE : NODE_DONE);

				} else {
					/*
					 *  Syntax error in the I/O port specification for this
					 *  device.  This may be due to a malformed specification
					 *  (detected by GetRes) or by invalid option flags.
					 */

					x = SYNTAX;
				}

				if (x == SYNTAX) {
					/*
					 *  SYNTAX is the special code we use to indicate a syntax
					 *  error in the .bef script file.  Print an error msg
					 *  with appropriate line number.
					 */

					printf("\n*** %s: Syntax error, line %d ***\n", 
						BEFSCRIPT,
					    ln);
				}

				while (next_field());	// Flush to next device clause
			}

			return(0);					// Done with this driver
		}

		case BEF_INSTALLONLY:
		{
			printf("\n*** INSTALLONLY simluation not implemented ***\n");
			break;
		}

		case BEF_PROBEINSTAL:
		{
			printf("\n*** PROBEINSTAL simulation not implemented ***\n");
			break;
		}

		default:
		{
			printf("\n*** Illegal \".bef\" function code (%d) ***\n", op); 
			break;
		}
	}

	return(-1);
}
