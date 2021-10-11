#if defined(__cplusplus) && !defined(PERL_OBJECT)
#define is_cplusplus
#endif

#ifdef is_cplusplus
extern "C" {
#endif

#include <EXTERN.h>
#include <perl.h>
#ifdef PERL_OBJECT
#define NO_XSLOCKS
#include <XSUB.h>
#include "win32iop.h"
#include <fcntl.h>
#include <perlhost.h>
#endif
#ifdef is_cplusplus
}
#  ifndef EXTERN_C
#    define EXTERN_C extern "C"
#  endif
#else
#  ifndef EXTERN_C
#    define EXTERN_C extern
#  endif
#endif

EXTERN_C void xs_init _((void));

EXTERN_C void boot_Apache _((CV* cv));
EXTERN_C void boot_Apache__Constants _((CV* cv));
EXTERN_C void boot_Apache__ModuleConfig _((CV* cv));
EXTERN_C void boot_Apache__Log _((CV* cv));
EXTERN_C void boot_Apache__URI _((CV* cv));
EXTERN_C void boot_Apache__Util _((CV* cv));
EXTERN_C void boot_Apache__Connection _((CV* cv));
EXTERN_C void boot_Apache__Server _((CV* cv));
EXTERN_C void boot_Apache__File _((CV* cv));
EXTERN_C void boot_Apache__Table _((CV* cv));
EXTERN_C void boot_DynaLoader _((CV* cv));

EXTERN_C void
xs_init(void)
{
	char *file = __FILE__;
	dXSUB_SYS;

	newXS("Apache::bootstrap", boot_Apache, file);
	newXS("Apache::Constants::bootstrap", boot_Apache__Constants, file);
	newXS("Apache::ModuleConfig::bootstrap", boot_Apache__ModuleConfig, file);
	newXS("Apache::Log::bootstrap", boot_Apache__Log, file);
	newXS("Apache::URI::bootstrap", boot_Apache__URI, file);
	newXS("Apache::Util::bootstrap", boot_Apache__Util, file);
	newXS("Apache::Connection::bootstrap", boot_Apache__Connection, file);
	newXS("Apache::Server::bootstrap", boot_Apache__Server, file);
	newXS("Apache::File::bootstrap", boot_Apache__File, file);
	newXS("Apache::Table::bootstrap", boot_Apache__Table, file);
	/* DynaLoader is a special case */
	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}
