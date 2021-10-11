#
# Copyright (c) 1998-1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)gettext.spec	1.1	99/01/25 SMI"
#
# lib/libintl/spec/gettext.spec

function	gettext extends libc/spec/i18n.spec gettext
version		SUNW_0.7
end		

function	dgettext extends libc/spec/i18n.spec dgettext
version		SUNW_0.7
end		

function	dcgettext extends libc/spec/i18n.spec dcgettext
version		SUNW_0.8
end		

function	textdomain extends libc/spec/i18n.spec textdomain
version		SUNW_0.7
end		

function	bindtextdomain extends libc/spec/i18n.spec bindtextdomain
version		SUNW_0.7
end		

