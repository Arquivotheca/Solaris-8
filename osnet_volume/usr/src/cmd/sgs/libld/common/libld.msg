#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)libld.msg	1.64	99/10/22 SMI"

@ _START_

# Message file for cmd/sgs/libld.

@ MSG_ID_LIBLD

#
# TRANSLATION_NOTE -- Beginning of USAGE message
# 	The following messages are the usage messages for the ld command.
#	Tab characters (\t) are used to align the messages.
#
#	Each usage message starts with \t, and if the message has more than one
#		line, the following messages are aligned by 3 tab characters.
#		When you see \n\t\t\t, the first \n is used to change the line,
#		and following 3 tab characters are used to align the line.
#
#	Each usage message option is surrounded by [ and ].  Then the
#	description of the option follows. The descriptions should be aligned,
#	so tab characters are padded as needed after the closing bracket ].
#
#	How to align the messages are up to the translators and the
#	localization engineers.
#
#	In C locale, the first 3 messages would look like the following:
#
#	usage:
#	    ld [-abc:d:e:f:h:il:mo:p:rstu:z:B:D:F:GI:L:M:N:P:Q:R:S:VY] file(s)
#        	[-a]            create an absolute file
#        	[-b]            do not do special PIC relocations in a.out
#        	[-c file]       record configuration `file'
#
@ MSG_ARG_USAGE		"usage: ld [-%s] file(s)\n"
@ MSG_ARG_DETAIL_A	"\t[-a]\t\tcreate an absolute file\n"
@ MSG_ARG_DETAIL_B	"\t[-b]\t\tdo not do special PIC relocations in a.out\n"
@ MSG_ARG_DETAIL_C	"\t[-c file]\trecord configuration `file'\n"
@ MSG_ARG_DETAIL_D	"\t[-d y|n]\toperate in dynamic|static mode\n"
@ MSG_ARG_DETAIL_E	"\t[-e sym]\tuse `sym' as entry point address\n"
@ MSG_ARG_DETAIL_F	"\t[-f name]\tspecify library for which this file is \
			 an auxiliary\n\
			 \t\t\tfilter\n"
@ MSG_ARG_DETAIL_H	"\t[-h name]\tuse `name' as internal shared object \
			 identifier\n"
@ MSG_ARG_DETAIL_I	"\t[-i]\t\tignore LD_LIBRARY_PATH setting\n"
@ MSG_ARG_DETAIL_L	"\t[-l x]\t\tsearch for libx.so or libx.a\n"
@ MSG_ARG_DETAIL_M	"\t[-m]\t\tprint memory map\n"
@ MSG_ARG_DETAIL_O	"\t[-o outfile]\tname the output file `outfile'\n"
@ MSG_ARG_DETAIL_P	"\t[-p name]\tidentify audit object to accompany this \
			 object\n"
@ MSG_ARG_DETAIL_R	"\t[-r]\t\tcreate a relocatable object\n"
@ MSG_ARG_DETAIL_S	"\t[-s]\t\tstrip any symbol and debugging information\n"
@ MSG_ARG_DETAIL_T	"\t[-t]\t\tdo not warn of multiply defined symbols of \
			 different\n\t\t\tsizes\n"
@ MSG_ARG_DETAIL_U	"\t[-u sym]\tcreate an undefined symbol `sym'\n"
@ MSG_ARG_DETAIL_ZA	"\t[-z absexec]\twhen building an executable absolute \
			 symbols\n \
			 \t\t\treferenced in dynamic objects are promoted to\n \
			 \t\t\tthe executable.\n"
@ MSG_ARG_DETAIL_ZD	"\t[-z defs|nodefs]\n\
			 \t\t\tdisallow|allow undefined symbols\n"
@ MSG_ARG_DETAIL_ZE	"\t[-z endfiltee]\tmarks a filtee such that it will \
			 terminate a filters\n\t\t\tsearch\n"
@ MSG_ARG_DETAIL_ZIG	"\t[-z ignore|record]\n\
			 \t\t\tignore|record unused dynamic dependencies\n"
@ MSG_ARG_DETAIL_ZI	"\t[-z initfirst]\tmark object to indicate that its \
			 .init section should\n\
			 \t\t\tbe executed before the .init section of any \
			 other\n\t\t\tobjects\n"
@ MSG_ARG_DETAIL_ZLZ	"\t[-z lazyload|nolazyload]\n\
			 \t\t\tenable|disable delayed loading of \
			 shared objects\n"
@ MSG_ARG_DETAIL_ZDR	"\t[-z direct]\tspecify 'direct' bindings for \
			 executable when run\n"
@ MSG_ARG_DETAIL_ZGP	"\t[-z groupperm|nogroupperm]\n\
			 \t\t\tenable|disable setting of GROUP permissions\n\
			 \t\t\ton dynamic dependencies\n"
@ MSG_ARG_DETAIL_ZL	"\t[-z loadfltr]\tmark filter as requiring immediate \
			 loading of its\n\
			 \t\t\tfiltees at runtime\n"
@ MSG_ARG_DETAIL_ZIN	"\t[-z interpose]\
			 \tdynamic object is to be an `interposer' on\n\
			 \t\t\tdirect bindings\n"
@ MSG_ARG_DETAIL_ZM	"\t[-z muldefs]\tallow multiply defined symbols\n"
@ MSG_ARG_DETAIL_ZNDL	"\t[-z nodefaultlib]\n\
			 \t\t\tmark object to ignore any default library \
			 search path\n"
@ MSG_ARG_DETAIL_ZNDE	"\t[-z nodelete]\tmark object as non-deletable\n"
@ MSG_ARG_DETAIL_ZNDO	"\t[-z nodlopen]\tmark object as non-dlopen()'able\n"
@ MSG_ARG_DETAIL_ZNDU	"\t[-z nodump]\tmark object as non-dldump()'able\n"
@ MSG_ARG_DETAIL_ZNV	"\t[-z noversion]\tdon't record any version sections\n"
@ MSG_ARG_DETAIL_ZB	"\t[-z now]\tmark object as requiring non-lazy \
			 binding\n"
@ MSG_ARG_DETAIL_ZO	"\t[-z origin]\tmark object as requiring $ORIGIN \
			 processing\n"
@ MSG_ARG_DETAIL_ZT	"\t[-z text]\tdisallow output relocations against \
			 text\n"
@ MSG_ARG_DETAIL_ZTW	"\t[-z textwarn]\twarn if there are relocations \
			 against text\n"
@ MSG_ARG_DETAIL_ZTO	"\t[-z textoff]\tallow output relocations against \
			 text\n"
@ MSG_ARG_DETAIL_ZR	"\t[-z redlocsym]\treduce local syms in .symtab to \
			 a minimum\n"
@ MSG_ARG_DETAIL_ZW	"\t[-z weakextract]\n\
			 \t\t\tallow extraction of archive members to resolve \
			 weak\n\
			 \t\t\treferences\n"
@ MSG_ARG_DETAIL_ZAE	"\t[-z allextract]\textract all member files from \
			 archive files\n"
@ MSG_ARG_DETAIL_ZDE	"\t[-z defaultextract]\n\
			 \t\t\textract member files from archive files\n\
			 \t\t\twhich resolve undefined or tentative symbols\n"
@ MSG_ARG_DETAIL_ZC	"\t[-z combreloc]\tcombine multiple relocation \
			 sections\n"
@ MSG_ARG_DETAIL_CBD	"\t[-B dynamic|static]\n\
			 \t\t\tsearch for shared libraries|archives\n"
@ MSG_ARG_DETAIL_CBG	"\t[-B group]\trelocate object from within group\n"
@ MSG_ARG_DETAIL_CBL	"\t[-B local]\treduce unqualified global symbols to \
			 local\n"
@ MSG_ARG_DETAIL_CBE	"\t[-B eliminate]\teliminate unqualified global\n\
			 \t\t\tsymbols from the symbol table\n"
@ MSG_ARG_DETAIL_CBR	"\t[-B reduce]\tprocess symbol reductions\n"
@ MSG_ARG_DETAIL_CBS	"\t[-B symbolic]\tbind external references to \
			 definitions when creating\n\
			 \t\t\tshared objects\n"
@ MSG_ARG_DETAIL_CBDR	"\t[-B direct]\tspecify 'direct' bindings for \
			 executable when run\n"
@ MSG_ARG_DETAIL_CBT	"\t[-B translator]\tspecify that this object is to act \
			 as a 'translator'\n\
			 \t\t\tfor an application bound with -Bdirect\n"
@ MSG_ARG_DETAIL_CD	"\t[-D options]\tprint diagnostic messages\n"
@ MSG_ARG_DETAIL_CF	"\t[-F name]\tspecify library for which this file is \
			 a filter\n"
@ MSG_ARG_DETAIL_CG	"\t[-G]\t\tcreate a shared object\n"
@ MSG_ARG_DETAIL_CI	"\t[-I interp]\tuse `interp' as path name of \
			 interpreter\n"
@ MSG_ARG_DETAIL_CL	"\t[-L path]\tsearch for libraries in directory \
			 `path'\n"
@ MSG_ARG_DETAIL_CM	"\t[-M mapfile]\tuse processing directives contained \
			 in `mapfile'\n"
@ MSG_ARG_DETAIL_CN	"\t[-N file]\tcreate a dynamic dependency on `file'\n"
@ MSG_ARG_DETAIL_CP	"\t[-P name]\tidentify audit object for processing \
			 the dependencies\n\
			 \t\t\tof this object\n"
@ MSG_ARG_DETAIL_CQ	"\t[-Q y|n]\tdo|do not place version information in \
			 output file\n"
@ MSG_ARG_DETAIL_CR	"\t[-R path]\tspecify a library search path to be \
			 used at run time\n"
@ MSG_ARG_DETAIL_CS	"\t[-S name]\tspecify a link-edit support library\n"
@ MSG_ARG_DETAIL_CV	"\t[-V]\t\tprint version information\n"
@ MSG_ARG_DETAIL_CY	"\t[-Y P,dirlist]\tuse `dirlist' as a default path \
			 when searching for\n\
			 \t\t\tlibraries\n"
#
# TRANSLATION_NOTE -- End of USAGE message
#


# Relocation processing messages (some of these are required to satisfy
# do_reloc(), which is common code used by cmd/sgs/rtld - make sure both
# message files remain consistent).

@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx does not fit\n"
@ MSG_REL_NONALIGN	"relocation error: %s: file %s: symbol %s: \
			 offset 0x%llx is non-aligned"
@ MSG_REL_NULL		"relocation error: file %s: section %s: \
			 skipping null relocation record"
@ MSG_REL_NOTSUP	"relocation error: file %s: section %s: \
			 relocation %s: relocation not currently supported"
@ MSG_REL_UNKNWSYM	"relocation error: %s: file %s: section %s: \
			 attempt to relocate with respect to unknown \
			 symbol %s: offset 0x%llx, symbol index %d"
@ MSG_REL_INVALOFFSET	"relocation error: %s: file: %s section %s: \
			 invalid offset symbol `%s': offset 0x%llx"
@ MSG_REL_EMPTYSEC	"relocation error: %s: file %s: symbol %s: \
			 attempted against empty section %s"
@ MSG_REL_EXTERNSYM	"relocation error: %s: file %s: symbol %s:\n\
			 \texternal symbolic relocation against \
			 non-allocatable section %s;\n\
			 \tcannot be processed at runtime: relocation ignored"
@ MSG_REL_UNEXPREL	"relocation error: %s: file %s: symbol %s: \
			 unexpected relocation; generic processing performed"
@ MSG_REL_UNEXPSYM	"relocation error: %s: file %s: symbol %s: \
			 unexpected symbol referenced from file %s"
@ MSG_REL_SYMDISC	"relocation error: file: %s section: %s symbol: %s: \
			 relocation against a discarded symbol,\n\
			 \tsymbol is part of discarded section: %s"

#
# TRANSLATION_NOTE
# 	The following 7 messages are the message to print the
#	following example messages.
#
#Text relocation remains		      referenced
#    against symbol		    offset	in file
#str                                 0x14      	main.o
#printf                              0x1c      	main.o
#
#	The first two lines are the header, and the next msgid
#	is the format string for the header.
#	Tabs and spaces are used for alignment.
#	The first and third %s are for: "Text relocation remains against symbol"
#	The second %s and fourth %s are for: "referenced in file"
#	The third %s is for: "offset"
#
@ MSG_REL_REMAIN_FMT_1	"%-40s\t%s\n    %s\t\t    %s\t%s"
#
# TRANSLATION_NOTE
#	The next two msdid make a sentence. So translate:
#		"Text relocation remain against symbol"
#	And separate them into two msgstr considering the proper
#	alignment.
@ MSG_REL_RMN_ITM_11	"Text relocation remains"
@ MSG_REL_RMN_ITM_12	"against symbol"
@ MSG_REL_RMN_ITM_13	"warning: Text relocation remains"

@ MSG_REL_RMN_ITM_2	"offset"

#
# TRANSLATION_NOTE
#	The next two msdid make a sentence. So translate:
#		"referenced in file"
#	And separate them into two msgstr considering the proper
#	alignment.
@ MSG_REL_RMN_ITM_31	"referenced"
@ MSG_REL_RMN_ITM_32	"in file"
@ MSG_REL_REMAIN_2	"%-35s 0x%-8llx\t%s"
@ MSG_REL_REMAIN_3	"relocations remain against allocatable but \
			 non-writable sections"

# Files processing messages

@ MSG_FIL_MULINC_1	"file %s: attempted multiple inclusion of file"
@ MSG_FIL_MULINC_2	"file %s: linked to %s: attempted multiple inclusion \
			 of file"
@ MSG_FIL_SOINSTAT	"input of shared object `%s' in static mode"
@ MSG_FIL_INVALSEC	"file %s: section %s has invalid type %s"
@ MSG_FIL_NOTFOUND	"file %s: required by %s, not found"
@ MSG_FIL_MALSTR	"file %s: section %s: malformed string table, \
			 initial or final byte"
@ MSG_FIL_PTHTOLONG	"`%s/%s' pathname too long"
@ MSG_FIL_RECMATCH_1	"file %s: recording name `%s' matches that supplied \
			 with -h option"
@ MSG_FIL_RECMATCH_2	"file %s: recording name `%s' matches that provided \
			 by file %s"
@ MSG_FIL_EXCLUDE	"file %s: section %s contains both SHF_EXCLUDE and \
			 SHF_ALLOC flags: SHF_EXCLUDE ignored"
@ MSG_FIL_INVRELOC	"file %s: section %s: has invalid sh_info %d"
@ MSG_FIL_INVRELOC2	"file %s: section %s: relocations can not be\n\
			 \tapplied against section `%s'"
@ MSG_FIL_INTERRUPT	"file %s: creation interrupted: %s"


# System call messages

@ MSG_SYS_OPEN		"file %s: cannot open file: %s"
@ MSG_SYS_MMAP		"file %s: cannot mmap file: %s"


# ELF processing messages

@ MSG_ELF_LIBELF	"libelf: version not supported: %d"

@ MSG_ELF_UNKNOWN	"file %s: unknown type, unable to process \
			 using elf(3E) libraries"
@ MSG_ELF_ARMEM		"file %s: unable to locate archive member;\n\t\
			 offset=%x, symbol[%d]=%s"

@ MSG_ELF_ARSYM		"file %s ignored: unable to locate archive symbol table"

@ MSG_ELF_VERSYM	"file %s: version symbol section entry mismatch:\n\t\
			 (section %s entries=%d; section %s entires=%d)"
@ MSG_ELF_INVALBYTE	"file %s: wrong machine byte order"
@ MSG_ELF_INVALCLASS	"file %s: wrong machine class"
@ MSG_ELF_INVALTYPE	"file %s: wrong machine type"
@ MSG_ELF_VERSHIGH	"file %s: creators elf(3E) library version is higher \
			 than the version known to ld"


# Section processing errors

@ MSG_SCN_NONALLOC	"%s: non-allocatable section `%s' directed to a \
			 loadable segment"

# Symbol processing errors

@ MSG_SYM_NOSECDEF	"symbol `%s' in file %s has no section definition"
@ MSG_SYM_MULDEF	"symbol `%s' is multiply defined:"
@ MSG_SYM_DIFFTYPE	"symbol `%s' has differing types:"
@ MSG_SYM_DIFFATTR	"symbol `%s' has differing %s:\n\
			 \t(file %s value=0x%llx; file %s value=0x%llx);"
@ MSG_SYM_FILES		"\t(file %s and file %s);"
@ MSG_SYM_FILETYPES	"\t(file %s type=%s; file %s type=%s);"
@ MSG_SYM_DEFTAKEN	"\t%s definition taken"
@ MSG_SYM_DEFUPDATE	"\t%s definition taken and updated with larger size"
@ MSG_SYM_LARGER	"\tlargest value applied"
@ MSG_SYM_TENTERR	"\ttentative symbol cannot override defined symbol \
			 of smaller size"

@ MSG_SYM_INVSHNDX	"local symbol `%s' has invalid section index; \
			 ignored:\n\t(file %s value=%s);"
@ MSG_SYM_NONGLOB	"global symbol `%s' has non-global binding:\n\
			 \t(file %s value=%s);"
@ MSG_SYM_RESERVE	"reserved symbol `%s' already defined in file %s"
@ MSG_SYM_NOTNULL	"undefined symbol `%s' with non-zero value encountered \
			 from file %s"

@ MSG_PSYM_OVERLAP	"OVERLAPPING for %s from %s"
@ MSG_PSYM_FATAL1	"section symbol can not be expanded for static object"
@ MSG_PSYM_WARN1	"section symbol will not be expanded"
@ MSG_PSYM_WARN2	"relocation against illegal move entry. Ignored"
@ MSG_PSYM_WARN3	"move entry with illegal size detected. Ignored"
@ MSG_PSYM_FATAL2	"Internal Error: expand_move()."


#
# TRANSLATION_NOTE
# 	The following 7 messages are the message to print the
#	following example messages.
#
#Undefined			first referenced
# symbol  			    in file
#inquire                            halt_hold.o
#
@ MSG_SYM_FMT_UNDEF	"%s\t\t\t%s\
			 \n %s  \t\t\t    %s"

#
# TRANSLATION_NOTE
#	The next two msdid make a sentence. So translate:
#		"Undefined symbol"
#	And separate them into two msgstr considering the proper
#	alignment.
@ MSG_SYM_UNDEF_ITM_11	"Undefined"
@ MSG_SYM_UNDEF_ITM_12	"symbol"
#
# TRANSLATION_NOTE
#	The next two msdid make a sentence. So translate:
#		"first referenced in file"
#	And separate them into two msgstr considering the proper
#	alignment.
@ MSG_SYM_UNDEF_ITM_21	"first referenced"
@ MSG_SYM_UNDEF_ITM_22	"in file"
#

@ MSG_SYM_UND_UNDEF	"%-35s %s"
@ MSG_SYM_UND_NOVER	"%-35s %s  (symbol has no version assigned)"
@ MSG_SYM_UND_IMPL	"%-35s %s  (symbol belongs to implicit dependency %s)"
@ MSG_SYM_UND_NOTA	"%-35s %s  (symbol belongs to unavailable version %s \
			 (%s))"
@ MSG_SYM_ENTRY		"entry point symbol `%s' is undefined"


# Output file update messages

@ MSG_UPD_NOREADSEG	"No read-only segments found.  Setting `_etext' to 0"
@ MSG_UPD_NORDWRSEG	"No read-write segments found.  Setting `_edata' to 0"
@ MSG_UPD_NOSEG		"Setting `end' and `_end' to 0"

@ MSG_UPD_SEGOVERLAP	"%s: segment address overlap;\n\
			 \tprevious segment ending at address 0x%llx overlaps\n\
			 \tuser defined segment `%s' starting at address 0x%llx"
@ MSG_UPD_LARGSIZE	"%s: segment %s calculated size 0x%llx\n\
			 \tis larger than user-defined size 0x%llx"

@ MSG_UPD_LINKRANGE	"%s: %s: sh_link out of range %d, not translated"
@ MSG_UPD_NOBITS	"NOBITS section found before end of initialized data"


# Version processing messages

@ MSG_VER_HIGHER	"file %s: version revision %d is higher than \
			 expected %d"
@ MSG_VER_NOEXIST	"file %s: version `%s' does not exist:\n\
			 \trequired by file %s"
@ MSG_VER_UNDEF		"version `%s' undefined, referenced by version `%s':\n\
			 \trequired by file %s"
@ MSG_VER_UNAVAIL	"file %s: version `%s' is unavailable:\n\
			 \trequired by file %s"
@ MSG_VER_DEFINED	"version symbol `%s' already defined in file %s"
@ MSG_VER_INVALNDX	"version symbol `%s' from file %s has an invalid \
			 version index (%d)"
@ MSG_VER_ADDVERS	"unused $ADDVERS specification from file `%s' \
			 for object `%s'\nversion(s):"
@ MSG_VER_ADDVER	"\t%s"
@ MSG_VER_CYCLIC	"following versions generate cyclic dependency:"


@ MSG_MAP_SECORDER	"section ordering set but no matching section found;\
			 \n\tsegment: %s section: %s"

# Basic strings

@ MSG_STR_ALIGNMENTS	"alignments"
@ MSG_STR_COMMAND	"(command line)"
@ MSG_STR_SIZES		"sizes"
@ MSG_STR_UNKNOWN       "<unknown>"

@ _END_


@ MSG_ELF_BEGIN		"file %s: elf_begin"
@ MSG_ELF_GETARHDR	"file %s: elf_getarhdr"
@ MSG_ELF_GETDATA	"file %s: elf_getdata"
@ MSG_ELF_GETEHDR	"file %s: elf_getehdr"
@ MSG_ELF_GETPHDR	"file %s: elf_getphdr"
@ MSG_ELF_GETSCN	"file %s: elf_getscn"
@ MSG_ELF_GETSHDR	"file %s: elf_getshdr"
@ MSG_ELF_NEWDATA	"file %s: elf_newdata"
@ MSG_ELF_NEWEHDR	"file %s: elf_newehdr"
@ MSG_ELF_NEWSCN	"file %s: elf_newscn"
@ MSG_ELF_NEWPHDR	"file %s: elf_newphdr"
@ MSG_ELF_STRPTR	"file %s: elf_strptr"
@ MSG_ELF_UPDATE	"file %s: elf_update"
@ MSG_ELF_FSIZE		"file %s: elf_fsize"


# The following strings represent reserved names.  Reference to these strings
# is via the MSG_ORIG() macro, and thus translations are not required.

@ MSG_STR_EMPTY		""
@ MSG_STR_COLON		":"
@ MSG_STR_DOT		"."
@ MSG_STR_SLASH		"/"
@ MSG_STR_DYNAMIC	"(.dynamic)"
@ MSG_STR_ORIGIN	"$ORIGIN"
@ MSG_STR_PLATFORM	"$PLATFORM"
@ MSG_STR_ISALIST	"$ISALIST"
@ MSG_STR_OSNAME	"$OSNAME"
@ MSG_STR_OSREL		"$OSREL"

@ MSG_FMT_ARMEM		"%s(%s)"
@ MSG_FMT_COLPATH	"%s:%s"

@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_RTLD_SPARCV9	"/usr/lib/sparcv9/ld.so.1"
@ MSG_PTH_RTLD_IA64	"/usr/lib/ia64/ld.so.1"
@ MSG_PTH_DEVZERO	"/dev/zero"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"

@ MSG_LD_NOVERSION	"LD_NOVERSION"


# Section strings

@ MSG_SCN_BSS		".bss"
@ MSG_SCN_COMMENT	".comment"
@ MSG_SCN_DEBUG		".debug"
@ MSG_SCN_DYNAMIC	".dynamic"
@ MSG_SCN_DYNSTR	".dynstr"
@ MSG_SCN_DYNSYM	".dynsym"
@ MSG_SCN_EXCL		".excl"
@ MSG_SCN_GOT		".got"
@ MSG_SCN_HASH		".hash"
@ MSG_SCN_INDEX		".index"
@ MSG_SCN_INTERP	".interp"
@ MSG_SCN_LINE		".line"
@ MSG_SCN_PLT		".plt"
@ MSG_SCN_SHSTRTAB	".shstrtab"
@ MSG_SCN_STAB		".stab"
@ MSG_SCN_STABEXCL	".stab.exclstr"
@ MSG_SCN_STRTAB	".strtab"
@ MSG_SCN_SUNWDATA1	".sunwdata1"
@ MSG_SCN_SYMTAB	".symtab"
@ MSG_SCN_SUNWBSS	".SUNW_bss"
@ MSG_SCN_SUNWMOVE	".SUNW_move"
@ MSG_SCN_VERSION	".SUNW_version"
@ MSG_SCN_RELOC		".SUNW_reloc"
@ MSG_SCN_REL		".rel"
@ MSG_SCN_RELA		".rela"

# Entry criteria strings (segment names)

@ MSG_ENT_PHDR		"phdr"
@ MSG_ENT_INTERP	"interp"
@ MSG_ENT_TEXT		"text"
@ MSG_ENT_DATA		"data"
@ MSG_ENT_DYNAMIC	"dynamic"
@ MSG_ENT_NOTE		"note"
@ MSG_ENT_SUNW_BSS	"SUNW_bss"
@ MSG_ENT_IA64_UNWIND	"IA64.unwind"

# Symbol names

@ MSG_SYM_START		"_start"
@ MSG_SYM_MAIN		"main"

@ MSG_SYM_FINI_U	"_fini"
@ MSG_SYM_INIT_U	"_init"
@ MSG_SYM_DYNAMIC	"DYNAMIC"
@ MSG_SYM_DYNAMIC_U	"_DYNAMIC"
@ MSG_SYM_EDATA		"edata"
@ MSG_SYM_EDATA_U	"_edata"
@ MSG_SYM_END		"end"
@ MSG_SYM_END_U		"_end"
@ MSG_SYM_ETEXT		"etext"
@ MSG_SYM_ETEXT_U	"_etext"
@ MSG_SYM_GOFTBL	"GLOBAL_OFFSET_TABLE_"
@ MSG_SYM_GOFTBL_U	"_GLOBAL_OFFSET_TABLE_"
@ MSG_SYM_PLKTBL	"PROCEDURE_LINKAGE_TABLE_"
@ MSG_SYM_PLKTBL_U	"_PROCEDURE_LINKAGE_TABLE_"

@ MSG_SYM_L_END		"END_"
@ MSG_SYM_L_END_U	"_END_"
@ MSG_SYM_L_START	"START_"
@ MSG_SYM_L_START_U	"_START_"


# Support functions

@ MSG_SUP_START_64	"ld_start64"
@ MSG_SUP_ATEXIT_64	"ld_atexit64"
@ MSG_SUP_FILE_64	"ld_file64"
@ MSG_SUP_SECTION_64	"ld_section64"
@ MSG_SUP_START		"ld_start"
@ MSG_SUP_ATEXIT	"ld_atexit"
@ MSG_SUP_FILE		"ld_file"
@ MSG_SUP_SECTION	"ld_section"



# 
# Message previously in 'ld'
#
# 
@ _START_

# System error messages

@ MSG_SYS_STAT		"file %s: stat failed: %s"
@ MSG_SYS_READ		"file %s: read failed: %s"
@ MSG_SYS_NOTREG	"file %s: is not a regular file"

# Argument processing messages

@ MSG_ARG_DYNINCOMP	"option %s and building \
			 a dynamic executable are incompatible"
@ MSG_ARG_INCOMP	"option %s and %s are incompatible"
@ MSG_ARG_MTONCE	"option %s appears more than once, first setting taken"
@ MSG_ARG_ILLEGAL	"option %s has illegal argument `%s'"
@ MSG_ARG_YP		"option -YP and -Y%c may not be specified concurrently"
@ MSG_ARG_STRIP		"option -r and -s both set; only debugging \
			 information stripped"
@ MSG_ARG_NOFILES	"no files on input command line"
@ MSG_ARG_NOFLTR	"option %s is only meaningful when building a filter"
@ MSG_ARG_NODEFLIB	"the default library search path has been suppressed, \
			 but no runpaths have been specified (see -R option)"

@ MSG_ARG_FLAGS		"Flags processing errors"
@ MSG_ARG_FILES		"File processing errors. No output written to %s"
@ MSG_ARG_SYM_WARN	"Symbol referencing errors"
@ MSG_ARG_SYM_FATAL	"Symbol referencing errors. No output written to %s"

# Entrance criteria messages

@ MSG_ENT_MAP_FMT_TIL_1	"\t\t%s\n\n"
@ MSG_ENT_MAP_TITLE_1	"LINK EDITOR MEMORY MAP"

#
# TRANSLATION_NOTE -- Entry map header
#
# The next message is a format string for a title. The title is composed of
# two lines. In C locale, it would look like:
#
# 	output		input		new
#	section		section		displacement	size
#
# The \t characters are used for alignment.  (output section), (input section),
# and (new displacement) have to be aligned.
#
@ MSG_ENT_MAP_FMT_TIL_2	"\n%s\t\t%s\t\t%s\n%s\t\t%s\t\t%s\t%s\n\n"
@ MSG_ENT_MAP_FMT_TIL_3	"\n%s\t\t%s\t\t%s\n%s\t\t%s\t\t%s\t\t%s\n\n"
@ MSG_ENT_ITM_OUTPUT	"output"
@ MSG_ENT_ITM_INPUT	"input"
@ MSG_ENT_ITM_NEW	"new"
@ MSG_ENT_ITM_SECTION	"section"
@ MSG_ENT_ITM_DISPMNT	"displacement"
@ MSG_ENT_ITM_SIZE	"size"
@ MSG_ENT_ITM_VIRTUAL	"virtual"
@ MSG_ENT_ITM_ADDRESS	"address"

@ MSG_ENT_MAP_ENTRY_1	"%-8.8s\t\t\t%08.2llx\t%08.2llx\n"
@ MSG_ENT_MAP_ENTRY_2	"\t\t%-8.8s\t%08.2llx\t%08.2llx %s\n"

#
# TRANSLATION_NOTE -- multiple defined symbol table header
#
# In C locale, an example output is:
#
#		MULTIPLY DEFINED SYMBOLS
#
#
#symbol				  definition used     also defined in
#
#variable1                           main.o
#							./libfred.so
@ MSG_ENT_MUL_FMT_TIL_0	"\n\n\t\t%s\n\n\n"
@ MSG_ENT_MUL_TIL_0	"MULTIPLY DEFINED SYMBOLS"

#
# TRANSLATION_NOTE -- This is the format string for:
#
#symbol				  definition used     also defined in
#
@ MSG_ENT_MUL_FMT_TIL_1	"%s\t\t\t\t  %s     %s\n\n"
@ MSG_ENT_MUL_ITM_SYM	"symbol"
@ MSG_ENT_MUL_ITM_DEF_0	"definition used"
@ MSG_ENT_MUL_ITM_DEF_1	"also defined in"

#
# TRANSLATION_NOTE -- This is the format string for the second item:
#
@ MSG_ENT_MUL_ENTRY_1	"%-35s %s\n"

#
# TRANSLATION_NOTE -- This is the format string for the third item:
#
@ MSG_ENT_MUL_ENTRY_2	"\t\t\t\t\t\t\t%s\n"

@ MSG_ENT_NOSEC_1	"mapfile: %s segment: section `%s' does not appear \
			 in file `%s'"
@ MSG_ENT_NOSEC_2	"mapfile: %s segment: section `%s' does not appear \
			 in any input file"

# Library messages

@ MSG_LIB_TOOLONG_1	"%s/lib%s: pathname too long"
@ MSG_LIB_TOOLONG_2	"lib%s: library name too long"

@ MSG_LIB_NOTFOUND	"library -l%s: not found"
@ MSG_LIB_MALFORM	"LD_LIBRARY_PATH malformed"
@ MSG_LIB_BADYP		"-YP library path malformed"


# Mapfile processing messages

@ MSG_MAP_BADFLAG	"%s: %lld: badly formed section flags `%s'"
@ MSG_MAP_REDEFATT	"%s: %lld: redefining %s attribute for `%s'"
@ MSG_MAP_PREMEOF	"%s: %lld: premature EOF"
@ MSG_MAP_ILLCHAR	"%s: %lld: illegal character `\\%03o'"
@ MSG_MAP_MALFORM	"%s: %lld: malformed entry"
@ MSG_MAP_NONLOAD	"%s: %lld: %s not allowed on non-LOAD segments"
@ MSG_MAP_MOREONCE	"%s: %lld: %s set more than once on same line"
@ MSG_MAP_NOTERM	"%s: %lld: string does not terminate with a `\"'"
@ MSG_MAP_SECINSEG	"%s: %lld: section within segment ordering done on \
			 a non-existent segment `%s'"
@ MSG_MAP_INCOMFLG	"%s: %lld: incompatible flags $SPECVERS and $ADDVERS \
			 used on the same object `%s'"
@ MSG_MAP_MULSONAME	"%s: %lld: multiple definition of SONAME field for %s\
			 \n\tfirst `%s', second `%s'; first value used"
@ MSG_MAP_UNEXDEP	"%s: %lld: dependency `%s' unexpected; ignored"
@ MSG_MAP_UNEXTOK	"%s: %lld: unexpected occurrence of `%c' token"

@ MSG_MAP_SEGEMPEXE	"%s: %lld: empty segment definition for this output \
			 file is unsupported"
@ MSG_MAP_SEGEMPLOAD	"%s: %lld: empty segment must be of type LOAD"
@ MSG_MAP_SEGEMPATT	"%s: %lld: empty segment must have an address and size"
@ MSG_MAP_SEGEMPSEC	"%s: %lld: empty segment can not have sections \
			 assigned to it"
@ MSG_MAP_SEGEMNOPERM	"%s: %lld: empty segment must not have p_flags set: 0x%x"

@ MSG_MAP_UNKSYMSCO	"%s: %lld: unknown symbol scope definition `%s'"
@ MSG_MAP_UNKSYMDEF	"%s: %lld: unknown symbol definition `%s'"
@ MSG_MAP_UNKSEGTYP	"%s: %lld: unknown internal segment type %d"
@ MSG_MAP_UNKSOTYP	"%s: %lld: unknown shared object type `%s'"
@ MSG_MAP_UNKSEGATT	"%s: %lld: unknown segment attribute `%s'"
@ MSG_MAP_UNKSEGFLG	"%s: %lld: unknown segment flag `?%c'"
@ MSG_MAP_UNKSECTYP	"%s: %lld: unknown section type `%s'"

@ MSG_MAP_SEGSIZE	"%s: %lld: segment `%s' already has a size symbol"
@ MSG_MAP_SEGADDR	"%s: %lld: segment address or length `%s' %s"
@ MSG_MAP_SEGRESV	"%s: %lld: segments `interp' or `dynamic' may not be \
			 changed"

@ MSG_MAP_SYMDEF	"%s: %lld: symbol `%s' is already defined in file: %s"

@ MSG_MAP_EXPSCOL	"%s: %lld: expected a `;'"
@ MSG_MAP_EXPEQU	"%s: %lld: expected a `=', `:', `|', or `@'"
@ MSG_MAP_EXPSEGATT	"%s: %lld: expected one or more segment attributes \
			 after an `='"
@ MSG_MAP_EXPSEGNAM	"%s: %lld: expected a segment name at the beginning \
			 of a line"
@ MSG_MAP_EXPSYM_1	"%s: %lld: expected a symbol name after `@'"
@ MSG_MAP_EXPSYM_2	"%s: %lld: expected a symbol name after `{'"
@ MSG_MAP_EXPSEC	"%s: %lld: expected a section name after `|'"
@ MSG_MAP_EXPSO		"%s: %lld: expected a shared object definition after `-'"
@ MSG_MAP_EXPVERS	"%s: %lld: expected a `;' or version reference after `}'"

@ MSG_MAP_SEGSAME	"segments `%s' and `%s' have the same assigned \
			 virtual address"
@ MSG_MAP_EXCLIMIT	"exceeds internal limit"
@ MSG_MAP_NOBADFRM	"number is badly formed"

@ MSG_MAP_SEGTYP	"segment type"
@ MSG_MAP_SEGVADDR	"segment virtual address"
@ MSG_MAP_SEGPHYS	"segment physical address"
@ MSG_MAP_SEGLEN	"segment length"
@ MSG_MAP_SEGFLAG	"segment flags"
@ MSG_MAP_SEGALIGN	"segment alignment"
@ MSG_MAP_SEGROUND	"segment rounding"

@ MSG_MAP_SECTYP	"section type"
@ MSG_MAP_SECFLAG	"section flags"
@ MSG_MAP_SECNAME	"section name"

@ MSG_MAP_SYMVAL	"symbol value"
@ MSG_MAP_SYMSIZE	"symbol size"


# Debugging messages

@ MSG_DBG_NOINABLE	"debugging is not enabled; recompile with -DDEBUG"

# Generic error diagnostic labels

@ MSG_STR_NULL		"(null)"

@ _END_


# Software identification

@ MSG_SGS_ID		"ld: Software Generation Utilities - Solaris-ELF (4.0)"

# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_PATHTOK	";:"
@ MSG_STR_AOUT		"a.out"

@ MSG_STR_SUF_A		".a"
@ MSG_STR_SUF_SO	".so"
@ MSG_STR_PRE_LIB	"/lib"
@ MSG_STR_PATH		"%s/%s"
@ MSG_STR_STRNL		"%s\n"

@ MSG_STR_INTERP	"interp"
@ MSG_STR_LD_DYNAMIC	"dynamic"
@ MSG_STR_SYMBOLIC	"symbolic"
@ MSG_STR_ELIMINATE	"eliminate"
@ MSG_STR_LOCAL		"local"
@ MSG_STR_PROGBITS	"progbits"
@ MSG_STR_SYMTAB	"symtab"
@ MSG_STR_DYNSYM	"dynsym"
@ MSG_STR_REL		"rel"
@ MSG_STR_RELA		"rela"
@ MSG_STR_STRTAB	"strtab"
@ MSG_STR_HASH		"hash"
@ MSG_STR_LIB		"lib"
@ MSG_STR_NOTE		"note"
@ MSG_STR_NOBITS	"nobits"

@ MSG_STR_OPTIONS	"abc:d:e:f:h:il:mo:p:rstu:z:B:D:F:GI:L:M:N:P:Q:R:S:VY:"


# Argument processing strings

@ MSG_ARG_A		"-a"
@ MSG_ARG_B		"-b"
@ MSG_ARG_CB		"-B"
@ MSG_ARG_BDYNAMIC	"-Bdynamic"
@ MSG_ARG_BSYMBOLIC	"-Bsymbolic"
@ MSG_ARG_BTRANS	"-Btrans"
@ MSG_ARG_BGROUP	"-Bgroup"
@ MSG_ARG_BELIMINATE	"-Beliminate"
@ MSG_ARG_BLOCAL	"-Blocal"
@ MSG_ARG_C		"-c"
@ MSG_ARG_D		"-d"
@ MSG_ARG_E		"-e"
@ MSG_ARG_DN		"-dn"
@ MSG_ARG_DY		"-dy"
@ MSG_ARG_F		"-f"
@ MSG_ARG_CF		"-F"
@ MSG_ARG_CG		"-G"
@ MSG_ARG_H		"-h"
@ MSG_ARG_CI		"-I"
@ MSG_ARG_CN		"-N"
@ MSG_ARG_O		"-o"
@ MSG_ARG_P		"-p"
@ MSG_ARG_CP		"-P"
@ MSG_ARG_CQ		"-Q"
@ MSG_ARG_R		"-r"
@ MSG_ARG_CY		"-Y"
@ MSG_ARG_CYL		"-YL"
@ MSG_ARG_CYP		"-YP"
@ MSG_ARG_CYU		"-YU"
@ MSG_ARG_Z		"-z"
@ MSG_ARG_ZDEFNODEF	"-zdefs/nodefs"
@ MSG_ARG_ZNODEF	"-znodefs"
@ MSG_ARG_ZTEXT		"-ztext"
@ MSG_ARG_ZTEXTOFF	"-ztextoff"
@ MSG_ARG_ZTEXTWARN	"-ztextwarn"
@ MSG_ARG_ZTEXTALL	"-z[text|textwarn|textoff]"
@ MSG_ARG_ZLOADFLTR	"-zloadfltr"
@ MSG_ARG_ZCOMBRELOC	"-zcombreloc"

@ MSG_ARG_ABSEXEC	"absexec"
@ MSG_ARG_GROUPPERM	"groupperm"
@ MSG_ARG_NOGROUPPERM	"nogroupperm"
@ MSG_ARG_LAZYLOAD	"lazyload"
@ MSG_ARG_NOLAZYLOAD	"nolazyload"
@ MSG_ARG_INTERPOSE	"interpose"
@ MSG_ARG_DIRECT	"direct"
@ MSG_ARG_IGNORE	"ignore"
@ MSG_ARG_RECORD	"record"
@ MSG_ARG_INITFIRST	"initfirst"
@ MSG_ARG_TRANSLATOR	"translator"
@ MSG_ARG_NOOPEN	"nodlopen"
@ MSG_ARG_NOW		"now"
@ MSG_ARG_ORIGIN	"origin"
@ MSG_ARG_DEFS		"defs"
@ MSG_ARG_NODEFS	"nodefs"
@ MSG_ARG_NODUMP	"nodump"
@ MSG_ARG_NOVERSION	"noversion"
@ MSG_ARG_TEXT		"text"
@ MSG_ARG_TEXTOFF	"textoff"
@ MSG_ARG_TEXTWARN	"textwarn"
@ MSG_ARG_MULDEFS	"muldefs"
@ MSG_ARG_NODELETE	"nodelete"
@ MSG_ARG_NOPARTIAL	"nopartial"
@ MSG_ARG_REDLOCSYM	"redlocsym"
@ MSG_ARG_WEAKEXT	"weakextract"
@ MSG_ARG_LOADFLTR	"loadfltr""
@ MSG_ARG_ALLEXTRT	"allextract"
@ MSG_ARG_DFLEXTRT	"defaultextract"
@ MSG_ARG_COMBRELOC	"combreloc"
@ MSG_ARG_NODEFAULTLIB	"nodefaultlib"
@ MSG_ARG_ENDFILTEE	"endfiltee"

@ MSG_ARG_GROUP		"group"
@ MSG_ARG_REDUCE	"reduce"
@ MSG_ARG_STATIC	"static"

@ MSG_ARG_LCOM		"L,"
@ MSG_ARG_PCOM		"P,"
@ MSG_ARG_UCOM		"U,"


# File and pathname strings

@ MSG_FIL_LIBSTAB	"libldstab.so.1"

# Environment variable strings

@ MSG_LD_OPTIONS	"LD_OPTIONS"
@ MSG_LD_RUN_PATH	"LD_RUN_PATH"
@ MSG_LD_LIBPATH	"LD_LIBRARY_PATH"
@ MSG_LD_LIBPATH_64	"LD_LIBRARY_PATH_64"

@ MSG_SGS_SUPPORT	"SGS_SUPPORT"


# Symbol names

@ MSG_SYM_LIBVER_U	"_lib_version"


# Mapfile tokens

@ MSG_MAP_TOK_1		"\"\n"
@ MSG_MAP_TOK_2		" \"\t\n:;=#"
@ MSG_MAP_LOAD		"load"
@ MSG_MAP_NOTE		"note"
@ MSG_MAP_NEED		"need"
@ MSG_MAP_SPECVERS	"specvers"
@ MSG_MAP_ADDVERS	"addvers"
@ MSG_MAP_GLOBAL	"global"
@ MSG_MAP_FUNCTION	"function"
@ MSG_MAP_DATA		"data"
@ MSG_MAP_COMMON	"common"
@ MSG_MAP_PASSTHRU	"passthru"
@ MSG_MAP_PARENT	"parent"
@ MSG_MAP_EXTERN	"extern"
