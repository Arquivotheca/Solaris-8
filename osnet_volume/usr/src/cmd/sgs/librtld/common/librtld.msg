#
# Copyright (c) 1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)librtld.msg	1.9	98/08/28 SMI"


@ _START_

# Messages for cmd/sgs/librtld

@ MSG_ID_LIBRTLD


# System call messages

@ MSG_SYS_PROC		"%s: /proc error: %s"
@ MSG_SYS_OPEN		"%s: open failed: %s"


# Image processing messages

@ MSG_IMG_ELF		"%s: is not a dynamic ELF object"
@ MSG_IMG_DATASEG	"%s: data segment not found"
@ MSG_IMG_DATASEC	"%s: final data section not found"


# ELF processing messages

@ MSG_DT_UNKNOWN	"%s: unknown dynamic entry: %lld"



# Basic strings

@ MSG_STR_UNKNOWN       "<unknown>"


@ _END_


# The following strings represent reserved section and symbol names.  Reference
# to these strings is via the MSG_ORIG() macro, and thus no translations are
# required.

# ELF processing messages
@ MSG_ELF_BEGIN		"%s: elf_begin"
@ MSG_ELF_GETDATA	"%s: elf_getdata"
@ MSG_ELF_GETEHDR	"%s: elf_getehdr"
@ MSG_ELF_GETPHDR	"%s: elf_getphdr"
@ MSG_ELF_GETSCN	"%s: elf_getscn"
@ MSG_ELF_GETSHDR	"%s: elf_getshdr"
@ MSG_ELF_NEWDATA	"%s: elf_newdata"
@ MSG_ELF_NEWEHDR	"%s: elf_newehdr"
@ MSG_ELF_NEWPHDR	"%s: elf_newphdr"
@ MSG_ELF_NEWSCN	"%s: elf_newscn"
@ MSG_ELF_NEWSHDR	"%s: elf_newshdr"
@ MSG_ELF_UPDATE	"%s: elf_update"

@ MSG_SCN_HEAP		".SUNW_heap"
@ MSG_SCN_RELOC		".SUNW_reloc"
@ MSG_SCN_SHSTR		".shstrtab"

@ MSG_SYM_END		"_end"
@ MSG_SYM_EDATA		"_edata"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
