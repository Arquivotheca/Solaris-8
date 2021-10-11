static char	sccsid[] =
	"@(#)lwlp.c 1.1	99/05/17 SMI";

#pragma ident	"@(#)lwlp.c	1.1	99/05/17 SMI"
/*
 * lwlp - Convert ASCII text to PostScript
 *
 * Usage:
 *	lwlp [-{2|4|8}] [-p] [-L] [-r] [-n#] [-l#|-w#] [-c#] [-t#]
 *		[-hstring] [-Bstring] [-Istring] [-Xstring] [-Pfile] [file ...]
 *
 * Options:
 *	-{1|2|4|8}	print multiple logical pages per page
 *	-d		debug, don't remove temporary file
 *	-L		specify Landscape instead of Portrait
 *	-p		filter input through pr
 *	-r		toggle page reversal flag (default is off)
 *	-e		elide unchanged functions
 *	-n#		number with numberwidth digits
 *	-l#		specify number of lines/logical page, default 66
 *	-w#		specify number of columns
 *	-c#		specify number of copies
 *	-t#		specify tab spacing
 *	-htext		specify header text
 *	-Btext		specify bold font selector
 *	-Itext		specify italic font selector
 *	-Xtext		specify bold-italic font selector
 *	-Gtext		specify graying selector
 *	-Pfile		specify different Postscript prologue file
 *
 * If no files are specified, stdin is used.
 * Form feeds handled
 * Backspacing with underlining (or overprinting) works
 * The output conforms to Adobe 2.0
 *
 * Problems:
 *	- assumes fixed-width (non-proportional) font in some places
 *	- can't back up (using backspaces) over tabs
 *	- assumes 8.5 x 11.0 paper
 *	- uses logical page with aspect ratio of 3 * 4
 *
 */

#define USAGE1	"[-{1|2|4|8}] [-p] [-L] [-r] [-n<numberwidth]"
#define USAGE2	"[-l<lines>|-w<columns>] [-c<count>] [-t<tabs>]"
#define USAGE3	"[-hstring] [-Bstring] [-Istring] [-Xstring] [-Gstring]"
#define	USAGE4	"[-Pfile] [file ...]"
#define	USAGE6	"[-hstring] [-e] oldfile newfile"

#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <ctype.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * Configurable...
 * BUFOUT should be fairly large
 */
#define BUFIN			1024	/* maximum length of an input line */
#define BUFOUT			(BUFIN * 5)
#define	MAXPAGES		10000
#define	REVERSE_OFF		0

#define DEFAULT_PAPER_HEIGHT	11.0
#define DEFAULT_PAPER_WIDTH	8.50
#define DEFAULT_PAGE_HEIGHT	10.0
#define DEFAULT_PAGE_WIDTH	7.50
#define DEFAULT_LINES_PER_PAGE	66
#define DEFAULT_TAB_SIZE	8
char	*default_font = "Courier";
char	*default_font_bold = "Courier-Bold";
char	*default_font_italic = "Courier-Oblique";
char	*default_font_bold_italic = "Courier-BoldOblique";
char	*select_default_font = "FRN";
char	*select_default_font_bold = "FRB";
char	*select_default_font_italic = "FIN";
char	*select_default_font_bold_italic = "FIB";
#define DEFAULT_FONT			select_default_font
#define DEFAULT_FONT_BOLD		select_default_font_bold
#define DEFAULT_FONT_ITALIC		select_default_font_italic
#define DEFAULT_FONT_BOLD_ITALIC	select_default_font_bold_italic
#define DEFAULT_CHAR_WIDTH	(.6)
#define DEFAULT_SPACES_AFTER_NUMBER	1
#define	DEFAULT_DESCENDER_FRACTION	0.3
#define	LWLP			"lwlp"
#define	CODEREVIEW		"codereview"
#define	END_C_FUNCTION		'}'
#define	END_ASM_FUNCTION	"SET_SIZE("
char	*banner = "**********************************************************";

/*
 * PostScript command strings
 */
#define LINETO			"lineto"
#define NEWPATH			"newpath"
#define SETLINEWIDTH		"setlinewidth"
#define	STROKE			"stroke"
/*
 * PostScript command strings defined in the prologue file
 */
#define BACKSPACE		"B"
#define MOVETO			"M"	/* x y */
#define SHOW			"S"	/* string */
#define TAB			"T"	/* spaces */
#define ZEROMOVETO		"Z"	/* y */
#define SELECT_FONT		"SFT"	/* size font */
#define SET_WIDTHS		"SWT"
#define START_PAGE		"SPG"	/* angle scale x y */
#define END_PAGE		"EPG"
#define FLUSH_PAGE		"FPG"	/* ncopies */
#define	SHADE			"SHD"	/* x0 y0 x1 y1 */

/*
 * Conformance requires that no PostScript line exceed 256 characters
 */
#define	POINTS_PER_INCH		72
#define MAX_OUTPUT_LINE_LENGTH	256

#define START_X			0	/* position of start of each line */
#define THREE_HOLE_X		1.0	/* portrait x offset (inches) 3 hole */
#define THREE_HOLE_Y		0.5	/* landscape y offset (inches) 3 hole */
#define	RULE_WIDTH		0.25	/* width in units of paging rules */

struct print_state {
	int	page_count;
	int	logical_page_count;
	int	lineno;
	long	offset;
	float	row;
	char	*font;
}	current, saved;

struct format_state {
	int	numberwidth, linenumber, altlinenumber;
	int	makegray;
	char	*font;
};

int	change_seen, dots_inserted, in_change, old_stuff, makegray;
int	lines_per_page;
int	columns;
float	point_size;
int	start_x, start_y, end_x;
int	landscape, rot_text;

int	ncopies;
int	tabstop;
int	reverse;
int	elide;
int	usetmp;
int	dflag, lflag, mflag, pflag, vflag, wflag;
int	numberwidth, linenumber, altlinenumber;
int	boldlength, itlclength, bitclength, graylength;
char	*boldstring, *itlcstring, *bitcstring, *graystring;
int	header;
#define	HEADER_EXPLICIT	1
#define	HEADER_IMPLICIT	2
char	*headerstring;
char	*bannerfile;

char	bufin[BUFIN];			/* input buffer */
char	bufout[BUFOUT];			/* output buffer */
long	*page_map;			/* offset of first byte of each page */

char	*username, *hostname, *release, *currentdate;

char	*fgetline();
char	*mktemp();
void	*malloc();
long	time();
char	*ctime(), *getlogin();
struct passwd	*getpwuid();

int	getopt(/* int argc, char *argv[], char* optstring */);
extern	char	*optarg;
extern	int	optind, opterr;

char	*prologue;
char	*progname;
int	iscodereview;

char	*default_prologue[] = {
#include "prologue.h"
	NULL
};

struct layout {
	float	scale;
	int	pages, page_rows, page_cols;
	int	rotation;
};
struct layout	*layoutp;
struct layout	layout1 = { 1.000000, 1, 1, 1, 0 };
struct layout	layout2 = { 0.666666, 2, 2, 1, 90 };
struct layout	layout4 = { 0.500000, 4, 2, 2, 0 };
struct layout	layout8 = { 0.333333, 8, 4, 2, 90 };

int	box_width, box_height;
int	gap_width, gap_height;
int	margin_x, margin_y;

struct position {
	int	base_x;
	int	base_y;
}	positions[8];

main(argc, argv)
	int		argc;
	char		**argv;
{
	register int	ch, i, j, first_file;
	char		*pc;
	double		atof();
	FILE		*infile;

	if ((pc = strrchr(argv[0], '/')) != NULL)
		progname = pc + 1;
	else
		progname = argv[0];

	lines_per_page = DEFAULT_LINES_PER_PAGE;
	layoutp = &layout1;
	tabstop = DEFAULT_TAB_SIZE;
	current.page_count = 0;
	ncopies = 1;
	reverse = REVERSE_OFF;

	if (iscodereview = strncmp(progname, CODEREVIEW,
			sizeof(CODEREVIEW)-1) == 0) {
		layoutp = &layout2;
		numberwidth = 4;
		columns = 85;		/* extra space for numbering */
		wflag = -1;
	}

	while ((ch = getopt(argc, argv,
			"1248B:c:deG:h:I:l:Ln:P:prt:vw:X:")) != -1) {
		switch (ch) {
		case '1':
			layoutp = &layout1;
			break;
		case '2':
			layoutp = &layout2;
			break;
		case '4':
			layoutp = &layout4;
			break;
		case '8':
			layoutp = &layout8;
			break;
		case 'B':
			boldlength = strlen(optarg);
			boldstring = (char*) malloc((unsigned) boldlength + 1);
			(void) strcpy(boldstring, optarg);
			break;
		case 'c':
			ncopies = atof(optarg);
			if (ncopies <= 0) {
				fatal("number of copies must be > 0");
				/*NOTREACHED*/
			}
			break;
		case 'd':
			dflag = 1;
			break;
		case 'e':
			elide = 1;
			break;
		case 'G':
			graylength = strlen(optarg);
			graystring = (char*) malloc((unsigned) graylength + 1);
			(void) strcpy(graystring, optarg);
			break;
		case 'h':
			header = HEADER_EXPLICIT;
			i = strlen(optarg);
			headerstring = (char*) malloc((unsigned) i + 1);
			(void) strcpy(headerstring, optarg);
			if (strcmp(headerstring, "-") == 0)
				header = HEADER_IMPLICIT;
			break;
		case 'I':
			itlclength = strlen(optarg);
			itlcstring = (char*) malloc((unsigned) itlclength + 1);
			(void) strcpy(itlcstring, optarg);
			break;
		case 'l':
			lines_per_page = atoi(optarg);
			if (lines_per_page < 1) {
				fatal("invalid number of lines/page");
				/*NOTREACHED*/
			}
			lflag = 1;
			if (wflag > 0) {
				fatal("can't have both -l and -w");
				/*NOTREACHED*/
			}
			wflag = 0;
			break;
		case 'L':
			landscape = 1;
			break;
		case 'm':
			mflag = 1;
			break;
		case 'n':
			numberwidth = atoi(optarg);
			if (numberwidth < 2) {
				fatal("invalid numbering width");
				/*NOTREACHED*/
			}
			break;
		case 'P':
			prologue = optarg;
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			reverse = !reverse;
			break;
		case 't':
			tabstop = atoi(optarg);
			if (tabstop < 1) {
				fatal("negative tabstop");
				/*NOTREACHED*/
			}
			break;
		case 'v':
			vflag = 1;
			break;
		case 'w':
			columns = atoi(optarg);
			if (columns < 1) {
				fatal("invalid number of columns");
				/*NOTREACHED*/
			}
			wflag = 1;
			if (lflag) {
				fatal("can't have both -l and -w");
				/*NOTREACHED*/
			}
			break;
		case 'X':
			bitclength = strlen(optarg);
			bitcstring = (char*) malloc((unsigned) bitclength + 1);
			(void) strcpy(bitcstring, optarg);
			break;
		default:
			(void) fprintf(stderr, 
					"usage: %s %s\n\t%s\n\t%s\n\t%s\n",
					iscodereview ? LWLP : progname,
					USAGE1, USAGE2, USAGE3, USAGE4);
			if (iscodereview)
				(void) fprintf(stderr, "\t%s [%s flags] %s\n",
						CODEREVIEW, LWLP, USAGE6);
			exit(1);
		}
	}

	if (elide && !iscodereview) {
		fatal("-e option valid only with codereview");
		/*NOTREACHED*/
	}
	usetmp = reverse || elide;
	/* allocate page_map if we need one */
	if (reverse) {
		page_map = (long*) malloc(MAXPAGES * sizeof(long*));
		if (page_map == NULL) {
			fatal("unable to allocate memory for page reversal");
			/*NOTREACHED*/
		}
	}

	/*
	 * Check that all files are readable
	 * This is so that no output at all is produced if any file is not
	 * readable in case the output is being piped to a printer
	 */
	first_file = optind;
	for (j = first_file; j < argc; j++) {
		if (access(argv[j], R_OK) == -1 && !(iscodereview &&
		    strcmp(argv[j], "-") == 0)) {
			fatal("cannot access %s", argv[j]);
			/*NOTREACHED*/
		}
	}
	if (iscodereview && (first_file + 2) != argc) {
		fatal("codereview: need old and new file");
		/*NOTREACHED*/
	}

	/* compute logical point size, logical dimensions */
	if (!landscape) {
		rot_text = layoutp->rotation;
		start_y = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH;
		start_x = START_X;
		end_x = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH;
		if (wflag) {
			point_size = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH /
					((columns + 0.5) * DEFAULT_CHAR_WIDTH);
			lines_per_page = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH /
					point_size;
		}
		else {
			point_size = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH /
					(lines_per_page + 0.5);
			columns = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH /
					(point_size * DEFAULT_CHAR_WIDTH);
		}
		if (mflag) {
			;
		}
	}
	else {
		rot_text = 90 - layoutp->rotation;
		start_y = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH;
		start_x = START_X;
		end_x = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH;
		if (wflag) {
			point_size = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH /
					((columns + 0.5) * DEFAULT_CHAR_WIDTH);
			lines_per_page = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH /
					point_size;
		}
		else {
			point_size = DEFAULT_PAGE_WIDTH * POINTS_PER_INCH /
					(lines_per_page + 0.5);
			columns = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH /
					(point_size * DEFAULT_CHAR_WIDTH);
		}
		if (mflag) {
			;
		}
	}

	box_height = DEFAULT_PAGE_HEIGHT * POINTS_PER_INCH / layoutp->page_rows;
	if (layoutp->rotation == 0)
		box_width = box_height /
				DEFAULT_PAGE_HEIGHT * DEFAULT_PAGE_WIDTH;
	else
		box_width = box_height *
				DEFAULT_PAGE_HEIGHT / DEFAULT_PAGE_WIDTH;
	gap_width = DEFAULT_PAPER_WIDTH * POINTS_PER_INCH /
			layoutp->page_cols - box_width;
	gap_height = DEFAULT_PAPER_HEIGHT * POINTS_PER_INCH /
			layoutp->page_rows - box_height;
	margin_x = gap_width/2;
	margin_y = gap_height/2;

	columns -= numberwidth + DEFAULT_SPACES_AFTER_NUMBER;
	if (columns <= 0) {
		fatal("numbering width exceeds number of columns");
		/* NOT REACHED */
	}
	/* compute physical "lower left corner" of each logical page */
	for (j = 0; j < layoutp->pages; j++) {
		int	phys_row;		/* 0 is bottom row */
		int	phys_col;		/* 0 is left column */

		if (landscape == (rot_text == 0)) {
			/* logical pages run physically up and down */
			phys_row = j % layoutp->page_rows;
			phys_col = j / layoutp->page_rows;
		}
		else {
			/* logical pages run physically left to right */
			phys_row = j / layoutp->page_cols;
			phys_col = j % layoutp->page_cols;
		}
		if (rot_text == 0) {
			/* top physical row is logically first */
			phys_row = layoutp->page_rows - 1 - phys_row;
		}

		positions[j].base_x = margin_x +
				phys_col * (box_width + gap_width);
		positions[j].base_y = margin_y +
				phys_row * (box_height + gap_height);
		if (rot_text != 0) {
			positions[j].base_x += box_width;
		}
	}

	if (vflag) {
		(void) fprintf(stderr, "%s: %s\n\n", progname, sccsid);
		(void) fprintf(stderr, "Lines/page = %d\n", lines_per_page);
		(void) fprintf(stderr, "Columns = %d\n", columns);
		for (j = 0; j < layoutp->pages; j++) {
			(void) fprintf(stderr, "\tx=%3d, y=%3d\n",
					positions[j].base_x,
					positions[j].base_y);
		}
		(void) fprintf(stderr, "box_width=%3d, box_height=%3d\n",
				box_width, box_height);
		(void) fprintf(stderr, "gap_width=%3d, gap_height=%3d\n",
				gap_width, gap_height);
	}

	setup();
	preamble();

	if (iscodereview) {
		char	command[BUFSIZ];

		(void) sprintf(command, "diff -w -D%s%s %s %s",
			*release == '4' ? "" : " ",
			CODEREVIEW, argv[first_file+1], argv[first_file]);
		infile = popen(command, "r");
		bannerfile = argv[first_file+1];
		if (ungetc(getc(infile), infile) == EOF) {
			(void) pclose(infile);
			(void) sprintf(command,
					"echo No differences encountered");
			infile = popen(command, "r");
		}
		setheaderfile(bannerfile);
		printfile(infile);
		(void) pclose(infile);
	}
	else if (first_file == argc) {	/* no files on command line */
		if (vflag)
			(void) fprintf(stderr, "\tprinting stdin\n");
		setheaderfile("stdin");
		printfile(stdin);
	}
	else {
		for (i = first_file; i < argc; i++) {
			if ((infile = fopen(argv[i], "r")) == (FILE *) NULL) {
				fatal("can't open %s for reading", argv[i]);
				/*NOTREACHED*/
			}
			if (pflag) {
				char	cmdbuf[BUFSIZ];
				(void) sprintf(cmdbuf, "pr %s", argv[i]);
				(void) fclose(infile);
				infile = popen(cmdbuf, "r");
			}
			if (vflag)
				(void) fprintf(stderr, "\tprinting %s\n",
						argv[i]);
			setheaderfile(argv[i]);
			printfile(infile);
			if (pflag)
				(void) pclose(infile);
			else
				(void) fclose(infile);
		}
	}

	postamble();

	if (fflush(stdout) == EOF) {
		fatal("write error on stdout");
		/*NOTREACHED*/
	}
	exit(0);
	/*NOTREACHED*/
}

/*
 * Initial lines sent to the LaserWriter
 * Generates the PostScript header and includes the prologue file
 * There is limited checking for I/O errors here
 */
preamble()
{
	(void) printf("%%!PS-Adobe-2.0\n");
	(void) printf("%%%%Creator: %s on %s\n", progname, hostname);
	(void) printf("%%%%CreationDate: %s\n", currentdate);
	(void) printf("%%%%For: %s\n", username);
	(void) printf("%%%%DocumentFonts: %s %s %s %s\n",
		default_font, default_font_bold,
		default_font_italic, default_font_bold_italic);
	(void) printf("%%%%Pages: (atend)\n");

	if (prologue == NULL) {
		register char	**cpp;
		for (cpp = default_prologue; *cpp; cpp++) {
			(void) fputs(*cpp, stdout);
		}
	}
	else {
		register FILE	*fp;
		if ((fp = fopen(prologue, "r")) == NULL) {
			fatal("can't open prologue file %s", prologue);
			/*NOTREACHED*/
		}
		while (fgets(bufin, sizeof(bufin), fp) != NULL)
			(void) fputs(bufin, stdout);
		(void) fclose(fp);
	}
	if (ferror(stdout) || fflush(stdout) == EOF) {
		fatal("write error on stdout");
		/*NOTREACHED*/
	}

	(void) printf("/%s {%f /%s %s}bind def\n", DEFAULT_FONT,
			point_size, default_font, SELECT_FONT);
	(void) printf("/%s {%f /%s %s}bind def\n", DEFAULT_FONT_BOLD,
			point_size, default_font_bold, SELECT_FONT);
	(void) printf("/%s {%f /%s %s}bind def\n", DEFAULT_FONT_ITALIC,
			point_size, default_font_italic, SELECT_FONT);
	(void) printf("/%s {%f /%s %s}bind def\n", DEFAULT_FONT_BOLD_ITALIC,
			point_size, default_font_bold_italic, SELECT_FONT);
}

postamble()
{
	(void) printf("%%%%Trailer\n");
	(void) printf("%%%%Pages: %d\n", current.page_count);
}

int
printbanner(filename, outfile)
	char		*filename;
	FILE		*outfile;
{
	char		buffer[BUFSIZ];
	struct stat	statbuf;
	struct format_state	format_state;

	/* we've already verified readability */
	(void) stat(filename, &statbuf);

	save_format_state(&format_state);
	numberwidth = 0;

	setcurrentfont(DEFAULT_FONT_BOLD_ITALIC, outfile);

	current.row -= point_size;
	(void) fprintf(outfile, "%d %.2f %s\n", start_x, current.row, MOVETO);
	proc(banner, outfile);
	(void) sprintf(buffer, "%8d %.24s %s", statbuf.st_size,
			ctime(&statbuf.st_mtime), filename);
	current.row -= point_size;
	(void) fprintf(outfile, "%d %.2f %s\n", start_x, current.row, MOVETO);
	proc(buffer, outfile);
	current.row -= point_size;
	(void) fprintf(outfile, "%d %.2f %s\n", start_x, current.row, MOVETO);
	proc(banner, outfile);

	restore_format_state(&format_state, outfile);
	savestate(outfile);
	return (3);
}

setcurrentfont(newfont, outfile)
	char	*newfont;
	FILE	*outfile;
{
	if (current.font != newfont) {
		if (newfont)
			current.font = newfont;
		(void) fprintf(outfile, "%s\n", current.font);
	}
}

savestate(f)
	FILE	*f;
{
	current.offset = ftell(f);
	saved = current;
}

restorestate(f)
	FILE	*f;
{
	char	*font;

	font = current.font;
	(void) fseek(f, saved.offset, 0);
	current = saved;
	setcurrentfont(font, f);
}

save_format_state(fs)
	struct format_state	*fs;
{
	fs->numberwidth = numberwidth;
	fs->linenumber = linenumber;
	fs->altlinenumber = altlinenumber;
	fs->makegray = makegray;
	fs->font = current.font;
}

restore_format_state(fs, outfile)
	struct format_state	*fs;
	FILE			*outfile;
{
	numberwidth = fs->numberwidth;
	linenumber = fs->linenumber;
	altlinenumber = fs->altlinenumber;
	makegray = fs->makegray;
	setcurrentfont(fs->font, outfile);
}

/*
 * Print a file
 *
 * The input stream may be stdin, a file, or a pipe
 */
printfile(infile)
	FILE		*infile;
{
	register int	eof;
	register char	*p;
	FILE		*outfile;

	if (reverse)
		page_map[0] = 0L;
	if (usetmp) {
		(void) sprintf(bufin, "/tmp/%sXXXXXX", progname);
		p = mktemp(bufin);
		if ((outfile = fopen(p, "w+")) == NULL) {
			fatal("can't open temporary file %s", p);
			/* NOTREACHED */
		}
		if (!dflag)
			(void) unlink(p);
		else
			(void) fprintf(stderr, "will not unlink %s\n", p);
	}
	else
		outfile = stdout;

	setcurrentfont(DEFAULT_FONT, outfile);
	change_seen = 0;
	dots_inserted = 0;
	in_change = 0;
	makegray = 0;
	linenumber = 0;
	altlinenumber = 0;
	current.logical_page_count = 0;
	do {
		current.row = start_y;
		eof = printpage(infile, outfile);
	} while (!eof);

	if (((int)current.row) != start_y)
		endpage(outfile);
	if ((current.logical_page_count % layoutp->pages) != 0)
		flushpage(outfile);
	if (vflag)
		(void) fprintf(stderr, "\n");
	if (fflush(outfile) == EOF) {
		fatal("write error while flushing output");
		/*NOTREACHED*/
	}
	if (usetmp) {
		if (reverse)
			reversepages(outfile);
		else
			copypage(outfile, 0L, current.offset);
		(void) fclose(outfile);
	}
}

/*
 * Process the next page
 * Return 1 on EOF, 0 otherwise
 */
printpage(infile, outfile)
	FILE	*infile, *outfile;
{
	int	tmplinenumber;
	char	command[BUFSIZ], flag[BUFSIZ];

	if (ungetc(getc(infile), infile) == EOF)
		return(1);

	current.lineno = 0;
	current.lineno += startpage(outfile);
	if (bannerfile) {
		current.lineno += printbanner(bannerfile, outfile);
		bannerfile = NULL;
	}
	for (; current.lineno < lines_per_page; ) {
		if (fgetline(bufin, sizeof(bufin), infile) == (char *) NULL)
			return(1);
		if (iscodereview &&
				/*
				 * Allow C comment delimiters around flag, 
				 * per latest diff -D output for bugfix 
				 * 4218301; only really applies to #else
				 * and #endif, but we don't expect to see 
				 * C comments around flag for #if.
				 * Also accept flag with no C comment 
				 * delimiters.
				 */
                                (sscanf(bufin, "#%s /* %s */", command, 
				flag) == 2 ||
                                sscanf(bufin, "#%s %s", command, flag) == 2) &&    
				strcmp(flag, CODEREVIEW) == 0) {
			if (strcmp(command, "ifdef") == 0) {
				change_seen = 1;
				in_change = 1;
				makegray = 1;
				old_stuff = 1;
				tmplinenumber = linenumber;
				linenumber = altlinenumber;
				altlinenumber = tmplinenumber;
				setcurrentfont(DEFAULT_FONT_ITALIC, outfile);
			}
			else if (strcmp(command, "ifndef") == 0) {
				change_seen = 1;
				in_change = 1;
				makegray = 1;
				old_stuff = 0;
				setcurrentfont(DEFAULT_FONT_BOLD, outfile);
			}
			else if (strcmp(command, "else") == 0) {
				makegray = 1;
				old_stuff = !old_stuff;
				tmplinenumber = linenumber;
				linenumber = altlinenumber;
				altlinenumber = tmplinenumber;
				if (!old_stuff)
					setcurrentfont(DEFAULT_FONT_BOLD,
							outfile);
				else
					setcurrentfont(DEFAULT_FONT_ITALIC,
							outfile);
			}
			else /* if (strcmp(command, "endif") == 0) */ {
				in_change = 0;
				makegray = 0;
				savestate(outfile);
				setcurrentfont(DEFAULT_FONT, outfile);
				if (old_stuff) {
					tmplinenumber = linenumber;
					linenumber = altlinenumber;
					altlinenumber = tmplinenumber;
				}
			}
			continue;
		}
		current.lineno++;
		current.row -= point_size;
		if (bufin[0] == '\f')
			break;
		proc(bufin, outfile);
		if (elide && (bufin[0] == END_C_FUNCTION ||
				(strstr(bufin, END_ASM_FUNCTION) != NULL))) {
			if (!change_seen && !in_change) {
				/* don't include function in output */
				restorestate(outfile);
				if (!dots_inserted) {
					struct format_state	format_state;

					save_format_state(&format_state);
					numberwidth = 0;
					current.lineno++;
					current.row -= point_size;
					setcurrentfont(DEFAULT_FONT_BOLD_ITALIC,
						outfile);
					proc("______unchanged_portion_omitted_",
						outfile);
					restore_format_state(&format_state,
						outfile);
					savestate(outfile);
					dots_inserted = 1;
				}
			}
			else {
				savestate(outfile);
				change_seen = in_change;
				dots_inserted = 0;
			}
		}
	}
	endpage(outfile);
	return(0);
}

/*
 * Start a new page
 */
int
startpage(outfile)
	FILE	*outfile;
{
	int	logical_page, lines, buflen;
	struct format_state	format_state;
	char	buf[8];
	

	logical_page = current.logical_page_count % layoutp->pages;

	if (logical_page == 0)
		setuppage(outfile);
	else
		setcurrentfont((char *)NULL, outfile);
	(void) fprintf(outfile, "%s ", SET_WIDTHS);
	(void) fprintf(outfile, "%d %f %d %d %s\n",
			rot_text, layoutp->scale,
			positions[logical_page].base_x,
			positions[logical_page].base_y,
			START_PAGE);
	lines = 0;
	if (header) {
		save_format_state(&format_state);
		setcurrentfont(DEFAULT_FONT_BOLD, outfile);
		numberwidth = 0;
		makegray = 0;

		current.row -= point_size;
		(void) fprintf(outfile, "%d %.2f %s\n", start_x, current.row,
				MOVETO);
		proc(headerstring, outfile);
		sprintf(buf, "%d", current.logical_page_count + 1);
		buflen = strlen(buf);
		(void) fprintf(outfile, "%d %.2f %s (%s)%s\n",
				(int) (end_x - (buflen + 0.5) *
					DEFAULT_CHAR_WIDTH * point_size),
				current.row, MOVETO, buf, SHOW);
		current.row -= point_size;
		restore_format_state(&format_state, outfile);
		lines = 2;
	}
	return (lines);
}

setheaderfile(filename)
	char		*filename;
{
	if (header == HEADER_IMPLICIT)
		headerstring = filename;
}

/*
 * Setup page
 */
setuppage(outfile)
	FILE		*outfile;
{
	register int	i, ilimit;
	register int	begin, end, place;

	(void) fprintf(outfile, "%%%%Page: ? %d\n", current.page_count + 1);
	setcurrentfont((char *)NULL, outfile);
	if (layoutp->pages == 1)
		return;

	(void) fprintf(outfile, "%f %s %s\n",
			RULE_WIDTH, SETLINEWIDTH, NEWPATH);
	begin = 0; end = DEFAULT_PAPER_WIDTH * POINTS_PER_INCH;
	for (i = 1, ilimit = layoutp->page_rows; i < ilimit; i++) {
		place = margin_y - gap_height/2 + i * (box_height+gap_height);
		(void) fprintf(outfile, "%d %d %s ", begin, place, MOVETO);
		(void) fprintf(outfile, "%d %d %s\n", end, place, LINETO);
	}
	begin = 0; end = DEFAULT_PAPER_HEIGHT * POINTS_PER_INCH;
	for (i = 1, ilimit = layoutp->page_cols; i < ilimit; i++) {
		place = margin_x - gap_width/2 + i * (box_width+gap_width);
		(void) fprintf(outfile, "%d %d %s ", place, begin, MOVETO);
		(void) fprintf(outfile, "%d %d %s\n", place, end, LINETO);
	}
	(void) fprintf(outfile, "%s\n", STROKE);
}

/*
 * Terminate the logical page and indicate the start of the next
 */
endpage(outfile)
	FILE	*outfile;
{
	(void) fprintf(outfile, "%s\n", END_PAGE);
	current.logical_page_count++;
	if (vflag)
		(void) fprintf(stderr, "x");
	if ((current.logical_page_count % layoutp->pages) == 0)
		flushpage(outfile);
}

/*
 * Flush the physical page
 * Record the start of the next page
 */
flushpage(outfile)
	FILE	*outfile;
{
	(void) fprintf(outfile, "%d %s\n", ncopies, FLUSH_PAGE);
	current.page_count++;
	current.offset = ftell(outfile);
	if (reverse) {
		if (current.page_count >= MAXPAGES) {
			fatal("page reversal limit (%d) reached", MAXPAGES);
			/* NOTREACHED */
		}
		page_map[current.page_count] = current.offset;
	}
	if (vflag)
		(void) fprintf(stderr, "|");
}

/*
 * reverse the order of pages
 */
reversepages(outfile)
	FILE		*outfile;
{
	register int	i;

	if (vflag)
		(void) fprintf(stderr, "\nreversing %d page%s\n",
				current.page_count,
				current.page_count > 1 ? "s" : "");
	for (i = current.page_count - 1; i >= 0; i--) {
		copypage(outfile, page_map[i], page_map[i+1]);
	}
}

/*
 * copy a page (or more) from tempfile to stdout
 */
copypage(outfile, off_beg, off_end)
	register FILE	*outfile;
	long		off_beg, off_end;
{
	register int	bytecount, nbytes;

	if (fseek(outfile, off_beg, 0) == -1L) {
		fatal("temporary file seek error");
		/* NOTREACHED */
	}
	nbytes = off_end - off_beg;
	while (nbytes > 0) {
		bytecount = nbytes;
		if (bytecount > sizeof(bufout))
			bytecount = sizeof(bufout);
		bytecount = fread(bufout, 1, bytecount, outfile);
		if (bytecount <= 0) {
			fatal("temporary file read error");
			/* NOTREACHED */
		}
		if (fwrite(bufout, 1, bytecount, stdout) != bytecount) {
			fatal("write error during page copy");
			/* NOTREACHED */
		}
		nbytes -= bytecount;
	}
}

/*
 * Process a line of input, escaping characters when necessary and handling
 * tabs
 *
 * The output is improved somewhat by coalescing consecutive tabs and
 * backspaces and eliminating tabs at the end of a line
 *
 * Overprinting (presumably most often used in underlining) can be far from
 * optimal; in particular the way nroff underlines by sequences like
 * "_\ba_\bb_\bc" creates a large volume of PostScript.  This isn't too
 * serious since a lot of nroff underlining is unlikely.
 *
 * Since a newline is generated for each call there will be more
 * newlines in the output than is necessary
 */
proc(in, outfile)
	char		*in;
	FILE		*outfile;
{
	register int	i;
	register char	*last, *p, *q;
	int		currentp, instr, tabc, tabto, grayed;
	char		*altfont;

	currentp = 0;
	instr = 0;
	tabto = 0;
	if (iscodereview) {
		grayed = makegray;
		altfont = current.font;
	}
	else {
		grayed = 0;
		altfont = DEFAULT_FONT;
	}
	/* subtract slop factor */
	last = bufout + MAX_OUTPUT_LINE_LENGTH - 20;
	for (;;) { /* check for any special line treatment */
		if (graylength && strncmp(in, graystring, graylength) == 0) {
			grayed++;
			in += graylength;
		}
		else if (boldlength &&
				strncmp(in, boldstring, boldlength) == 0) {
			altfont = DEFAULT_FONT_BOLD;
			in += boldlength;
		}
		else if (itlclength &&
				strncmp(in, itlcstring, itlclength) == 0) {
			altfont = DEFAULT_FONT_ITALIC;
			in += itlclength;
		}
		else if (bitclength &&
				strncmp(in, bitcstring, bitclength) == 0) {
			altfont = DEFAULT_FONT_BOLD_ITALIC;
			in += bitclength;
		}
		else
			break;
	}
	if (grayed) {
		(void) fprintf(outfile, "%d %.2f %d %.2f %s\n",
			start_x, current.row -
				DEFAULT_DESCENDER_FRACTION * point_size,
			end_x, current.row +
				(1.0 - DEFAULT_DESCENDER_FRACTION) * point_size,
			SHADE);
	}

	linenumber++;
	if (!in_change)
		altlinenumber++;
	if (*in == '\0')
		return;

	if (start_x != 0) {
		(void) fprintf(outfile, "%d %.2f %s\n",
				start_x, current.row, MOVETO);
	}
	else
		(void) fprintf(outfile, "%.2f %s\n",
				current.row, ZEROMOVETO);
	if (numberwidth) {
		setcurrentfont(DEFAULT_FONT, outfile);
		(void) sprintf(bufout, "%*d", numberwidth, linenumber);
		for (q = bufout, i = 0; *q == ' '; q++, i++)
			;
		(void) fprintf(outfile, "%d %s (%s)%s %d %s ",
			i, TAB, q, SHOW, DEFAULT_SPACES_AFTER_NUMBER, TAB);
	}
	setcurrentfont(altfont, outfile);

	q = bufout;
	*q = '\0';
	for (p = in; *p != '\0'; p++) {
		switch (*p) {
		case '\t':
			/*
			 * Count the number of tabs that immediately follow
			 * the one we're looking at
			 */
			tabc = 0;
			while (*(p + 1) == '\t') {
				p++;
				tabc++;
			}
			if (currentp > 0) {	/* not beginning of line */
				i = tabstop - (currentp % tabstop) +
						tabc * tabstop;
				if (instr) {
					(void) sprintf(q, ")%s ", SHOW);
					q += strlen(q);
					instr = 0;
				}
			}
			else
				i = (tabc + 1) * tabstop;
			tabto += i;
			currentp += i;
			break;
		case '\b':
			/* backspacing over tabs doesn't work... */
			if (tabto != 0) {
				fatal("attempt to backspace over a tab");
				/*NOTREACHED*/
			}
			p++;
			for (i = 1; *p == '\b'; p++)
				i++;
			p--;
			if (currentp - i < 0) {
				fatal("too many backspaces");
				/*NOTREACHED*/
			}
			if (instr) {
				*q = '\0';
				(void) fprintf(outfile, "%s)%s\n",
						bufout, SHOW);
			}
			instr = 0;
			if (currentp >= columns)
				i -= currentp-columns;
			if (i <= 0) {
				/* backspace in truncated line */
				bufout[0] = '\0';
			}
			else if (i == 1) {
				/* frequent case gets special attention */
				(void) sprintf(bufout, "%s ", BACKSPACE);
			}
			else
				(void) sprintf(bufout, "-%d %s ", i, TAB);
			q = bufout + strlen(bufout);
			currentp -= i;
			break;
		case '\f':
			tabto = 0;		/* optimizes */
			*q = '\0';
			if (instr)
				(void) fprintf(outfile, "%s)%s\n",
						bufout, SHOW);
			else
				(void) fprintf(outfile, "%s\n", bufout);
			endpage(outfile);
			startpage(outfile);
			current.row = start_y;
			(void) fprintf(outfile, "%d %.2f %s\n",
					start_x, current.row, MOVETO);
			if (numberwidth)
				(void) fprintf(outfile, "%d %s\n", numberwidth +
					DEFAULT_SPACES_AFTER_NUMBER, TAB);
			q = bufout;
			currentp = 0;
			instr = 0;
			break;
		case '\r':
			tabto = 0;		/* optimizes */
			if (instr) {
				*q = '\0';
				(void) fprintf(outfile, "%s)%s\n",
						bufout, SHOW);
				instr = 0;
				q = bufout;
			}
			(void) fprintf(outfile, "%d %.2f %s\n",
					start_x, current.row, MOVETO);
			if (numberwidth)
				(void) fprintf(outfile, "%d %s\n", numberwidth +
					DEFAULT_SPACES_AFTER_NUMBER, TAB);
			currentp = 0;
			break;
		case '\\':
		case '(':
		case ')':
			if (currentp < columns) {
				if (!instr) {
					if (tabto) {
						(void) sprintf(q, "%d %s ",
								tabto, TAB);
						q += strlen(q);
						tabto = 0;
					}
					*q++ = '(';
					instr = 1;
				}
				*q++ = '\\';
				*q++ = *p;
			}
			currentp++;
			break;
		default: {
			/*
			 * According to the PostScript Language Manual,
			 * PostScript files can contain only "the printable
			 * subset of the ASCII character set (plus the
			 * newline marker)".
			 */
			char	pchar;

			pchar = *p;
			if (currentp < columns) {
				if (!instr) {
					if (tabto) {
						(void) sprintf(q, "%d %s ",
								tabto, TAB);
						q += strlen(q);
						tabto = 0;
					}
					*q++ = '(';
					instr = 1;
				}
				if (!isascii(pchar) || !isprint(pchar)) {
					if (iscntrl(pchar)) {
						if (pchar == '\177')
							pchar = '_';
						else
							pchar += '@';
						*q++ = '^';
					}
					else {
						*q++ = '\\';
						*q++ = '0' + ((pchar>>6) & 7);
						*q++ = '0' + ((pchar>>3) & 7);
						pchar = '0' + (pchar & 7);
					}
				}
				*q++ = pchar;
			}
			currentp++;
			break;
			}
		}
		if (q >= last) {
			*q = '\0';
			if (instr)
				(void) fprintf(outfile, "%s)%s\n", bufout, SHOW);
			else
				(void) fprintf(outfile, "%s\n", bufout);
			q = bufout;
			instr = 0;
		}
	}
	if (instr) {
		(void) sprintf(q, ")%s", SHOW);
		q += strlen(q);
	}
	else
		*q = '\0';
	if (q >= last) {
		fatal("bufout overflow");
		/*NOTREACHED*/
	}
	if (bufout[0] != '\0')
		(void) fprintf(outfile, "%s\n", bufout);
}

/*
 * Initialize globals:
 *	username - login name of user
 *	hostname - name of machine on which lwlp is run
 *	currentdate - what it says
 * Possible system dependencies here...
 */
setup()
{
	int	len;
	char	*p;
	long	t;
	struct utsname	utsname;
	struct passwd	*pw;

	if ((p = getlogin()) == (char *) NULL) {
		if ((pw = getpwuid(getuid())) == (struct passwd *) NULL)
			p = "Whoknows";
		else
			p = pw->pw_name;
		endpwent();
	}
	username = (char*) malloc((unsigned) (strlen(p) + 1));
	(void) strcpy(username, p);

	(void) uname(&utsname);
	hostname = (char*) malloc((unsigned) (strlen(utsname.nodename) + 1));
	(void) strcpy(hostname, utsname.nodename);
	release = (char*) malloc((unsigned) (strlen(utsname.release) + 1));
	(void) strcpy(release, utsname.release);

	t = time((long *) 0);
	p = ctime(&t);
	len = strlen(p);
	*(p + len - 1) = '\0';		/* zap the newline character */
	currentdate = (char*) malloc((unsigned) len);
	(void) strcpy(currentdate, p);
	current.font = DEFAULT_FONT;
}

/*
 * Special version of fgets
 * Read until a formfeed, newline, or overflow
 * If a formfeed is the first character, return it immediately
 * If a formfeed is found after the first character, replace it by a newline
 * and push the formfeed back onto the input stream
 * A special case is a formfeed followed by a newline in which case the
 * newline is ignored 
 * The input buffer will be null-terminated and will *not* end with a newline
 * The buffer size n includes the null
 */
char *
fgetline(s, n, iop)
	char		*s;
	int		n;
	register FILE	*iop;
{
	register int	ch;
	register char	*cs;

	if (n < 2) {
		fatal("fgetline called with bad buffer size!?");
		/*NOTREACHED*/
	}

	cs = s;
	n--;				/* the null */

	/*
	 * Check out the special cases
	 */
	if ((ch = getc(iop)) == EOF)
		return((char *) NULL);
	if (ch == '\f') {
		if ((ch = getc(iop)) != '\n') {
			/*
			 * If EOF was just read it will be noticed
			 * next time through
			 */
			if (ungetc(ch, iop) == EOF && !feof(iop)) {
				/* Shouldn't happen since a getc()
				 * was just done */
				fatal("fgetline - ungetc failed");
				/*NOTREACHED*/
			}
		}
		*cs++ = '\f';
		*cs = '\0';
		return(s);
	}

	/*
	 * Check for "weird" input characters is made in proc()
	 */
	while (n-- > 0) {
		if (ch == '\f' || ch == '\n')
			break;
		*cs++ = ch;
		if ((ch = getc(iop)) == EOF)
			break;
	}

	if (ch == EOF && cs == s)		/* Nothing was read */
		return((char *) NULL);
	if (ch == '\f') {
		if (ungetc(ch, iop) == EOF)
			(void) fprintf(stderr, "fgetline - can't ungetc??\n");
	}
	else if (ch != '\n' && ch != EOF) {
		fatal("fgetline - input line too long");
		/*NOTREACHED*/
	}
	*cs = '\0';
	return(s);
}

/*VARARGS*/
fatal(s, a, b, c, d, e, f, g, h, i, j)
	char	*s;
{
	(void) fprintf(stderr, "%s: ", progname);
	(void) fprintf(stderr, s, a, b, c, d, e, f, g, h, i, j);
	(void) fprintf(stderr, "\n");
	exit(1);
	/*NOTREACHED*/
}
