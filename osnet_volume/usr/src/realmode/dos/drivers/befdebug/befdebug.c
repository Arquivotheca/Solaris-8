/*
 *  Copyright (c) 1997 by Sun Microsystems, Inc.
 *  All Rights Reserved.
 */

/*
 * The #ident directive is commented out because it causes an error
 * in the MS-DOS linker.
 *
#ident "@(#)befdebug.c	1.8	97/05/09 SMI"
 */

/*
 *	MS-DOS program for helping to debug realmode drivers.
 *
 *	We build two different versions of this program (see
 *	the makefile for full details).  The standard version
 *	is called BEFDEBUG and is intended for use by driver
 *	developers.  It is a terminate-and-stay-resident
 *	program that runs under MS-DOS and creates a partial
 *	emulation of the Solaris boot subsystem environment.
 *	The EXE versions of realmode drivers can be run under
 *	debuggers such as CodeView for source level driver
 *	debugging.
 *
 *	The debug version is called DBGDEBUG and is intended
 *	for BEFDEBUG developers.  DBGDEBUG acts as a normal
 *	MS-DOS program and uses EXEC to run the EXE version
 *	of a driver.  This version allows source level
 *	debugging of BEFDEBUG itself.
 */

#include "befdebug.h"

void comarp(struct arp_packet *, char *);
void char_from_program(int);
int far_strcmp(char far *, char far *);
struct befdebug_data far *find_installed(int);
static void coded_char(int, int);
ulong get_entry_point(void);
void parse_data(char *);
ulong read_vector(ushort);
void revarp(char *);
void save_data(char *);
static void service(ushort, ushort, ushort);
void set_name(char *, char far *);
void show_data(void);
void stay_resident(ushort, ushort);
void update_data(void);
void write_vector(ushort, ulong);

#ifdef DEBUG
char *prog_name = "dbgdebug";
#else
char *prog_name = "befdebug";
#endif

ulong my_entry_point;

struct befdebug_data bd = { MAX_NODES, sizeof (struct befdebug_data) };

struct name_value {
	char *name;
	ulong value;
} name_table[] = {
	"RES_BUS_ISA",			RES_BUS_ISA,
	"RES_BUS_EISA",			RES_BUS_EISA,
	"RES_BUS_PCI",			RES_BUS_PCI,
	"RES_BUS_PCMCIA",		RES_BUS_PCMCIA,
	"RES_BUS_PNPISA",		RES_BUS_PNPISA,
	"RES_BUS_MCA",			RES_BUS_MCA
};

#define	DEFAULT_PAUSE	20
#define	PROGRAM_OUTPUT	1
#define	DRIVER_OUTPUT	2

void
usage(void)
{
	bd.pause_count = 0;
	printf("\nUsage:\t%s [-d] [-h] [-p[NN]] "
#ifdef DEBUG
		"[-r file] [-x] "
#endif
		"probe [file]\n", prog_name);
	printf("or:\t%s save file\n", prog_name);
	printf("or:\t%s [-d] [-h] [-p[NN]] "
#ifdef DEBUG
		"[-zr] [-zw] "
#endif
		"install file\n\n", prog_name);
	printf("where:\t-d means display debugging trace output\n");
	printf("\t-h means hide output from the driver under test\n");
	printf("\t-p means pause until a keystroke after every NN lines ");
	printf("(default %d)\n", DEFAULT_PAUSE);
#ifdef DEBUG
	printf("\t-r means run driver directly from %s\n", prog_name);
	printf("\t-x means discard installed copy of %s\n", prog_name);
	printf("\t-zr or -zw means ignore REVARP or WHOAMI replies\n");
#endif
	printf("\n\tThe probe function prepares to test the BEF_LEGACYPROBE\n");
	printf("\tfunctionality of a driver.  The optional input file\n");
	printf("\trepresents a list of device nodes already defined.\n\n");
	printf("\tThe save function writes saved data from a previous\n");
	printf("\tBEF_LEGACYPROBE test to the specified data file.\n\n");
	printf("\tThe install function prepares to test the BEF_INSTALLONLY\n");
	printf("\tfunctionality of a driver.  The required input file\n");
	printf("\trepresents a list of device nodes for devices to be\n");
	printf("\tconfigured in the driver.\n");

	dos_exit(1);
}


/*
 * This version installs as a terminate-and-stay-resident program
 * and the driver calls into it by following a vector.
 */
void
main(int argc, char *argv[])
{
	ulong saved_FB_vector;
	ushort max_avail;
	ushort new_block;
	ushort my_size;
	int i;
	char *file_name = 0;
#ifdef DEBUG
	char *run_file;
	ushort run_flag = 0;
#endif
	extern unsigned short _psp;
	
#ifdef DEBUG
	MODULE_DEBUG_FLAG = DEBUG_FLAG;
#endif

	/* There is no default function */
	bd.function = TEST_NONE;

	/*
	 * Parse the arguments.  Allow for upper case versions of
	 * everything because MS-DOS debug program 'name' command
	 * translates everything to upper case.
	 */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case 'd': case 'D':
				bd.user_debug_flag = 1;
				break;
			case 'h': case 'H':
				bd.misc_flags |= HIDE_FLAG;
				break;
			case 'p': case 'P':
				bd.pause_count = DEFAULT_PAUSE;
				if (argv[i][2] >= '0' && argv[i][2] <= '9') {
					bd.pause_count = argv[i][2] - '0';
					if (argv[i][3] >= '0' &&
							argv[i][3] <= '9') {
						bd.pause_count *= 10;
						bd.pause_count +=
							argv[i][3] - '0';
					}
				}
				break;
#ifdef DEBUG			
			case 'r': case 'R':
				run_flag = 1;
				if (argv[i][2])
					run_file = argv[i] + 2;
				else if (++i < argc)
					run_file = argv[i];
				else
					usage();
				/*
				 * Uninstall any terminate-and-stay resident
				 * copy.
				 */
				find_installed(1);
				break;			
#endif			
			case 'x': case 'X':
				/*
				 * Undocumented feature for befdebug
				 * development.  -x uninstalls an installed
				 * copy so that new code will run.  Always
				 * compiled in so that a non-DEBUG version
				 * can uninstall a DEBUG one.
				 */
				find_installed(1);
				dos_exit(0);
			case 'z': case 'Z':
				/*
				 * Undocumented fature for befdebug
				 * development.  -zr causes the program to
				 * ignore the expected REVARP response to
				 * test the program behavior.  -zw similarly
				 * means to ignore WHOAMI responses.
				 */
				if (argv[i][2] == 'r' || argv[i][2] == 'R')
					bd.misc_flags |= IGNORE_REVARP;
				else if (argv[i][2] == 'w' ||
						argv[i][2] == 'W')
					bd.misc_flags |= IGNORE_WHOAMI;
				else {
					printf("Unknown switch \"%s\".\n\n",
						argv[i]);
					usage();
				}
				break;
			default:
				printf("Unknown switch \"%s\".\n\n", argv[i]);
				usage();
			}
		} else if (strncmp(argv[i], "probe", strlen(argv[i])) == 0 ||
		    strncmp(argv[i], "PROBE", strlen(argv[i])) == 0) {
			if (i + 1 < argc) {
				i++;
				file_name = argv[i];
			}
			bd.function = TEST_PROBE;
			break;
		} else if (strncmp(argv[i], "install", strlen(argv[i])) == 0 ||
		    strncmp(argv[i], "INSTALL", strlen(argv[i])) == 0) {
			if (argc <= i + 1)
				usage();
			i++;
			file_name = argv[i];
			bd.function = TEST_INSTALL;
			break;
		} else if (strncmp(argv[i], "save", strlen(argv[i])) == 0 ||
		    strncmp(argv[i], "SAVE", strlen(argv[i])) == 0) {
			if (argc <= i + 1)
				usage();
			i++;
			file_name = argv[i];
			bd.function = SAVE_DATA;
			break;
		}
		else
			usage();
	}
	
	if (bd.function	== TEST_NONE)
		usage();
	
	if ((bd.function == TEST_PROBE && file_name) ||
			bd.function == TEST_INSTALL)
		parse_data(file_name);
	
	/* If there is already a befdebug installed, read/write its data */
	update_data();

	if (bd.function	== SAVE_DATA) {
		show_data();
		save_data(file_name);
		printf("Data saved to file \"%s\".\n", file_name);
		dos_exit(0);
	}

	/* Adjust memory allocation ready for terminate and stay resident */
	dos_alloc(0xFFFF, &max_avail);
	dos_alloc(max_avail, &new_block);
	dos_mem_adjust(_psp, 0xFFFF, &my_size);
	dos_free(new_block);
	Dprintf(DBG_ALL, ("%s occupies %x paragraphs at %x.\n",
		prog_name, my_size, _psp));

	/* Use the INT FB vector to announce my presence */
	my_entry_point = get_entry_point();
	saved_FB_vector = read_vector(0xFB);
	write_vector(0xFB, my_entry_point);

#ifdef DEBUG
	if (run_flag) {
		if (dos_exec(run_file, "") != 0)
			printf("Failed to execute \"%s\".\n", run_file);

		/* Restore the original INT FB vector */
		write_vector(0xFB, saved_FB_vector);
		dos_exit(0);
	}
#endif

	printf("Installing new %s for BEF %s testing.\n", prog_name,
		bd.function == TEST_PROBE ? "probe" : "install");

	stay_resident(0, my_size);
	printf("%s internal error: installation may have failed.\n",
		prog_name);
}

struct befdebug_data far *
find_installed(int uninstall)
{
	char far *s;
	struct befdebug_data far *dd;

	s = ((char far *)read_vector(0xFB)) + 2;
	if (s[0] != 'B' || s[1] != 'E' || s[2] != 'F' || s[3] != 'D' ||
			s[4] != 'E' || s[5] != 'B' || s[6] != 'U' ||
			s[7] != 'G') {
		Dprintf(DBG_ALL, ("No previously installed version.\n"));
		return (0);
	}
	dd = *(struct befdebug_data far * far *)(s + 8);

	if (dd->table_max != bd.table_max ||
			dd->struct_size != bd.struct_size) {
		if (uninstall == 0)
			printf("Installed %s is incompatible with new one.\n",
				prog_name);
		uninstall = 1;
	}
	if (uninstall) {
		printf("De-installing old %s.\n", prog_name);
		s[0] = 'X';
		return (0);
	}
	return (dd);
}

/*
 *	Look for an existing installed befdebug.  If function is
 *	probe or install, update the installed befdebug data from
 *	this copy and exit.  If function is save data, update this copy
 *	from the installed one.
 */
void
update_data(void)
{
	struct befdebug_data far *dd;
	ushort i, j;

	dd = find_installed(0);
	if (dd == 0) {
		if (bd.function == SAVE_DATA) {
			printf("Cannot find installed %s with data to save.\n",
				prog_name);
			dos_exit(1);
		}
		return;
	}

	/* SAVE: grab the data and return or fail */
	if (bd.function == SAVE_DATA) {
		if (dd->function != TEST_PROBE) {
			printf("Installed %s was not set up for probe.\n",
				prog_name);
			dos_exit(1);
		}
		
		if (dd->exercised == 0) {
			printf("Installed %s has not tested any drivers.\n",
				prog_name);
			dos_exit(1);
		}

		if (dd->node_called = 0) {
			printf("Driver under test never called node_op().\n");
			dos_exit(1);
		}

		if (dd->table_size == 0) {
			printf("Driver under test found no devices.\n");
			dos_exit(1);
		}
		
		/*
		 * Cannot just blindly copy entire table.  Just copy the
		 * part that is in use, adjusting "node_start" to "node"
		 * and discarding "node_end".
		 */
		Dprintf(DBG_ALL,
			("Installed %s contains %d data items.\n",
			prog_name, dd->table_size));
		bd.table_size = 0;
		for (i = 0, j = 0; i < dd->table_size; i++) {
			bd.node_tab[j] = dd->node_tab[i];
			if (dos_strcmp(bd.node_tab[j].name, "node_end")
					== 0)
				continue;
			if (dos_strcmp(bd.node_tab[j].name,
					"node_start") == 0)
				set_name(bd.node_tab[j].name, "node");
			bd.table_size++;
			j++;
		}
		return;
	}

	/* PROBE or INSTALL: try to write the data then exit */
	printf("Updating data in previously installed %s.\n",
		prog_name);
	*dd = bd;

	dos_exit(0);
}

void
parse_data(char *file_name)
{
	void *stream;
	int line = 0;
	int parse_errors = 0;
	char buffer[512];

	if (bd.function == TEST_PROBE) {
		printf("Data file for probe function not implemented yet "
			"(ignored).\n");
		return;
	}
	bd.table_size = 0;
	stream = dos_open(file_name, "r");
	if (stream == 0) {
		printf("Cannot open file \"%s\".\n", file_name);
		dos_exit(1);
	}
	while (dos_get_line(buffer, sizeof (buffer) - 1, stream)) {
		line++;
		parse_errors += parse_line(line, buffer);
	}
	dos_close(stream);
	if (parse_errors != 0) {
		printf("%d parse error%s in file \"%s\".\n",
			parse_errors, parse_errors == 1 ? "" : "s", file_name);
		dos_exit(1);
	}
}

/*
 *	Parse a line possibly representing a node or resource.
 *	Store any data in the table.
 *	Return the number of errors reported.
 */
int
parse_line(int line, char *buffer)
{
	char *token;
	ushort count;
	ushort i;
	ulong val;

	token = strtok(buffer, " \t\r\n");
	if (token == 0 || *token == 0 || *token == '#')
		return (0);

	if (bd.table_size >= bd.table_max) {
		printf("Line %d: too much data for %s table.\n",
			line, prog_name);
		return (1);
	}

	if (strlen(token) >= NAME_SIZE) {
		printf("Line %d: unknown entry type \"%s\".\n",
			line, token);
		return (1);
	}
	dos_strcpy(bd.node_tab[bd.table_size].name, token);

	token = strtok((char *)0, " \t\r\n");
	if (token == 0 || *token == 0 || *token == '#') {
		bd.table_size++;
		return (0);
	}

	if (sscanf(token, "%x", &count) != 1) {
		printf("Line %d: expected tuple count at \"%s\".\n",
			line, token);
		return (1);
	}
	bd.node_tab[bd.table_size].len = count;

	for (i = 0; i < count; i++) {
		token = strtok((char *)0, " \t\r\n");
		if (token == 0) {
			printf("Line %d: expected %d values, found %d.\n",
				line, count, i);
			return (1);
		}
		/*
		 * Look for names before numbers because names can start 
		 * letters that are valid hex digits.
		 */
		if (lookup_name(token, &val) == 0 &&
				sscanf(token, "%lx", &val) != 1) {
			printf("Line %d: expected tuple value at \"%s\".\n",
				line, token);
			return (1);
		}
		bd.node_tab[bd.table_size].val[i] = val;
	}

	token = strtok((char *)0, " \t\r\n");
	if (token != 0 && token != 0 && *token != '#') {
		printf("Line %d: extra input found at \"%s\".\n",
			line, token);
	}
	
	bd.table_size++;
	return (0);
}

int
lookup_name(char *token, ulong *p)
{
	int i;
	
	for (i = 0; i < sizeof (name_table) / sizeof (struct name_value); i++)
		if (dos_strcmp(name_table[i].name, token) == 0) {
			*p = name_table[i].value;
			return (1);
		}
	return (0);
}

void
save_data(char *file_name)
{
	void *stream;
	int i;
	ushort j;

	stream = dos_open(file_name, "w");
	if (stream == 0) {
		printf("Cannot write file \"%s\".\n", file_name);
		dos_exit(1);
	}
	for (i = 0; i < bd.table_size; i++) {
		dos_fprintf(stream, "%s", bd.node_tab[i].name);
		if (bd.node_tab[i].len) {
			dos_fprintf(stream, "\t%d", bd.node_tab[i].len);
			for (j = 0; j < bd.node_tab[i].len; j++)
				dos_fprintf(stream, "\t%lx",
					bd.node_tab[i].val[j]);
		}
		dos_fprintf(stream, "\n");
	}
	dos_close(stream);
}

/*
 *	The BEF under test calls on this program via the INT FB handler
 *	which in turn calls this routine after setting up appropriate
 *	context.
 */
static void
service(ushort bef_code, ushort end_off, ushort bef_data)
{
	ulong saved_int13_vector;
	ushort psp;
	ushort new_size;
	ushort max = 0;

	bd.exercised = 1;

	/*
	 * Adjust the memory allocated to the driver to reach to _end
	 * to match the boot subsystem behavior.
	 */
	psp = dos_get_psp();
	new_size = bef_data + ((end_off + 15) >> 4) - psp;
	if (dos_mem_adjust(psp, new_size, &max) != 0) {
		printf("Failed to adjust driver size to %x.\n",
			new_size);
		printf("Maximum available is %x paragraphs.\n", max);
		return;
	}
	
	/* Save the original INT 13 vector */
	saved_int13_vector = read_vector(0x13);

	test_bef(bef_code, psp, new_size);

	/* Reclaim my entry vector in case a network driver grabbed it */
	write_vector(0xFB, my_entry_point);

	/* Restore the INT 13 vector to avoid messing up DOS */
	write_vector(0x13, saved_int13_vector);

	if (bd.function == TEST_PROBE)
		show_data();
}

void
show_data(void)
{
#ifdef DEBUG
	ushort i;
	ushort j;
	
	for (i = 0; i < bd.table_size; i++) {
		printf("%s", bd.node_tab[i].name);
		if (bd.node_tab[i].len > 0) {
			printf("\t%d", bd.node_tab[i].len);
			for (j = 0; j < bd.node_tab[i].len &&
					j < MaxTupleSize; j++)
				printf("\t%lx",
					bd.node_tab[i].val[j]);
		}
		printf("\n");
	}
#endif
}

void
set_name(char *name, char far *value)
{
	int i;
	
	for (i = 0; i < NAME_SIZE - 1 && value[i]; i++)
		name[i] = value[i];
	name[i] = 0;	
}

/*
 *	This routine is used for the 'putc' entry in the callback structure.
 */
int
char_from_driver(int a)
{
	if (callback_state != CALLBACK_ALLOWED) {
		printf("%s: driver made a %s callback after initial return\n",
			prog_name, "putc");
	}

	if ((bd.misc_flags & HIDE_FLAG) == 0)
		coded_char(a, DRIVER_OUTPUT);
	return (BEF_OK);
}

/*
 * Very basic printf to avoid using the MS-DOS library version
 * which complains about stack overflow if called when we are
 * using the driver stack.
 *
 * Implementation is limited to formats used in the program.
 */
int
printf(const char *fmt, ...)
{
	const char *s;
	char *n;
	char *str;
	static char *digits = "0123456789ABCDEF";
	char buffer[20];
	int base;
	ushort sval;
	ulong val;
	int width;

	n = (char *)&fmt;
	n += sizeof (fmt);
	for (s = fmt; s[0]; s++) {
		if (s[0] != '%')
			char_from_program(s[0]);
		else switch (s[1]) {
		case '%':
			char_from_program('%');
			s++;
			break;
		case 's':
			for (str = *(char **)n; str[0]; str++)
				char_from_program(str[0]);
			n += sizeof (char *);
			s++;
			break;
		case '0':
			if (s[2] == '.' && s[3] >= '1' && s[3] <= '9' && 
					s[4] == 'x') {
				width = s[3] - '0';
				sval = *(ushort *)n;
				n += sizeof (ushort);
				val = (ulong)sval;
				base = 16;
				s += 3;
				goto num_common;
			}
			char_from_program('%');
			break;
		case 'l':
			if (s[2] == 'd') {
				width = 1;
				val = *(ulong *)n;
				n += sizeof (ulong);
				base = 10;
				s++;
				goto num_common;
			}
			if (s[2] == 'x') {
				width = 1;
				val = *(ulong *)n;
				n += sizeof (ulong);
				base = 16;
				s++;
				goto num_common;
			}
			char_from_program('%');
			break;
		case 'd':
		case 'x':
			width = 1;
			sval = *(ushort *)n;
			n += sizeof (ushort);
			switch (s[1]) {
			case 'd':
				val = (long)(int)sval;
				base = 10;
				break;
			case 'x':
				val = (ulong)sval;
				base = 16;
				break;
			}
	num_common:
			if (base == 10 && (long)val < 0) {
				char_from_program('-');
				val = 0 - val;
			}
			buffer[19] = 0;
			str = &buffer[19];
			do {
				str--;
				*str = digits[val % base];
				val /= base;
				width--;
			} while (val > 0);
			for (; width > 0; width--)
				char_from_program('0');
			for (; str[0]; str++)
				char_from_program(str[0]);
			s++;
			break;
		default:
			char_from_program('%');
			break;
		}
	}
}

void
char_from_program(int a)
{
	coded_char(a, PROGRAM_OUTPUT);
}

/*
 * Distinguish output from different sources.  For now the driver
 * output is surrounded by "<< ... >>".  Changing colors would be
 * better.
 */
static void
coded_char(int a, int source)
{
	static pause_count;
	char c;
	static last_source = PROGRAM_OUTPUT;
	static char *sgr[] = {
		0,
		">>",
		"<<"
	};

	/*
	 * Pretend all line end chars come from the program.  That
	 * way the end-of-driver-output marks appear on the same
	 * line as the output.  Might need to remove this test
	 * after changing to attributes.
	 */
	if (a == '\n' || a == '\r')
		source = PROGRAM_OUTPUT;

	if (source != last_source) {
		dos_write(0, sgr[source], strlen(sgr[source]));
		last_source = source;
	}

	/*
	 * Pause before doing the line end if it is time to halt the output.
	 * This test follows the source change test so that the end-of-
	 * driver-output marks appear before the pause.
	 */
	if (a == '\n') {
		if (pause_count == 0)
			pause_count = bd.pause_count;
		if (pause_count > 0) {
			if (--pause_count == 0)
				dos_kb_char();
		}
		coded_char('\r', source);
	}

	c = a;
	dos_write(0, &c, 1);
}

void
stay_resident(ushort exit_code, ushort res_size)
{
	_asm {
		mov	ax, exit_code
		mov	ah, 31h
		mov	dx, res_size
		int	21h
	}	
}

/*
 *	This routine does double duty.  When called directly
 *	it returns an address within itself that is the
 *	interrupt entry point.  When interrupt entry occurs
 *	it calls "service" then returns to the caller.
 */
ulong
get_entry_point(void)
{
	static ulong answer;
	static ushort my_sp;
	static ushort my_ss;
	static ulong ret_addr;
	
	_asm {

;		Set up some values to use when invoked by the
;		driver via INT FBh.  Leave some spare room on
;		my stack because with the -r flag there will
;		be EXEC context on the stack.  If we clobber
;		that there will be a problem when the driver
;		exits.
		mov	cs:ds_ptr, ds
		lea	ax, bd
		mov	cs:data_ptr, ax
		mov	my_ss, ss
		mov	my_sp, sp
		sub	my_sp, 100h
		call	later
	
;		Interrupt entry point.  Followed by signature so that
;		drivers can make sure I am installed before attempting
;		to call me and by a far pointer to bd.
;
;		Entry is done by equivalent of software INT.  DS:AX
;		addresses _end in driver.
		jmp	short past
	
;		db	"BEFDEBUG"
;		Compiler did not like 'db'.  So spell it out the hard way.
		inc	dx;		'B'
		inc	bp;		'E'
		inc	si;		'F'
		inc	sp;		'D'
		inc	bp;		'E'
		inc	dx;		'B'
		push	bp;		'U'
		inc	di;		'G'
	data_ptr:
		nop
		nop
	ds_ptr:
		nop
		nop
	past:
		pop	cx;		save IP
		pop	dx;		save CS

;		Move the driver data segment
		mov	bx, ds

;		Find my data segment
		mov	ds, cs:ds_ptr

;		Save the return addr
		mov	word ptr ret_addr, cx
		mov	word ptr ret_addr+2, dx

;		Switch to my stack
		mov	ss, my_ss
		mov	sp, my_sp

;		Do my main thing
		push	bx;		driver DS
		push	ax;		driver _end offset
		push	dx;		driver CS
		call	service
		pop	ax

;		Return to caller.  No context saved because
;		calls from service destroyed the caller stack
;		anyway.
		call	dword ptr [ret_addr]
	
	later:
		pop	word ptr answer
		mov	word ptr answer+2, cs
	}
	return (answer);
}

ulong
read_vector(ushort vector)
{
	ulong far *loc = (ulong far *)(vector * 4);

	return (*loc);
}

void
write_vector(ushort vector, ulong contents)
{
	ulong far *loc = (ulong far *)(vector * 4);
	
	*loc = contents;
}

int
far_strcmp(char far *a, char far *b)
{
	for (; *a == *b; a++, b++)
		if (*a == 0)
			return (0);
	return (*a < *b ? -1 : 1);
}
