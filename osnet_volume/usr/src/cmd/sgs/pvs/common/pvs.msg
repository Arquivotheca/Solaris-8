#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)pvs.msg	1.6	98/08/28 SMI"


@ _START_

# Message file for cmd/sgs/pvs.

@ MSG_ID_PVS


# Argument usage messages.

@ MSG_USAGE_BRIEF	"usage: %s [-dlnorsv] [-N Name] file(s)\n"
@ MSG_USAGE_DETAIL	"\t[-d print version definition information]\n\
			 \t[-l print reduced symbols]\n\
			 \t[-n normalize version definition information]\n\
			 \t[-o one-line version definition output]\n\
			 \t[-r print version dependency (requirements) \
			  information]\n\
			 \t[-s print version definition symbols]\n\
			 \t[-v print verbose information]\n\
			 \t[-N qualify version with Name]\n"


# Versioning messages.

@ MSG_VER_HIGHREV	"%s: warning: %s: version revision %d is \
			 higher than expected %d\n"
@ MSG_VER_UNREDSYMS	"%s: warning: %s: unable to deduce reduced symbols:\n"
@ MSG_VER_NOTSTTFILE	"\tmalformed symbol table[1]=%s: does not equal \
			 STT_FILE\n"
@ MSG_VER_NOSYMTAB	"\tno symbol table (SHT_SYMTAB) found\n"


# ELF messages
@ MSG_ELF_NOTELF	"%s: %s: is not ELF\n"

# System error messages

@ MSG_SYS_OPEN		"%s: %s: cannot open file: %s\n"
@ MSG_SYS_MALLOC	"%s: %s: malloc failed: %s\n"

@ _END_

# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

# ELF messages
@ MSG_ELF_BEGIN		"%s: %s: elf_begin: %s\n"
@ MSG_ELF_GETEHDR	"%s: %s: elf_getehdr: %s\n"
@ MSG_ELF_GETSCN	"%s: %s: elf_getscn: %s\n"
@ MSG_ELF_GETSHDR	"%s: %s: elf_getshdr: %s\n"
@ MSG_ELF_GETDATA	"%s: %s: elf_getdata: %s\n"

@ MSG_STR_EMPTY		""

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
