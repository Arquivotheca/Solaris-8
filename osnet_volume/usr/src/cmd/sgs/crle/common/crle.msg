#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)crle.msg	1.1	99/08/13 SMI"


@ _START_

# Message file for cmd/sgs/crle.

@ MSG_ID_CRLE


# Argument usage messages.

@ MSG_ARG_USAGE		"usage: %s [-64] [-a name ] [-c conf] [-f flags] \
			 [-i name] [-I name ]\n\t[-g name] [-G name] [-l dir] \
			 [-o dir ] [-s dir] [-t ELF|AOUT] [-v]\n"

@ MSG_ARG_MULT		"%s: argument %s specified multiple times\n"
@ MSG_ARG_TYPE		"%s: unknown object type: %s\n"
@ MSG_ARG_FLAGS		"%s: unknown flags: %s\n"
@ MSG_ARG_MODE		"%s: RTLD_REL_EXEC cannot be used with multiple \
			 dynamic executables\n"
@ MSG_ARG_ALT		"%s: %s: cannot override original file with \
			 alternative; specify -o\n"

# System error messages

@ MSG_SYS_MALLOC	"%s: malloc failed: %s\n"
@ MSG_SYS_TEMPNAME	"%s: unable to create temporary filename: %s\n"
@ MSG_SYS_PIPE		"%s: pipe failed: %s\n"
@ MSG_SYS_FORK		"%s: fork failed: %s\n"
@ MSG_SYS_PUTENV	"%s: putenv failed: %s\n"

@ MSG_COR_TRUNC		"%s: %s: corrupt file, possible truncation\n"

@ MSG_SYS_ACCESS	"%s: %s: access failed: %s\n"
@ MSG_SYS_OPEN		"%s: %s: open failed: %s\n"
@ MSG_SYS_MMAP		"%s: %s: mmap failed: %s\n"
@ MSG_SYS_TRUNC		"%s: %s: ftruncate failed: %s\n"
@ MSG_SYS_STAT		"%s: %s: stat failed: %s\n"
@ MSG_SYS_WRITE		"%s: %s: write failed: %s\n"

@ MSG_SYS_EXEC		"%s: %s: exec failed: due to signal %d %s\n"
@ MSG_SYS_EXECLP	"%s: %s: exec failed: %s\n"
@ MSG_SYS_CORE		"(core dumped)"


# Elf error messages

@ MSG_ELF_TYPE		"%s: %s: invalid ELF file\n"
@ MSG_ELF_BEGIN		"%s: %s: elf_begin: %s\n"


# Generic error messages

@ MSG_GEN_INVFILE	"%s: %s: invalid file type\n"


# Diagnostic strings

@ MSG_DIA_CONFILE	"output configuration file: %s\n"
@ MSG_DIA_OBJDIR	"output object directory: %s\n"
@ MSG_DIA_DLIBPTH	" adding default library path (%s): %s\n"
@ MSG_DIA_SLIBPTH	" adding secure library path (%s): %s\n"
@ MSG_DIA_NOEXIST	" adding directory [id=%d]: %s  (non-existent path)\n"
@ MSG_DIA_RDIR		" adding directory [id=%d]: %s\n",
@ MSG_DIA_ADIR		" adding directory [id=%d]: %s  (alias)\n"
@ MSG_DIA_RFILE		"  adding file [id=%d]: %s\n"
@ MSG_DIA_AFILE		"  adding file [id=%d]: %s  (alias)\n"
@ MSG_DIA_ALTERNATE	"  adding alternate [id=%d]: %s\n"

# Dump strings

@ MSG_DMP_HEAD		"\nConfiguration file: %s  %s\n"
@ MSG_DMP_APP		"  Specific application: %s  (original: %s)\n" 
@ MSG_DMP_DLIBPTH	"  Default Library Path (%s):\t%s\n"
@ MSG_DMP_SLIBPTH	"  Secure Library Path (%s):\t%s\n"
@ MSG_DMP_RESV		"\nMemory Reservation:\t%llx - %llx (%llx bytes)\n"
@ MSG_DMP_DIR_1		"\nDirectory: %s\n"
@ MSG_DMP_DIR_2		"\nDirectory: %s\t(non-existent)\n"
@ MSG_DMP_FILE_1	"  %s\n"
@ MSG_DMP_FILE_2	"  %s\t(alternate: %s)\n"
@ MSG_DMP_HASH		"Object Entries:\n   Id\tBkt\tString\n"
@ MSG_DMP_HASHENT_1	"    %d\t[%d]\t%s\t%s\n"
@ MSG_DMP_HASHENT_2	"    %d\t\t%s\t%s\n"

@ MSG_DMP_STAT		"\nWARNING: original file %s: stat failed: %s\n\n"
@ MSG_DMP_DCMP		"\nWARNING: original directory %s: modified since \
			 configuration file creation\n\n"
@ MSG_DMP_FCMP		"\nWARNING: original file %s: modified since \
			 configuration file creation\n\n"

@ _END_


# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_ARG_OPTIONS	"6:a:c:f:G:g:I:i:l:o:s:t:v"
@ MSG_ARG_C		"-c"
@ MSG_ARG_F		"-f"
@ MSG_ARG_O		"-o"

@ MSG_STR_ELF		"ELF"
@ MSG_STR_AOUT		"AOUT"
@ MSG_STR_EMPTY		""
@ MSG_STR_READ		"r"
@ MSG_STR_NL		"\n"

@ MSG_AUD_PRF		"__CRLE_DEP__:"
@ MSG_AUD_RESBGN	"resbgn:"
@ MSG_AUD_RESEND	"resend:"

@ MSG_ENV_AUD_FD	"CRLE_FD=%d"
@ MSG_ENV_AUD_FLAGS	"CRLE_FLAGS=%d"

@ MSG_ENV_LD_AUDIT	"LD_AUDIT=libcrle.so.1"
@ MSG_ENV_LD_PRELOAD	"LD_PRELOAD=%s"
@ MSG_ENV_LD_CONFIG	"LD_CONFIG=%s"
@ MSG_ENV_LD_FLAGS	"LD_FLAGS=confgen"

@ MSG_LDFLG_LOADAVAIL	",loadavail"

@ MSG_PTH_CONFIG	"/var/ld/ld.config"
@ MSG_PTH_CONFIG_64	"/var/ld/64/ld.config"
@ MSG_PTH_DOT		"./"
@ MSG_PTH_LDDSTUB	"/usr/lib/lddstub"
@ MSG_PTH_LDDSTUB_64	"/usr/lib/64/lddstub"

@ MSG_DIR_ROOT		"/"
@ MSG_DIR_DOT		"."

@ MSG_TMP_DIR		"/var/tmp"
@ MSG_TMP_PFX		"crle."

@ MSG_FMT_PATH		"%s/%s"
@ MSG_FMT_COLON		":%s"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"


@ MSG_MOD_SEPARATE	" \t|"
@ MSG_MOD_REL_RELATIVE	"RTLD_REL_RELATIVE"
@ MSG_MOD_REL_EXEC	"RTLD_REL_EXEC"
@ MSG_MOD_REL_DEPENDS	"RTLD_REL_DEPENDS"
@ MSG_MOD_REL_PRELOAD	"RTLD_REL_PRELOAD"
@ MSG_MOD_REL_SELF	"RTLD_REL_SELF"
@ MSG_MOD_REL_WEAK	"RTLD_REL_WEAK"
@ MSG_MOD_REL_ALL	"RTLD_REL_ALL"
@ MSG_MOD_REL_MEMORY	"RTLD_MEMORY"
@ MSG_MOD_REL_STRIP	"RTLD_STRIP"
@ MSG_MOD_REL_NOHEAP	"RTLD_NOHEAP"
@ MSG_MOD_REL_CONFGEN	"RTLD_CONFGEN"
