#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)tnf_process_disable.spec	1.1	99/01/25 SMI"
#
# lib/libtnf/spec/tnf_process_disable.spec

function	tnf_process_disable
include		<tnf/probe.h>
declaration	void tnf_process_disable(void )
		### ERROR: MISSING REQUIRED KEYWORD: version (tnf_process_disable.daa)
end		

function	tnf_process_enable
include		<tnf/probe.h>
declaration	void tnf_process_enable(void )
		### ERROR: MISSING REQUIRED KEYWORD: version (tnf_process_disable.daa)
end		

function	tnf_thread_disable
include		<tnf/probe.h>
declaration	void tnf_thread_disable(void )
		### ERROR: MISSING REQUIRED KEYWORD: version (tnf_process_disable.daa)
end		

function	tnf_thread_enable
include		<tnf/probe.h>
declaration	void tnf_thread_enable(void)
		### ERROR: MISSING REQUIRED KEYWORD: version (tnf_process_disable.daa)
end		

