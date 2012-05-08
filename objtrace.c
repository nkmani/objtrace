#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "php_ini.h"
#include "php_objtrace.h"

ZEND_DECLARE_MODULE_GLOBALS(objtrace)

static function_entry objtrace_functions[] = {
    PHP_FE(objtrace, NULL)
    {NULL, NULL, NULL}
};

zend_module_entry objtrace_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
    STANDARD_MODULE_HEADER,
#endif
    PHP_OBJTRACE_EXTNAME,
    objtrace_functions,
    PHP_MINIT(objtrace),
    PHP_MSHUTDOWN(objtrace),
    PHP_RINIT(objtrace),
    NULL,
    NULL,
#if ZEND_MODULE_API_NO >= 20010901
    PHP_OBJTRACE_VERSION,
#endif
    STANDARD_MODULE_PROPERTIES
};

PHP_INI_BEGIN()
PHP_INI_ENTRY("objtrace.mode", "1", PHP_INI_ALL, NULL)
PHP_INI_END()

static void php_objtrace_init_globals(zend_objtrace_globals *objtrace_globals)
{
	objtrace_globals->counter = 0;
}

PHP_MINIT_FUNCTION(objtrace)
{
	ZEND_INIT_MODULE_GLOBALS(objtrace, php_objtrace_init_globals, NULL);
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(objtrace)
{
	UNREGISTER_INI_ENTRIES();
	return SUCCESS;
}

PHP_RINIT_FUNCTION(objtrace)
{
	OBJTRACE_G(counter) = 0;
	return SUCCESS;
}

#ifdef COMPILE_DL_OBJTRACE
ZEND_GET_MODULE(objtrace)
#endif

static void objtrace_debug_dump(zval *o) {
	switch (Z_TYPE_P(o)) {
        case IS_NULL:
            php_printf("NULL\n");
            break;
        case IS_BOOL:
            php_printf("Boolean: %s\n", Z_LVAL_P(o) ? "TRUE" : "FALSE");
            break;
        case IS_LONG:
            php_printf("Long: %ld\n", Z_LVAL_P(o));
            break;
        case IS_DOUBLE:
            php_printf("Double: %f\n", Z_DVAL_P(o));
            break;
        case IS_STRING:
            php_printf("String: ");
            PHPWRITE(Z_STRVAL_P(o), Z_STRLEN_P(o));
            php_printf("\n");
            break;
        case IS_RESOURCE:
            php_printf("Resource\n");
            break;
        case IS_ARRAY:
            php_printf("Array\n");
            break;
        case IS_OBJECT:
            php_printf("Object\n");
            break;
        default:
            php_printf("Unknown\n");
    }
}

PHP_FUNCTION(objtrace)
{
	long mode = INI_INT("objtrace.mode");

	OBJTRACE_G(counter)++;

	zval *o;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &o) == FAILURE) {
		RETURN_NULL();
	}

	objtrace_debug_dump(o);

	char *s = emalloc(128);
	sprintf(s, "Object Tracing -- mode:%ld counter:%ld", mode, OBJTRACE_G(counter));
	RETURN_STRING(s, 0);
}
