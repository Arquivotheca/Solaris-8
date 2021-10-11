#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)md5_psr-sun4u.spec	1.1	99/07/02 SMI"
#
# lib/libmd5_psr/spec/sparcv9/md5_psr-sun4u.spec
#

function	MD5Init extends libmd5/spec/md5.spec
arch		sparcv9
version		SUNWprivate_1.1
end

function	MD5Update extends libmd5/spec/md5.spec
arch		sparcv9
version		SUNWprivate_1.1
end

function	MD5Final extends libmd5/spec/md5.spec
arch		sparcv9
version		SUNWprivate_1.1
end

function	md5_calc extends libmd5/spec/md5.spec
arch		sparcv9
version		SUNWprivate_1.1
end
