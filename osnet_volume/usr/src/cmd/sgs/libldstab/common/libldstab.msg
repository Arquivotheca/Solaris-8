#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)libldstab.msg	1.3	97/04/04 SMI"


@ _START_

# Message file for cmd/sgs/libldstab.

@ MSG_ID_LIBLDSTAB


# Stab processing errors.

@ MSG_STAB_MISTBL	"file %s: .stab[.index|.sbfocus] found with no \
			 matching string table"
@ MSG_STAB_BADTBL	"file %s: .stab[.index|.sbfocus] error when reading \
			 string table"
@ MSG_STAB_NOSBROW	"SourceBrowser data will be lost"

# System errors

@ MSG_SYS_EXEC		"file %s: %s exec failed: %s: %s"
@ MSG_SYS_FORK		"file %s: fork failed: %s: %s"
@ MSG_SYS_GETCWD	"file %s: getcwd failed: %s"

@ _END_

# The following strings represent reserved names.  Reference to these strings
# is via the MSG_ORIG() macro, and thus translations are not required.

# Elf processing messages.

@ MSG_ELF_GETDATA	"file %s: elf_getdata"
@ MSG_ELF_GETSCN	"file %s: elf_getscn"

@ MSG_STR_DASHR		"-r"
@ MSG_STR_DASHX		"-x"
@ MSG_STR_W		"w"

@ MSG_STR_SBFOCUS	"sbfocus"

@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"

# Section strings

@ MSG_SCN_STAB		".stab"
@ MSG_SCN_STABSTR	".stabstr"
@ MSG_SCN_STABINDEX	".stab.index"
@ MSG_SCN_STABINDEXSTR	".stab.indexstr"
@ MSG_SCN_STABSBFOCUS	".stab.sbfocus"
@ MSG_SCN_STABSBFOCUSTR	".stab.sbfocusstr"
