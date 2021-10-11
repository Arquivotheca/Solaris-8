#
# Copyright (c) 1999 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)md5.spec	1.1	99/07/02 SMI"
#
# lib/libmd5/spec/md5.spec
#

function	MD5Init
include		<md5.h>
declaration	void MD5Init(MD5_CTX *context)
version		SUNW_1.1
end

function	MD5Update
include		<md5.h>
declaration	void MD5Update(MD5_CTX *context, \
				unsigned char *input, \
				unsigned int inputLen)
version		SUNW_1.1
end

function	MD5Final
include		<md5.h>
declaration	void MD5Final(unsigned char digest[16], MD5_CTX *context)
version		SUNW_1.1
end

function	md5_calc
declaration	void md5_calc(unsigned char *output, \
				unsigned char *input, \
				unsigned int inlen)
version		SUNW_1.1
end
