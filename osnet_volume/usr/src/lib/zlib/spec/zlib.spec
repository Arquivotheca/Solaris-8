#
#ident	"@(#)zlib.spec	1.1	99/10/08 SMI"
#
# lib/zlib/spec/zlib.spec

function	zlibVersion
include		<zlib.h>
declaration	const char *zlibVersion(void)
version		SUNW_1.1
end

function	deflateInit_
include		<zlib.h>
declaration	int deflateInit_(z_streamp strm, int level, \
		    const char *version, int stream_size)
version		SUNW_1.1
end

function	deflateInit2_
include		<zlib.h>
declaration	int deflateInit2_(z_streamp strm, int  level, int  method, \
		    int windowBits, int memLevel, int strategy, \
		    const char *version, int stream_size)
version		SUNW_1.1
end

function	deflate
include		<zlib.h>
declaration	int deflate(z_streamp strm, int flush)
version		SUNW_1.1
end

function	deflateSetDictionary
include		<zlib.h>
declaration	int deflateSetDictionary(z_streamp strm, \
		    const Bytef *dictionary, uInt dictLength)
version		SUNW_1.1
end

function	deflateCopy
include		<zlib.h>
declaration	int deflateCopy(z_streamp dest, z_streamp source)
version		SUNW_1.1
end

function	deflateReset
include		<zlib.h>
declaration	int deflateReset(z_streamp strm)
version		SUNW_1.1
end

function	deflateParams
include		<zlib.h>
declaration	int deflateParams(z_streamp strm, int level, int strategy)
version		SUNW_1.1
end

function	deflateEnd
include		<zlib.h>
declaration	int deflateEnd(z_streamp strm)
version		SUNW_1.1
end

function	inflateInit_
include		<zlib.h>
declaration	int inflateInit_(z_streamp strm, const char *version, \
		    int stream_size)
version		SUNW_1.1
end

function	inflateInit2_
include		<zlib.h>
declaration	int inflateInit2_(z_streamp strm, int  windowBits, \
		    const char *version, int stream_size)
version		SUNW_1.1
end

function	inflate
include		<zlib.h>
declaration	int inflate(z_streamp strm, int flush)
version		SUNW_1.1
end

function	inflateSetDictionary
include		<zlib.h>
declaration	int inflateSetDictionary(z_streamp strm, \
		    const Bytef *dictionary, uInt dictLength)
version		SUNW_1.1
end

function	inflateSync
include		<zlib.h>
declaration	int inflateSync(z_streamp strm)
version		SUNW_1.1
end

function	inflateReset
include		<zlib.h>
declaration	int inflateReset(z_streamp strm)
version		SUNW_1.1
end

function	inflateEnd
include		<zlib.h>
declaration	int inflateEnd(z_streamp strm)
version		SUNW_1.1
end

function	compress
include		<zlib.h>
declaration	int compress(Bytef *dest, uLongf *destLen, \
		    const Bytef *source, uLong sourceLen)
version		SUNW_1.1
end

function	compress2
include		<zlib.h>
declaration	int compress2(Bytef *dest, uLongf *destLen, \
		    const Bytef *source, uLong sourceLen, int level)
version		SUNW_1.1
end

function	uncompress
include		<zlib.h>
declaration	int uncompress(Bytef *dest, uLongf *destLen, \
		    const Bytef *source, uLong sourceLen)
version		SUNW_1.1
end

function	gzopen
include		<zlib.h>
declaration	gzFile gzopen(const char *path, const char *mode)
version		SUNW_1.1
end

function	gzdopen
include		<zlib.h>
declaration	gzFile gzdopen(int fd, const char *mode)
version		SUNW_1.1
end

function	gzsetparams
include		<zlib.h>
declaration	int gzsetparams(gzFile file, int level, int strategy)
version		SUNW_1.1
end

function	gzread
include		<zlib.h>
declaration	int gzread(gzFile file, voidp buf, unsigned len)
version		SUNW_1.1
end

function	gzwrite
include		<zlib.h>
declaration	int gzwrite(gzFile file, const voidp buf, unsigned len)
version		SUNW_1.1
end

function	gzprintf
include		<zlib.h>
declaration	int gzprintf(gzFile file, const char *format, ...)
version		SUNW_1.1
end

function	gzputs
include		<zlib.h>
declaration	int gzputs(gzFile file, const char *s)
version		SUNW_1.1
end

function	gzgets
include		<zlib.h>
declaration	char *gzgets(gzFile file, char *buf, int len)
version		SUNW_1.1
end

function	gzputc
include		<zlib.h>
declaration	int gzputc(gzFile file, int c)
version		SUNW_1.1
end

function	gzgetc
include		<zlib.h>
declaration	int gzgetc(gzFile file)
version		SUNW_1.1
end

function	gzflush
include		<zlib.h>
declaration	int gzflush(gzFile file, int flush)
version		SUNW_1.1
end

function	gzseek
include		<zlib.h>
declaration	z_off_t gzseek(gzFile file, z_off_t offset, int whence)
version		SUNW_1.1
end

function	gzrewind
include		<zlib.h>
declaration	int gzrewind(gzFile file)
version		SUNW_1.1
end

function	gztell
include		<zlib.h>
declaration	z_off_t gztell(gzFile file)
version		SUNW_1.1
end

function	gzeof
include		<zlib.h>
declaration	int gzeof(gzFile file)
version		SUNW_1.1
end

function	gzclose
include		<zlib.h>
declaration	int gzclose(gzFile file)
version		SUNW_1.1
end

function	gzerror
include		<zlib.h>
declaration	const char *gzerror(gzFile file, int *errnum)
version		SUNW_1.1
end

function	adler32
include		<zlib.h>
declaration	uLong adler32(uLong adler, const Bytef *buf, uInt len)
version		SUNW_1.1
end

function	crc32
include		<zlib.h>
declaration	uLong crc32(uLong crc, const Bytef *buf, uInt len)
version		SUNW_1.1
end

function	zError
include		<zlib.h>
declaration	const char *zError(int err)
version		SUNW_1.1
end

function	inflateSyncPoint
include		<zlib.h>
declaration	int inflateSyncPoint(z_streamp z)
version		SUNW_1.1
end

function	get_crc_table
include		<zlib.h>
declaration	const uLongf *get_crc_table(void)
version		SUNW_1.1
end
