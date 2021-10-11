/*
 *	Copyright (c) 1999 by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 * sgsmsg generates several message files from an input template file.  Messages
 * are constructed for use with gettext(3i) - the default - or catgets(3c).  The
 * files generate are:
 *
 * msg.h	a header file containing definitions for each message.  The -h
 *		option triggers the creation of these definitions and specifies
 *		the name to use.
 *
 * msg.c	a data array of message strings.  The msg.h definitions are
 *		offsets into this array.  The -d option triggers the creation of
 *		these definitions and specifies the name to use.
 *
 * messages	a message file suitable for catgets(3c) or gettext(3i) use.  The
 *		-m option triggers this output and specifies the filename to be
 *		used.
 *
 * The template file is processed based on the first character of each line:
 *
 * # or $	entries are copied (as is) to the message file (messages).
 *
 * @ token(s)	entries are translated.  Two translations are possible dependent
 *		on whether one or more tokens are supplied:
 *
 *		A single token is interpreted as one of two reserved message
 *		output indicators, or a message identifier.  The reserved output
 *		indicator _START_ enables output to the message file - Note that
 *		the occurance of any other @ token will also enable message
 *		output.  The reserved output indicator _END_ disables output to
 *		the message file.  The use of these two indicators provides for
 *		only those message strings that require translation to be output
 *		to the message file.
 *
 *		Besides the reserved output indicators, a single token is taken
 *		to be a message identifier which will be subsituted for a
 *		`setid' for catgets(3c) output, or a `domain' name for
 *		gettext(3i) output.  This value is determine by substituting the
 *		token for the associated definition found in the message
 *		identifier file (specified with the -i option).
 *
 *		Multiple tokens are taken to be a message definition followed by
 *		the associated message string.  The message string is copied to
 *		the data array being built in msg.c.  The index into this array
 *		becomes the `message' identifier created in the msg.h file.
 */
#pragma ident	"@(#)sgsmsg.c	1.12	99/06/23 SMI"

#include	<fcntl.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<limits.h>
#include	<string.h>
#include	<ctype.h>
#include	<errno.h>
#include	<sys/param.h>

/*
 * Define any error message strings.
 */
static const char
	* Errmsg_malt =	"sgsmsg: file %s: line %d: malformed input at line\n",
	* Errmsg_nmem =	"sgsmsg: memory allocation failed: %s\n",
	* Errmsg_opne =	"sgsmsg: file %s: open failed: %s\n",
	* Errmsg_wrte =	"sgsmsg: file %s: write failed: %s\n",
	* Errmsg_read =	"sgsmsg: file %s: read failed %s\n",
	* Errmsg_use =	"usage: sgsmsg [-cl] [-d mesgdata] [-h mesgdefs] "
			    "[-m messages] [-n name] [-i mesgident] file ...\n";

/*
 * Define all output filenames and associated descriptors.
 */
static FILE *	fddefs, * fddata, * fdmsgs, * fdmids, * fddesc;
static char *	fldefs, * fldata, * flmsgs, * flmids, * fldesc;
static FILE *	fdlint;
static char	fllint[MAXPATHLEN];

/*
 * Define any default strings.
 */
static const char
	* nmlint =	"/tmp/sgsmsg.lint",
	* interface =	"sgs_msg",
	* start =	"_START_",
	* end =		"_END_";

/*
 * Define any default flags and data items.
 */
static int	cflag = 0, lflag = 0, prtmsgs = 0, line, ptr = 1, msgid = 0;
static char *	mesgid = 0, * setid = 0, * domain = 0;

/*
 * Initialize a setid value.  Given a setid definition determine its numeric
 * value from the specified message identifier file (specified with the -i
 * option).  Return a pointer to the numeric string.
 */
static int
getmesgid(char * id)
{
	char *	buffer, * token, * _mesgid = 0, * _setid = 0, * _domain = 0;

	/*
	 * If we're being asked to interpret a message id but the user didn't
	 * provide the required message identifier file (-i option) we're in
	 * trouble.
	 */
	if (flmids == 0) {
		(void) fprintf(stderr, "sgsmsg: file %s: line %d: mesgid %s: "
		    "unable to process mesgid\n\t"
		    "no message identifier file specified "
		    "(see -i option)\n", fldesc, line, id);
		return (1);
	}

	if ((buffer = (char *)malloc(LINE_MAX)) == 0) {
		(void) fprintf(stderr, Errmsg_nmem, strerror(errno));
		return (1);
	}

	/*
	 * Read the message identifier file and locate the required mesgid.
	 */
	rewind(fdmids);
	while (fgets(buffer, LINE_MAX, fdmids) != NULL) {
		if ((token = strstr(buffer, id)) == NULL)
			continue;

		/*
		 * Establish individual strings for the mesgid, setid and domain
		 * values.
		 */
		_mesgid = token;
		while (!(isspace(*token)))
			token++;
		*token++ = 0;

		while (isspace(*token))
			token++;
		_setid = token;
		while (!(isspace(*token)))
			token++;
		*token++ = 0;

		while (isspace(*token))
			token++;
		_domain = token;
		while (!(isspace(*token)))
			token++;
		*token = 0;
		break;
	}

	/*
	 * Did we find a match?
	 */
	if ((_mesgid == 0) || (_setid == 0) || (_domain == 0)) {
		(void) fprintf(stderr, "sgsmsg: file %s: line %d: mesgid %s: "
		    "unable to process mesgid\n\t"
		    "identifier does not exist in file %s\n",
		    fldesc, line, id, flmids);
		return (1);
	}

	/*
	 * Have we been here before?
	 */
	if (mesgid) {
		if (cflag == 1) {
			/*
			 * If we're being asked to process more than one mesgid
			 * warn the user that only one mesgid can be used for
			 * the catgets(3c) call.
			 */
			(void) fprintf(stderr, "sgsmsg: file %s: line %d: "
			    "setid %s: warning: multiple mesgids "
			    "encountered\n\t"
			    "last setting used in messaging code\n",
			    fldesc, line, id);
		}
	}

	mesgid = _mesgid;
	setid = _setid;
	domain = _domain;

	/*
	 * Generate the message file output (insure output flag is enabled).
	 */
	if (prtmsgs != -1)
		prtmsgs = 1;
	if (fdmsgs && (prtmsgs == 1)) {
		if (cflag == 1) {
			if (fprintf(fdmsgs, "$quote \"\n$set %s\n",
			    setid) < 0) {
				(void) fprintf(stderr, Errmsg_wrte, flmsgs,
				    strerror(errno));
				return (1);
			}
		} else {
			if (fprintf(fdmsgs, "domain\t\"%s\"\n", domain) < 0) {
				(void) fprintf(stderr, Errmsg_wrte, flmsgs,
				    strerror(errno));
				return (1);
			}
		}
	}

	/*
	 * For catgets(3c) output generate a setid definition in the message
	 * definition file.
	 */
	if (fddefs && (cflag == 1) &&
	    (fprintf(fddefs, "#define\t%s\t%s\n\n", mesgid, setid) < 0)) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	return (0);
}

/*
 * Initialize the message definition header file stream.
 */
static int
init_defs(void)
{
	static char	guard[FILENAME_MAX + 6];
	char *		optr;
	const char *	iptr, * _ptr;

	/*
	 * Establish a header guard name using the files basename.
	 */
	for (iptr = 0, _ptr = fldefs; _ptr && (*_ptr != '\0'); _ptr++) {
		if (*_ptr == '/')
			iptr = _ptr + 1;
	}
	if (iptr == 0)
		iptr = fldefs;

	optr = guard;
	for (*optr++ = '_'; iptr && (*iptr != '\0'); iptr++, optr++) {
		if (*iptr == '.') {
			*optr++ = '_';
			*optr++ = 'D';
			*optr++ = 'O';
			*optr++ = 'T';
			*optr = '_';
		} else
			*optr = toupper(*iptr);
	}

	if (fprintf(fddefs, "#ifndef\t%s\n#define\t%s\n\n", guard, guard) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	if (fprintf(fddefs, "#ifndef\t__lint\n\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	/*
	 * add "typedef int	Msg;"
	 */
	if (fprintf(fddefs, "typedef int\tMsg;\n\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	/*
	 * If the associated data array is global define a prototype.
	 * Define a macro to access the array elements.
	 */
	if (lflag == 0) {
		if (fprintf(fddefs, "extern\tconst char\t__%s[];\n\n",
		    interface) < 0) {
			(void) fprintf(stderr, Errmsg_wrte, fldefs,
			    strerror(errno));
			return (1);
		}
	}
	if (fprintf(fddefs, "#define\tMSG_ORIG(x)\t&__%s[x]\n\n",
	    interface) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	/*
	 * Generate a prototype to access the associated data array.
	 */
	if (fprintf(fddefs, "extern\tconst char *\t_%s(Msg);\n\n",
	    interface) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}
	if (fprintf(fddefs, "#define\tMSG_INTL(x)\t_%s(x)\n\n",
	    interface) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	return (0);
}


/*
 * Finish the message definition header file.
 */
static int
fini_defs(void)
{
	if (fprintf(fddefs, "\n#else\t/* __lint */\n\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	/*
	 * When __lint is defined, Msg is a char *.  This allows lint to
	 * check our format strings against it's arguments.
	 */
	if (fprintf(fddefs, "\ntypedef char *\tMsg;\n\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	if (fprintf(fddefs, "extern\tconst char *\t_%s(Msg);\n\n",
	    interface) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	if (lflag == 0) {
		if (fprintf(fddefs, "extern\tconst char\t__%s[];\n\n",
		    interface) < 0) {
			(void) fprintf(stderr, Errmsg_wrte, fldefs,
			    strerror(errno));
			return (1);
		}
	}

	if (fprintf(fddefs,
	    "#define MSG_ORIG(x)\tx\n#define MSG_INTL(x)\tx\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	/*
	 * Copy the temporary lint defs file into the new header.
	 */
	if (fdlint) {
		long	size;
		char *	buf;

		size = ftell(fdlint);
		(void) rewind(fdlint);

		if ((buf = malloc(size)) == 0) {
			(void) fprintf(stderr, Errmsg_nmem, strerror(errno));
			return (1);
		}
		if (fread(buf, size, 1, fdlint) == 0) {
			(void) fprintf(stderr, Errmsg_read, fllint,
			    strerror(errno));
			return (1);
		}
		if (fwrite(buf, size, 1, fddefs) == 0) {
			(void) fprintf(stderr, Errmsg_wrte, fldefs,
			    strerror(errno));
			return (1);
		}
		(void) free(buf);
	}

	if (fprintf(fddefs, "\n#endif\t/* __lint */\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	if (fprintf(fddefs, "\n#endif\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldefs, strerror(errno));
		return (1);
	}

	return (0);
}


/*
 * Initialize the data structure definition.
 */
static int
init_data(void)
{
	const char *	str;

	/*
	 * Determine from the local flag whether the data declaration should
	 * be static.
	 */
	if (lflag)
		str = (const char *)"static const";
	else
		str = (const char *)"const";

	if (fprintf(fddata, "%s char __%s[] = { 0x00,", str, interface) < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldata, strerror(errno));
		return (1);
	}

	return (0);
}


/*
 * Finish off the data structure definition.
 */
static int
fini_data(void)
{
	if (fprintf(fddata, "\n\t0x00\n};\n") < 0) {
		(void) fprintf(stderr, Errmsg_wrte, fldata, strerror(errno));
		return (1);
	}

	return (0);
}

static int
file()
{
	char	buffer[LINE_MAX], * token;
	int	column = 1, escape = 0;

	line = 1;

	while ((token = fgets(buffer, LINE_MAX, fddesc)) != NULL) {
		char	defn[PATH_MAX], * _defn, * str;
		int	len;

		switch (*token) {
		case '#':
		case '$':
			if (escape) {
				(void) fprintf(stderr, Errmsg_malt, fldesc,
				    line);
				return (1);
			}

			/*
			 * If a msgid has been output a msgstr must follow
			 * before we digest the new token.  A msgid is only set
			 * if fdmsgs is in use.
			 */
			if (msgid) {
				msgid = 0;
				if (fprintf(fdmsgs, "msgstr\t\"\"\n") < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    flmsgs, strerror(errno));
					return (1);
				}
			}

			/*
			 * Pass lines directly through to the output message
			 * file.
			 */
			if (fdmsgs && (prtmsgs == 1)) {
				char	comment;

				if (cflag == 0)
					comment = '#';
				else
					comment = '$';

				if (fprintf(fdmsgs, "%c%s", comment,
				    ++token) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    flmsgs, strerror(errno));
					return (1);
				}
			}
			break;

		case '@':
			if (escape) {
				(void) fprintf(stderr, Errmsg_malt, fldesc,
				    line);
				return (1);
			}

			/*
			 * If a msgid has been output a msgstr must follow
			 * before we digest the new token.
			 */
			if (msgid) {
				msgid = 0;
				if (fprintf(fdmsgs, "msgstr\t\"\"\n") < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    flmsgs, strerror(errno));
					return (1);
				}
			}

			/*
			 * Determine whether we have one or more tokens.
			 */
			token++;
			while (isspace(*token))		/* rid any whitespace */
				token++;
			_defn = token;			/* definition start */
			while (!(isspace(*token)))
				token++;
			*token++ = 0;

			while (isspace(*token))		/* rid any whitespace */
				token++;

			/*
			 * Determine whether the single token is one of the
			 * reserved message output delimiters otherwise
			 * translate it as a message identifier.
			 */
			if (*token == 0) {
				if (strcmp(_defn, start) == 0)
					prtmsgs = 1;
				else if (strcmp(_defn, end) == 0)
					prtmsgs = -1;
				else if (getmesgid(_defn) == 1)
					return (1);
				break;
			}

			/*
			 * Multiple tokens are translated by taking the first
			 * token as the message definition, and the rest of the
			 * line as the message itself.  A message line ending
			 * with an escape ('\') is expected to be continued on
			 * the next line.
			 */
			if (prtmsgs != -1)
				prtmsgs = 1;
			if (fdmsgs && (prtmsgs == 1)) {
				/*
				 * For catgets(3c) make sure a message
				 * identifier has been established (this is
				 * normally a domain for gettext(3i), but for
				 * sgsmsg use this could be argued as being
				 * redundent).  Also make sure that the message
				 * definitions haven't exceeeded the maximum
				 * value allowed by gencat(1) before generating
				 * any message file entries.
				 */
				if (cflag == 1) {
					if (setid == 0) {
						(void) fprintf(stderr, "file "
						    "%s: no message identifier "
						    "has been established\n",
						    fldesc);
						return (1);
					}
					if (ptr	> NL_MSGMAX) {
						(void) fprintf(stderr, "file "
						    "%s: message definition "
						    "(%d) exceeds allowable "
						    "limit (NL_MSGMAX)\n",
						    fldesc, ptr);
						return (1);
					}
				}

				/*
				 * For catgets(3c) write the definition and the
				 * message string to the message file.  For
				 * gettext(3i) write the message string as a
				 * mesgid - indicate a mesgid has been output
				 * so that a msgstr can follow.
				 */
				if (cflag == 1) {
					if (fprintf(fdmsgs, "%d\t%s", ptr,
					    token) < 0) {
						(void) fprintf(stderr,
						    Errmsg_wrte, flmsgs,
						    strerror(errno));
						return (1);
					}
				} else {
					if (fprintf(fdmsgs, "msgid\t\"") < 0) {
						(void) fprintf(stderr,
						    Errmsg_wrte, flmsgs,
						    strerror(errno));
						return (1);
					}
					msgid = 1;
				}
			}

			/*
			 * The message itself is a quoted string as this makes
			 * embedding spaces at the start (or the end) of the
			 * string very easy.
			 */
			if (*token != '"') {
				(void) fprintf(stderr, Errmsg_malt, fldesc,
				    line);
				return (1);
			}

			/*
			 * Write the definition to the header file using the
			 * present pointer value.  Save the definition name for
			 * later use in the definition size.
			 */
			if (fddata == fddefs) {
				column = 1;
				str = "\n\n";
			} else
				str = "";

			(void) strcpy(defn, _defn);
			if (fddefs) {
				if (fprintf(fddefs, "%s#define\t%s\t%d\n", str,
				    _defn, ptr) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fldefs, strerror(errno));
					return (1);
				}
			}

			/*
			 * Also write the tag to the lint definitions.
			 */
			if (fdlint) {
				if (fprintf(fdlint, "%s#define\t%s\t", str,
				    _defn) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fllint, strerror(errno));
					return (1);
				}
			}

			len = 0;

			/*
			 * Write each character of the message string to the
			 * data array.  Translate any escaped characters - use
			 * the same specially recognized characters as defined
			 * by gencat(1).
			 */
message:
			if (*token == '"') {
				if (fdlint &&
				    (fprintf(fdlint, "%c", *token) < 0)) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fllint, strerror(errno));
					return (1);
				}
				token++;
			}
			while (*token) {
				char	_token;

				if ((*token == '\\') && (escape == 0)) {
					escape = 1;
					if (fdlint && (*(token + 1) != '\n') &&
					    fprintf(fdlint, "%c", *token) < 0) {
						(void) fprintf(stderr,
						    Errmsg_wrte, fllint,
						    strerror(errno));
						return (1);
					}
					token++;
					continue;
				}
				if (escape) {
					if (*token == 'n')
						_token = '\n';
					else if (*token == 't')
						_token = '\t';
					else if (*token == 'v')
						_token = '\v';
					else if (*token == 'b')
						_token = '\b';
					else if (*token == 'f')
						_token = '\f';
					else if (*token == '\\')
						_token = '\\';
					else if (*token == '"')
						_token = '"';
					else if (*token == '\n')
						break;
					else
						_token = *token;

					if (fdmsgs && (prtmsgs == 1) &&
					    (fprintf(fdmsgs, "\\") < 0)) {
						(void) fprintf(stderr,
						    Errmsg_wrte, flmsgs,
						    strerror(errno));
						return (1);
					}
				} else {
					/*
					 * If this is the trailing quote then
					 * thats the last of the message string.
					 * Eat up any remaining white space and
					 * unless an escape character is found
					 * terminate the data string with a 0.
					 */
					if (*token == '"') {
					    if (fdlint && (fprintf(fdlint,
						"%c", *token) < 0)) {
						(void) fprintf(stderr,
						    Errmsg_wrte, fllint,
						    strerror(errno));
						return (1);
					    }

					    if (fdmsgs && (prtmsgs == 1) &&
						(fprintf(fdmsgs, "%c",
						*token) < 0)) {
						(void) fprintf(stderr,
						    Errmsg_wrte, flmsgs,
						    strerror(errno));
						return (1);
					    }

					    while (*++token) {
						if (*token == '\n')
							break;
					    }
					    _token = '\0';
					} else
					    _token = *token;
				}

				if (fdmsgs && (prtmsgs == 1) &&
				    (fprintf(fdmsgs, "%c", *token) < 0)) {
					(void) fprintf(stderr, Errmsg_wrte,
					    flmsgs, strerror(errno));
					return (1);
				}

				if (fdlint && fprintf(fdlint,
				    "%c", *token) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fllint, strerror(errno));
					return (1);
				}

				if (column == 1)
					str = "\n\t0x%.2x,";
				else
					str = "  0x%.2x,";

				if (fddata && fprintf(fddata, str,
				    _token) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fldata, strerror(errno));
					return (1);
				}
				ptr++, token++, len++;
				escape = 0;

				if (column++ == 10)
					column = 1;
				if (_token == '\0')
					break;
			}

			/*
			 * After the complete message string has been processed
			 * (including its continuation beyond one line), create
			 * a string size definition.
			 */
			if (escape == 0) {
				const char * form = "%s#define\t%s_SIZE\t%d\n";

				if (fddata == fddefs) {
					column = 1;
					str = "\n\n";
				} else
					str = "";

				if (fddefs && fprintf(fddefs, form, str, defn,
				    (len - 1)) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fldefs, strerror(errno));
					return (1);
				}
				if (fdlint && fprintf(fdlint, form, str, defn,
				    (len - 1)) < 0) {
					(void) fprintf(stderr, Errmsg_wrte,
					    fllint, strerror(errno));
					return (1);
				}
			}
			break;

		default:
			/*
			 * Empty lines are passed through to the message file.
			 */
			while (isspace(*token))
				token++;

			if (*token == 0) {
				if (msgid || (fdmsgs && (prtmsgs == 1))) {
					/*
					 * If a msgid has been output a msgstr
					 * must follow before we digest the new
					 * token.
					 */
					if (msgid) {
						msgid = 0;
						str = "msgstr\t\"\"\n\n";
					} else
						str = "\n";

					if (fprintf(fdmsgs, str) < 0) {
						(void) fprintf(stderr,
						    Errmsg_wrte, flmsgs,
						    strerror(errno));
						return (1);
					}
				}
				break;
			}

			/*
			 * If an escape is in effect then any tokens are taken
			 * to be message continuations.
			 */
			if (escape) {
				escape = 0;
				goto message;
			}

			(void) fprintf(stderr, "file %s: line %d: invalid "
			    "input does not start with #, $ or @\n", fldesc,
			    line);
			return (1);
		}
		line++;
	}

	return (0);
}

main(int argc, char ** argv)
{
	opterr = 0;
	while ((line = getopt(argc, argv, "cd:h:lm:n:i:")) != EOF) {
		switch (line) {
		case 'c':			/* catgets instead of gettext */
			cflag = 1;
			break;
		case 'd':			/* new message data filename */
			fldata = optarg;	/*	(msg.c is default) */
			break;
		case 'h':			/* new message defs filename */
			fldefs = optarg;	/*	(msg.h is default) */
			break;
		case 'i':			/* input message ids from */
			flmids = optarg;	/*	from this file */
			break;
		case 'l':			/* define message data arrays */
			lflag = 1;		/*	to be local (static) */
			break;
		case 'm':			/* generate message database */
			flmsgs = optarg;	/*	to this file */
			break;
		case 'n':			/* new data array and func */
			interface = optarg;	/*	name (msg is default) */
			break;
		case '?':
			(void) fprintf(stderr, Errmsg_use, argv[0]);
			exit(1);
		default:
			break;
		}
	}

	/*
	 * Validate the we have been given at least one input file.
	 */
	if ((argc - optind) < 1) {
		(void) fprintf(stderr, Errmsg_use);
		exit(1);
	}

	/*
	 * Open all the required output files.
	 */
	if (fldefs) {
		if ((fddefs = fopen(fldefs, "w+")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, fldefs,
			    strerror(errno));
			return (1);
		}
	}
	if (fldata) {
		if (fldefs && (strcmp(fldefs, fldata) == 0))
			fddata = fddefs;
		else if ((fddata = fopen(fldata, "w+")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, fldata,
			    strerror(errno));
			return (1);
		}
	}
	if (fddefs && fddata) {
		(void) sprintf(fllint, "%s.%d", nmlint, (int)getpid());
		if ((fdlint = fopen(fllint, "w+")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, fllint,
			    strerror(errno));
			return (1);
		}
	}
	if (flmsgs) {
		if ((fdmsgs = fopen(flmsgs, "w+")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, flmsgs,
			    strerror(errno));
			return (1);
		}
	}
	if (flmids) {
		if ((fdmids = fopen(flmids, "r")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, flmids,
			    strerror(errno));
			return (1);
		}
	}

	/*
	 * Initialize the message definition and message data streams.
	 */
	if (fddefs) {
		if (init_defs())
			return (1);
	}
	if (fddata) {
		if (init_data())
			return (1);
	}

	/*
	 * Read the input message file, and for each line process accordingly.
	 */
	for (; optind < argc; optind++) {
		int	err;

		fldesc = argv[optind];

		if ((fddesc = fopen(fldesc, "r")) == NULL) {
			(void) fprintf(stderr, Errmsg_opne, fldesc,
			    strerror(errno));
			return (1);
		}
		err = file();
		(void) fclose(fddesc);

		if (err != 0)
			return (1);
	}

	/*
	 * If a msgid has been output a msgstr must follow before we end the
	 * file.
	 */
	if (msgid) {
		msgid = 0;
		if (fprintf(fdmsgs, "msgstr\t\"\"\n") < 0) {
			(void) fprintf(stderr, Errmsg_wrte, flmsgs,
			    strerror(errno));
			return (1);
		}
	}

	if (fdmids)
		(void) fclose(fdmids);
	if (fdmsgs)
		(void) fclose(fdmsgs);

	/*
	 * Finish off any generated data and header file.
	 */
	if (fldata) {
		if (fini_data())
			return (1);
	}
	if (fddefs) {
		if (fini_defs())
			return (1);
	}

	/*
	 * Close up everything and go home.
	 */
	if (fddata)
		(void) fclose(fddata);
	if (fddefs && (fddefs != fddata))
		(void) fclose(fddefs);
	if (fddefs && fddata) {
		(void) fclose(fdlint);
		(void) unlink(fllint);
	}

	return (0);
}
