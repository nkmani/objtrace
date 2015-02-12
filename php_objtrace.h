#ifndef PHP_OBJTRACE_H
#define PHP_OBJTRACE_H 1

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#ifdef ZTS
#include "TSRM.h"
#endif

#include "zend.h"
#include "zend_hash.h"
#include "zend_compile.h"
#include "zend_execute.h"
#include "zend_extensions.h"
#include "zend_ini.h"
#include "zend_vm.h"

ZEND_BEGIN_MODULE_GLOBALS(objtrace)
int mode;
char *trace_output_dir;
char *trace_output_name;
FILE *log;
HashTable *sources;
ZEND_END_MODULE_GLOBALS(objtrace)

#define OBJTRACE_MODE_FLAG_ENABLED 0x000001
#define OBJTRACE_MODE_FLAG_VALUES  0x000002
#define OBJTRACE_MODE_FLAG_DEBUG   0x000004

typedef struct _objtrace_source {
  char *file;
  char *source;
  int size;
  int count;
  char **lines;
} objtrace_source;

#ifdef ZTS
#define OTG(v) TSRMG(objtrace_globals_id, zend_objtrace_globals *, v)
#else
#define OTG(v) (objtrace_globals.v)
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
