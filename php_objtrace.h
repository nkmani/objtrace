#ifndef PHP_OBJTRACE_H
#define PHP_OBJTRACE_H 1

#ifdef ZTS
#include "TSRM.h"
#endif

ZEND_BEGIN_MODULE_GLOBALS(objtrace)
long counter;
ZEND_END_MODULE_GLOBALS(objtrace)

#ifdef ZTS
#define OBJTRACE_G(v) TSRMG(objtrace_globals_id, zend_objtrace_globals *, v)
#else
#define OBJTRACE_G(v) (objtrace_globals.v)
#endif

#define PHP_OBJTRACE_VERSION "1.0"
#define PHP_OBJTRACE_EXTNAME "objtrace"

PHP_MINIT_FUNCTION(objtrace);
PHP_MSHUTDOWN_FUNCTION(objtrace);
PHP_RINIT_FUNCTION(objtrace);

PHP_FUNCTION(objtrace);

extern zend_module_entry objtrace_module_entry;
#define phpext_objtrace_ptr &objtrace_module_entry

#endif
