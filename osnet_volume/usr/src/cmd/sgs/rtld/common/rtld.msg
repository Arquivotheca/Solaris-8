#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)rtld.msg	1.59	99/10/12 SMI"

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1)

@ MSG_ID_RTLD

# Message formatting errors

@ MSG_FMT_LONG		"ld.so.1: msgbuf overflow.  error format string: \
			 `%s'"

# Argument processing errors

@ MSG_ARG_ILLMODE	"illegal mode: %d"
@ MSG_ARG_ILLPATH	"illegal pathname"
@ MSG_ARG_ILLSYM	"illegal symbol name"
@ MSG_ARG_INVADDR	"address 0x%llx does not fall within any mapped object"
@ MSG_ARG_INVHNDL	"invalid handle"
@ MSG_ARG_ILLVAL	"illegal request value"
@ MSG_ARG_NOCONFIG	"no configuration file in use"
@ MSG_ARG_ATEXIT	"purge of atexit() registrations failed: %d"


# General error diagnostics

@ MSG_GEN_NULLFILE	"attempt to open file with null name"
@ MSG_GEN_NOFILE	"%s: can't find file"
@ MSG_GEN_ALTER		"%s: alternate file in use"
@ MSG_GEN_UNKNFILE	"%s: unknown file type"
@ MSG_GEN_CORTRUNC	"%s: corrupt or truncated file"
@ MSG_GEN_NOSYM		"%s: can't find symbol"
@ MSG_GEN_BADTYPE	"%s: bad ELF type: %s"
@ MSG_GEN_INVPRGHDR	"%s: invalid ELF program header, segments out of order"
@ MSG_GEN_NOLOADSEG	"%s: no loadable ELF segments found"
@ MSG_GEN_MAPINUSE	"%s: required mapping 0x%llx size 0x%llx, \
			 is already in use by file %s"
@ MSG_GEN_NOOPEN	"%s: DF_1_NOOPEN tagged object may not be dlopen()'ed"
@ MSG_GEN_NODUMP	"%s: DF_1_NODUMP tagged object may not be dldump()'ed"
@ MSG_GEN_AUDITERM	"%s: auditing terminated search"


# Move related messages

@ MSG_MOVE_ERR1		"move entry with illegal size; ignored"


# Relocation processing messages (some of these are required to satisfy
# do_reloc(), which is common code used by cmd/sgs/libld - make sure both
# message files remain consistent).

@ MSG_REL_NOSYM		"relocation error: file %s: symbol %s: \
			 referenced symbol not found"
@ MSG_REL_PLTENTRY	"relocation error: %s: unable to process .plt[%lld]: \
			 called from 0x%llx: %s"
@ MSG_REL_PLTREF	"unidentifiable procedure reference"

# System call messages.

@ MSG_SYS_OPEN		"%s: open failed: %s"
@ MSG_SYS_MMAP		"%s: mmap failed: %s"
@ MSG_SYS_FSTAT		"%s: fstat failed: %s"
@ MSG_SYS_MPROT		"%s: mprotect failed: %s"
@ MSG_SYS_PROC		"%s: /proc error: %s"


# Configuration failures

@ MSG_CONF_APP		"configuration file: %s: is specific to application: %s"
@ MSG_CONF_STAT		"configuration file: %s: original file %s: stat \
			 failed: %s"
@ MSG_CONF_FCMP		"configuration file: %s: original file %s: modified \
			 since configuration file creation"

# Link Audit diagnostic message formats

@ MSG_AUD_BADVERS	"version mismatch: current %d: required %d"
@ MSG_AUD_DISABLED	"%s: audit initialization failure: disabled"
@ MSG_AUD_PROFAUDINC	"LD_AUDIT and LD_PROFILE are mutually exclusive"


# Versioning diagnostics.

@ MSG_VER_NFOUND	"%s: version `%s' not found (required by file %s)"


# Diagnostics generated under the control of ldd(1).

@ MSG_LDD_VER_FIND	"   find version=%s\n"
@ MSG_LDD_VER_NFOUND	"\t%s (%s) =>\t (version not found)\n"

@ MSG_LDD_SYM_NFOUND	"\tsymbol not found: %s\t\t(%s)\n"

@ MSG_LDD_PTH_TRYING	"    trying path=%s\n"
@ MSG_LDD_PTH_ENVDIR	"    search path=%s  (LD_LIBRARY_PATH)\n"
@ MSG_LDD_PTH_RPATH	"    search path=%s  (RPATH from file %s)\n"
@ MSG_LDD_PTH_BGNDFL	"    search path="
@ MSG_LDD_PTH_ENDDFL	"  (default)\n"
@ MSG_LDD_PTH_ENDDFLC	"  (configuration default - %s)\n"
@ MSG_LDD_PTH_IGNORE	"    ignore path=%s  (insecure directory name)\n"

@ MSG_LDD_FIL_FIND	"\n   find library=%s; required by %s\n"
@ MSG_LDD_FIL_NFOUND	"\t%s =>\t (file not found)\n"
@ MSG_LDD_FIL_ILLEGAL	"\t%s =>\t (illegal insecure filename)\n"
@ MSG_LDD_FIL_ALTER	"  (alternate)"

@ MSG_LDD_CPY_SIZDIF	"\tcopy relocation sizes differ: %s\n\
			 \t\t(file %s size=0x%llx; file %s size=0x%llx)\n"
@ MSG_LDD_CPY_INSDATA	"\t\t%s size used; possible insufficient data copied\n"
@ MSG_LDD_CPY_DATRUNC	"\t\t%s size used; possible data truncation\n"

@ MSG_LDD_INIT_FMT_01	"   init library=%s\n"
@ MSG_LDD_INIT_FMT_02	"   init library=%s (cyclic dependency on"

# Error diagnostic standard prefixes.

@ MSG_ERR_WARNING	"warning: "
@ MSG_ERR_FATAL		"fatal: "
@ MSG_ERR_ELF		"elf error: "

@ MSG_STR_UNKNOWN	"(unknown)"
@ MSG_STR_NULL		"(null)"


@ _END_


@ MSG_LDD_FIL_PATH	"\t%s%s\n"
@ MSG_LDD_FIL_EQUIV	"\t%s =>\t %s%s\n"
@ MSG_LDD_FMT_PATH1	"%s"
@ MSG_LDD_FMT_PATHN	":%s"
@ MSG_LDD_FMT_FILE	" %s"
@ MSG_LDD_FMT_END	")\n"
@ MSG_LDD_VER_FOUND	"\t%s (%s) =>\t %s\n"



# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_NEGATE	"-"
@ MSG_STR_ZERO		"0"
@ MSG_STR_HEX		"0x"
@ MSG_STR_ELF		"ELF"
@ MSG_STR_EMSGFOR1	"%s: %s: %s"
@ MSG_STR_EMSGFOR2	"%s: %s"
@ MSG_STR_HEXNUM	"0123456789abcdef"
@ MSG_STR_NL		"\n"
@ MSG_STR_SLASH		"/"
@ MSG_STR_DELIMIT	": "

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
@ MSG_SUNW_OST_OSLIB	"SUNW_OST_OSLIB"

@ MSG_TKN_PLATFORM	"PLATFORM"
@ MSG_TKN_ORIGIN	"ORIGIN"
@ MSG_TKN_ISALIST	"ISALIST"
@ MSG_TKN_OSNAME	"OSNAME"
@ MSG_TKN_OSREL		"OSREL"
@ MSG_TKN_BINDINGS	"bindings"
@ MSG_TKN_POSIX		"POSIX"

@ MSG_FMT_PATH		"%s/%s"
@ MSG_FMT_CWD		"."
@ MSG_FMT_PARENT	"/.."
@ MSG_FMT_PROC		"/proc/%d"
@ MSG_FMT_MSGFILE	"/usr/lib/locale/%s/LC_MESSAGES/%s.mo"

@ MSG_FIL_RTLD		"ld.so.1"

@ MSG_SYM_ELFERRMSG	"elf_errmsg"
@ MSG_SYM_ELFERRNO	"elf_errno"
@ MSG_SYM_ELFPLTTRACE	"elf_plt_trace"
@ MSG_SYM_ENVIRON	"_environ"

@ MSG_FIL_LDPROF	"/usr/lib/link_audit/ldprof.so.1"
@ MSG_SYM_LAPREINIT	"la_preinit"
@ MSG_SYM_LAVERSION	"la_version"
@ MSG_SYM_LAACTIVITY	"la_activity"
@ MSG_SYM_LAOBJSEARCH	"la_objsearch"
@ MSG_SYM_LAOBJOPEN	"la_objopen"
@ MSG_SYM_LAOBJCLOSE	"la_objclose"
@ MSG_SYM_LADYNDATA	"la_dyndata"
@ MSG_SYM_PROFOPEN	"profile_open"
@ MSG_SYM_PROFCGINTRP	"plt_cg_interp"

@ MSG_SPECFIL_DYNPLT	"dyn_plt(ld.so.1)"

@ MSG_PTH_LIBSYS	"/usr/lib/libsys.so.1"
@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_USRLIB	"/usr/lib"
@ MSG_PTH_USRLIBSE	"/usr/lib/secure"

@ MSG_PTH_DEVNULL	"/dev/null"
@ MSG_PTH_DEVZERO	"/dev/zero"
@ MSG_PTH_CONFIG	"/var/ld/ld.config"
@ MSG_PTH_CONFIG_64	"/var/ld/64/ld.config"

@ MSG_ORG_CONFIG	"$ORIGIN/ld.config.%s"

@ MSG_LD_LIBPATH	"LIBRARY_PATH="
@ MSG_LD_LOADFLTR	"LOADFLTR="
@ MSG_LD_PRELOAD	"PRELOAD="
@ MSG_LD_TRACEOBJS	"TRACE_LOADED_OBJECTS"
@ MSG_LD_TRACE_E	"_E="
@ MSG_LD_TRACE_A	"_A="
@ MSG_LD_TRACEPTHS	"TRACE_SEARCH_PATHS="
@ MSG_LD_VERBOSE	"VERBOSE="
@ MSG_LD_WARN		"WARN="
@ MSG_LD_INIT		"INIT="
@ MSG_LD_BINDINGS	"BINDINGS="
@ MSG_LD_BIND_NOW	"BIND_NOW="
@ MSG_LD_BIND_NOT	"BIND_NOT="
@ MSG_LD_NOAUXFLTR	"NOAUXFLTR="
@ MSG_LD_NOVERSION	"NOVERSION="
@ MSG_LD_CONFIG		"CONFIG="
@ MSG_LD_NOCONFIG	"NOCONFIG="
@ MSG_LD_NODIRCONFIG	"NODIRCONFIG="
@ MSG_LD_NOOBJALTER	"NOOBJALTER="
@ MSG_LD_PROFILE	"PROFILE"
@ MSG_LD_DEBUG		"DEBUG"
@ MSG_LD_OUTPUT		"_OUTPUT="
@ MSG_LD_AUDIT		"AUDIT="
@ MSG_LD_AUDIT_64	"AUDIT_64="
@ MSG_LD_AUDIT_ARGS	"AUDIT_ARGS="
@ MSG_LD_BREADTH	"BREADTH="
@ MSG_LD_ORIGIN		"ORIGIN="
@ MSG_LD_NODIRECT	"NODIRECT="
@ MSG_LD_FLAGS		"FLAGS="

@ MSG_LC_ALL		"ALL="
@ MSG_LC_MESSAGES	"MESSAGES="

@ MSG_LDFLG_NOLAZY	"nolazyload"
@ MSG_LDFLG_NODIRECT	"nodirect"
@ MSG_LDFLG_NOAUDIT	"noaudit"
@ MSG_LDFLG_CONFGEN	"confgen"
@ MSG_LDFLG_LOADAVAIL	"loadavail"
@ MSG_LDFLG_LOADFLTR	"loadfltr"

@ MSG_EMG_NOSPACE	"ld.so.1: internal: malloc failed"

@ MSG_DBG_FMT_DIAG	"%5.5d: "
@ MSG_DBG_FMT_FILE	"%s.%5.5d"
@ MSG_DBG_FMT_THREAD	"%5.5d: %d: "

@ MSG_AUD_PLTENTER	"la_pltenter"
@ MSG_AUD_SYMBIND	"la_symbind"
