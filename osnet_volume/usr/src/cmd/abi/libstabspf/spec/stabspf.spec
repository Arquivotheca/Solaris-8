#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)stabspf.spec	1.1	99/05/14 SMI"
#
# cmd/abi/libstabspf/spec/stabspf.spec

function	spf_load_stabs
include		<apptrace.h>
declaration	stabsret_t spf_load_stabs(const char *elf_filename)
version		SUNWprivate_1.1
exception	$return != STAB_SUCCESS
end

function	spf_prtype
include		<apptrace.h>
declaration	int spf_prtype(FILE *fp, char const *type, int ref, \
			void const *addr)
version		SUNWprivate_1.1
exception	$return <= 0
end
