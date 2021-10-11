/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fns_symbol.cc	1.25	97/11/12 SMI"

#include <stdio.h>
#include <dlfcn.h>
#include <sys/param.h>
#include <string.h>
#include <synch.h>
#include <ctype.h>

#ifdef DEBUG
#define	DEBUG_LIBPATH
#endif

// When compiled with -DDEBUG_LIBPATH:
//
//    - User may add directories (such as $ROOT/usr/lib/fn) to
//	$LD_LIBRARY_PATH; they will be searched before /usr/lib/fn.
//
//    - FNS_library_path is global, so we can use nm to see if we compiled
//	the library with DEBUG_LIBPATH.

#ifndef DEBUG_LIBPATH
static
#endif

#if defined(__sparcv9)
const char	*FNS_library_path = "/usr/lib/fn/sparcv9/";
#else
const char	*FNS_library_path = "/usr/lib/fn/";
#endif

// Caching dlopen of zero
static mutex_t	self_lock = DEFAULTMUTEX;
static void *dlopen_of_zero = 0;

// Caching functions for dlopen
class FNSP_functions {
protected:
	char *function;
	char *module;
	void *fh;
	int delete_module;
	void *mh;
	FNSP_functions *next;
	void *create_function(const char *func, const char *mod,
	    void *mod_mh);

public:
	FNSP_functions();
	virtual ~FNSP_functions();
	void *get_function(const char *func, const char *mod,
	    void *&mod_mh);
};

FNSP_functions::FNSP_functions()
{
	function = 0;
	module = 0;
	next = 0;
	delete_module = 0;
}

FNSP_functions::~FNSP_functions()
{
	if (delete_module)
		dlclose(mh);
	delete[] function;
	delete[] module;
	delete next;
}

void *
FNSP_functions::get_function(const char *func, const char *mod,
    void *&mod_mh)
{
	if ((module) && (strcmp(module, mod) == 0)) {
		if (strcmp(function, func) == 0)
			return (fh);
		mod_mh = mh;
	}

	if (next)
		return (next->get_function(func, mod, mod_mh));

	// dlopen the shared object and dlsym the function
	next = new FNSP_functions;
	if (next == 0)
		return (0);
	if (next->create_function(func, mod, mod_mh))
		return (next->fh);
	// else
	delete next;
	next = 0;
	return (0);
}

void *
FNSP_functions::create_function(const char *func, const char *mod,
    void *mod_mh)
{
	// Check if the module is previously opened
	if (mod_mh)
		mh = mod_mh;
	else {
		char mname[MAXPATHLEN];
#ifdef DEBUG_LIBPATH
		// Follow LD_LIBRARY_PATH.
		strcpy(mname, mod);
		strcat(mname, ".so");
		strcat(mname, FNVERS);
		// %%% global in dlopen to overcome linker bug
		// bug# 1248401
		if (mh = dlopen(mname, RTLD_LAZY | RTLD_GLOBAL)) {
			delete_module = 1;
			goto find_func;
		}
#endif
		strcpy(mname, FNS_library_path);
		strcat(mname, mod);
		strcat(mname, ".so");
		strcat(mname, FNVERS);
		// %%% global in dlopen to overcome linker bug
		// bug# 1248401
		if ((mh = dlopen(mname, RTLD_LAZY | RTLD_GLOBAL)) == 0)
			return (0);
		else
			delete_module = 1;
	}

	// dlsym
#ifdef DEBUG_LIBPATH
 find_func:
#endif
	if ((fh = dlsym(mh, func)) == 0)
		return (0);

	// Copy the function name and module names
	function = new char[strlen(func) + 1];
	module = new char[strlen(mod) + 1];
	if ((function == 0) || (module == 0))
		// Malloc error
		return (0);
	strcpy(function, func);
	strcpy(module, mod);

	// Return the function pointer
	return (fh);
}

class FNSP_functions_of_dlzero {
protected:
	char *function;
	void *fh;
	FNSP_functions_of_dlzero *next;
public:
	FNSP_functions_of_dlzero();
	virtual ~FNSP_functions_of_dlzero();
	void *get_function(const char *func);
};

FNSP_functions_of_dlzero::FNSP_functions_of_dlzero()
{
	function = 0;
	next = 0;
	if (dlopen_of_zero == 0)
		dlopen_of_zero = dlopen(0, RTLD_LAZY);
}

FNSP_functions_of_dlzero::~FNSP_functions_of_dlzero()
{
	delete [] function;
	if (dlopen_of_zero)
		dlclose(dlopen_of_zero);
	dlopen_of_zero = 0;
	delete next;
}

void *
FNSP_functions_of_dlzero::get_function(const char *func)
{
	if (dlopen_of_zero == NULL)
		return (0);

	if ((function) && (strcmp(function, func) == 0))
		return (fh);

	if (next)
		return (next->get_function(func));

	next = new FNSP_functions_of_dlzero;
	if (next == 0)
		return (0);
	next->function = new char[strlen(func) + 1];
	if (next->function == 0) {
		delete next;
		next = 0;
		return (0);
	}
	strcpy(next->function, func);
	next->fh = dlsym(dlopen_of_zero, func);
	return (next->fh);
}

static FNSP_functions fns_link_functions;
static FNSP_functions_of_dlzero dlzero_functions;

void *
fns_link_symbol(const char *function_name, const char *module_name)
{
	void *fh = 0;
	void *mh = 0;

	if (function_name == 0)
		return (0);
	mutex_lock(&self_lock);

	// Check first with dlopen_of_zero, then the shared object
	fh = dlzero_functions.get_function(function_name);

	// Look into the shared object module, if required
	if ((fh == 0) && (module_name)) {
		fh = fns_link_functions.get_function(
		    function_name, module_name, mh);
	}

	mutex_unlock(&self_lock);
	return (fh);
}


// The first character in a legal C identifier is either a _ or
// a letter in the alphabet (both upper and lower cases)

static inline int
legal_first_char(char c)
{
	return (isalpha((int)c) || c == '_');
}

// The non-first characters in a legal C identifier is one of
// _
// a letter in the alphabet (both upper and lower cases)
// a digit (0-9)

static inline int
legal_rest_char(char c)
{
	return (isalnum((int)c) || c == '_');
}

#define	DEF_ID_CHAR	'_'

void
fns_legal_C_identifier(char *outstr, const char *instr, size_t len)
{
	size_t i;

	outstr[len] = '\0';  // terminate string

	if (len == 0)
		return;

	outstr[0] = (legal_first_char(instr[0]) ? instr[0] : DEF_ID_CHAR);

	for (i = 1; i < len; i ++)
		outstr[i] =
		    (legal_rest_char(instr[i]) ? instr[i] : DEF_ID_CHAR);
}
