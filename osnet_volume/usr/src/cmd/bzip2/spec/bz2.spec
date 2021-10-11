#
#
#ident	"@(#)bz2.spec	1.1	99/10/08 SMI"
#
# cmd/bzip2/spec/bz2.spec

function	bzCompressInit
include		<stdio.h>, <bzlib.h>
declaration	int bzCompressInit(bz_stream *strm, int blockSize100k, \
		int verbosity, int workFactor)
version		SUNW_1.1
end

function	bzCompress
include		<stdio.h>, <bzlib.h>
declaration	int bzCompress(bz_stream *strm, int action)
version		SUNW_1.1
end

function	bzCompressEnd
include		<stdio.h>, <bzlib.h>
declaration	int bzCompressEnd(bz_stream *strm)
version		SUNW_1.1
end


function	bzDecompressInit
include		<stdio.h>, <bzlib.h>
declaration	int bzDecompressInit(bz_stream *strm, int verbosity, int small)
version		SUNW_1.1
end

function	bzDecompress
include		<stdio.h>, <bzlib.h>
declaration	int bzDecompress(bz_stream *strm)
version		SUNW_1.1
end

function	bzDecompressEnd
include		<stdio.h>, <bzlib.h>
declaration	int bzDecompressEnd(bz_stream *strm)
version		SUNW_1.1
end

function	bzReadOpen
include		<stdio.h>, <bzlib.h>
declaration	BZFILE *bzReadOpen(int *bzerror, FILE *f, int verbosity, \
		int small, void *unused, int nUnused)
version		SUNW_1.1
end

function	bzReadClose
include		<stdio.h>, <bzlib.h>
declaration	void bzReadClose(int *bzerror, BZFILE *b)
version		SUNW_1.1
end

function	bzReadGetUnused
include		<stdio.h>, <bzlib.h>
declaration	void bzReadGetUnused(int *bzerror, BZFILE *b, void **unused, \
		int *nUnused)
version		SUNW_1.1
end

function	bzRead
include		<stdio.h>, <bzlib.h>
declaration	int bzRead(int *bzerror, BZFILE *b, void *buf, int len)
version		SUNW_1.1
end

function	bzWriteOpen
include		<stdio.h>, <bzlib.h>
declaration	BZFILE *bzWriteOpen(int *bzerror, FILE *f, int blockSize100k, \
		int verbosity, int workFactor)
version		SUNW_1.1
end

function	bzWrite
include		<stdio.h>, <bzlib.h>
declaration	void bzWrite(int *bzerror, BZFILE *b, void *buf, int len)
version		SUNW_1.1
end

function	bzWriteClose
include		<stdio.h>, <bzlib.h>
declaration	void bzWriteClose(int *bzerror, BZFILE *b, int abandon, \
		unsigned int *nbytes_in, unsigned int *nbytes_out)
version		SUNW_1.1
end

function	bzBuffToBuffCompress
include		<stdio.h>, <bzlib.h>
declaration	int bzBuffToBuffCompress(char *dest, unsigned int *destLen, \
		char *source, unsigned int sourceLen, int blockSize100k, \
		int verbosity, int workFactor)
version		SUNW_1.1
end

function	bzBuffToBuffDecompress
include		<stdio.h>, <bzlib.h>
declaration	int bzBuffToBuffDecompress(char *dest, unsigned int *destLen, \
		char *source, unsigned int sourceLen, int small, int verbosity)
version		SUNW_1.1
end

function	bzlibVersion
include		<stdio.h>, <bzlib.h>
declaration	const char *bzlibVersion(void)
version		SUNW_1.1
end

function	bzopen
include		<stdio.h>, <bzlib.h>
declaration	BZFILE *bzopen(const char *path, const char *mode)
version		SUNW_1.1
end

function	bzdopen
include		<stdio.h>, <bzlib.h>
declaration	BZFILE *bzdopen(int fd, const char *mode)
version		SUNW_1.1
end

function	bzread
include		<stdio.h>, <bzlib.h>
declaration	int bzread(BZFILE *b, void *buf, int len)
version		SUNW_1.1
end

function	bzwrite
include		<stdio.h>, <bzlib.h>
declaration	int bzwrite(BZFILE *b, void *buf, int len)
version		SUNW_1.1
end

function	bzflush
include		<stdio.h>, <bzlib.h>
declaration	int bzflush(BZFILE *b)
version		SUNW_1.1
end

function	bzclose
include		<stdio.h>, <bzlib.h>
declaration	void bzclose(BZFILE *b)
version		SUNW_1.1
end

function	bzerror
include		<stdio.h>, <bzlib.h>
declaration	const char *bzerror(BZFILE *b, int *errnum)
version		SUNW_1.1
end
