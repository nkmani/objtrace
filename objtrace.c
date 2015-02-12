#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "php.h"
#include "php_ini.h"
#include "php_objtrace.h"

ZEND_DECLARE_MODULE_GLOBALS(objtrace)

static function_entry objtrace_functions[] = {
PHP_FE(objtrace, NULL) {
NULL, NULL, NULL } };

zend_module_entry objtrace_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
        STANDARD_MODULE_HEADER,
#endif
        PHP_OBJTRACE_EXTNAME, objtrace_functions,
        PHP_MINIT(objtrace),
        PHP_MSHUTDOWN(objtrace),
        PHP_RINIT(objtrace),
        NULL,
        NULL,
#if ZEND_MODULE_API_NO >= 20010901
        PHP_OBJTRACE_VERSION,
#endif
        STANDARD_MODULE_PROPERTIES };

#ifdef P_tmpdir
# define OBJTRACE_TEMP_DIR P_tmpdir
#else
# define OBJTRACE_TEMP_DIR "/tmp"
#endif

PHP_INI_BEGIN()
PHP_INI_ENTRY("objtrace.mode", "1", PHP_INI_ALL, NULL)
PHP_INI_ENTRY("objtrace.trace_output_dir", OBJTRACE_TEMP_DIR, PHP_INI_ALL, NULL)
PHP_INI_ENTRY("objtrace.trace_output_name", "objtrace.ot", PHP_INI_ALL, NULL)
PHP_INI_END()

static void php_objtrace_init_globals(zend_objtrace_globals *og) {
    og->log = NULL;
    og->sources = malloc(sizeof(HashTable));
}

static void objtrace_source_dtor(objtrace_source *src TSRMLS_DC) {
    if (src) {
        if (src->source)
            free(src->source);
        if (src->lines)
            free(src->lines);
        free(src);
    }
}

static objtrace_source *read_file(char *file) {
    int fd;
    struct stat buf;
    char s[1024];
    int i, cnt, maxlines;
    char *p, *n;

    fd = open(file, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Error opening %s\n", file);
        return NULL;
    }

    if (fstat(fd, &buf) < 0) {
        fprintf(stderr, "Can't stat file\n");
        close(fd);
        return NULL;
    }

    objtrace_source *src = (objtrace_source *) malloc(sizeof(objtrace_source));
    src->file = file;
    src->source = (char *) malloc(buf.st_size);
    if (src->source == NULL) {
        fprintf(stderr, "Can't alloate %ld bytes for file\n", buf.st_size);
        close(fd);
        objtrace_source_dtor(src);
        return NULL;
    }

    p = src->source;
    while ((cnt = read(fd, s, sizeof(s))) > 0) {
        strncpy(p, s, cnt);
        p += cnt;
    }

    close(fd);

    p = n = src->source;
    maxlines = 1024;
    src->lines = (char **) malloc(sizeof(char *) * maxlines);
    if (src->lines == NULL) {
        fprintf(stderr, "Can't allocate line buffer\n");
        objtrace_source_dtor(src);
        return NULL;
    }

    src->lines[0] = p;
    src->count = 1;
    for (i = 0; i < buf.st_size; i++) {
        if (p[i] == '\n') {
            p[i] = '\0';
            src->lines[src->count] = n;
            n = p + i + 1;
            src->count++;
            if (src->count > maxlines) {
                maxlines += 1024;
                src->lines = (char **) realloc(src->lines, sizeof(char *) * maxlines);
                if (src->lines == NULL) {
                    fprintf(stderr, "Can't realloc line buffer\n");
                    objtrace_source_dtor(src);
                    return NULL;
                }
            }
        }
    }

    return src;
}

static char *opcodes[256];

static void objtrace_init_opcodes() {
    static char *unknown = "?";
    int i;

    for (i = 0; i < sizeof(zend_uchar); i++)
        opcodes[i] = unknown;

    opcodes[ZEND_NEW] = "new";
    opcodes[ZEND_ASSIGN] = "assign";
    opcodes[ZEND_ASSIGN_OBJ] = "assign_obj";
    opcodes[ZEND_POST_INC_OBJ] = "post_inc_obj";
    opcodes[ZEND_POST_DEC_OBJ] = "post_dec_obj";
    opcodes[ZEND_PRE_INC_OBJ] = "pre_inc_obj";
    opcodes[ZEND_PRE_DEC_OBJ] = "pre_dec_obj";
    opcodes[ZEND_UNSET_OBJ] = "unset_obj";
}

#ifdef COMPILE_DL_OBJTRACE
ZEND_GET_MODULE(objtrace)
#endif

#define YEX_T(offset) (*(temp_variable *)((char *) execute_data->Ts + offset))

static void objtrace_dump_zval(zval *o) {
    switch (Z_TYPE_P(o)) {
    case IS_NULL:
        fprintf(OTG(log), " NULL ");
        break;
    case IS_BOOL:
        fprintf(OTG(log), " Boolean: %s\n", Z_LVAL_P(o) ? "TRUE" : "FALSE");
        break;
    case IS_LONG:
        fprintf(OTG(log), " Long: %ld\n", Z_LVAL_P(o));
        break;
    case IS_DOUBLE:
        fprintf(OTG(log), " Double: %f\n", Z_DVAL_P(o));
        break;
    case IS_STRING:
        fprintf(OTG(log), " String: ");
        fprintf(OTG(log), "%s\n", Z_STRVAL_P(o));
        break;
    case IS_RESOURCE:
        fprintf(OTG(log), " Resource\n");
        break;
    case IS_ARRAY:
        fprintf(OTG(log), " Array\n");
        break;
    case IS_OBJECT: {
        HashTable *ht;
        Bucket *p;
        zval *v;
        zend_class_entry *ce = zend_get_class_entry(o TSRMLS_CC);
        fprintf(OTG(log), " Object [%s] ", ce->name);
        ht = o->value.obj.handlers->get_properties(o TSRMLS_CC);
        p = ht->pListHead;
        while (p != NULL) {
            fprintf(OTG(log), " %s:", p->arKey);
            v = zend_read_property(Z_OBJCE_P(o), o, p->arKey, strlen(p->arKey), 1 TSRMLS_CC);
            objtrace_dump_zval(v);
            p = p->pListNext;
        }
        fprintf(OTG(log), "\n");
        break;
    }
    default:
        fprintf(OTG(log), " Unknown\n");
    }
}

void objtrace_dump_var(ZEND_OPCODE_HANDLER_ARGS, zend_op *cur_opcode, znode *op) {
    zend_op_array *op_array = execute_data->op_array;

    if (op->op_type == IS_VAR) {
        zend_class_entry *ze = YEX_T(cur_opcode->op1.u.var).class_entry;
        if (ze) {
            fprintf(OTG(log), " %s() %s:%d-%d\n", ze->name, ze->filename, ze->line_start, ze->line_end);
        }
    }

    else if (op->op_type == IS_CV) {
        if (OTG(mode) & OBJTRACE_MODE_FLAG_DEBUG)
            fprintf(OTG(log), " cv: !%d -> $%s\n", op->u.var, op_array->vars[op->u.var].name);
        zval **ptr = zend_get_compiled_variable_value(execute_data, op->u.var);
        if (ptr != NULL)
            objtrace_dump_zval(*ptr);
    }
}

int objtrace_opcode_handler(ZEND_OPCODE_HANDLER_ARGS) {
    zend_op *cur_opcode;
    int lineno;
    char *file;
    objtrace_source *src = NULL;

    zend_op_array *op_array = execute_data->op_array;

    cur_opcode = *EG(opline_ptr);
    lineno = cur_opcode->lineno;

    file = (char *) op_array->filename;

    if (file) {
        int keyLen = strlen(file);

        if (zend_hash_find(OTG(sources), file, keyLen, (void **) &src) == FAILURE) {
            src = read_file(file);
            zend_hash_add(OTG(sources), file, keyLen, src, sizeof(objtrace_source), (void ** )&src);
        }
    }

    if (src) {
        fprintf(OTG(log), "\n%d: %s\n", lineno, src->lines[lineno]);
    }

    if (OTG(mode) & OBJTRACE_MODE_FLAG_DEBUG)
        fprintf(OTG(log), " %s\n", opcodes[cur_opcode->opcode]);

    if (OTG(mode) & OBJTRACE_MODE_FLAG_VALUES) {
        if (cur_opcode->op1.op_type != IS_UNUSED)
            objtrace_dump_var(execute_data, cur_opcode, &cur_opcode->op1);
        if (cur_opcode->op2.op_type != IS_UNUSED)
            objtrace_dump_var(execute_data, cur_opcode, &cur_opcode->op2);
    }

    return ZEND_USER_OPCODE_DISPATCH;
}

PHP_MINIT_FUNCTION(objtrace) {
    char fname[1024];

    ZEND_INIT_MODULE_GLOBALS(objtrace, php_objtrace_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    OTG(mode) = INI_INT("objtrace.mode");

    if (!(OTG(mode) & OBJTRACE_MODE_FLAG_ENABLED))
        return SUCCESS;

    sprintf(fname, "%s/%s", INI_STR("objtrace.trace_output_dir"), INI_STR("objtrace.trace_output_name"));
    OTG(log) = fopen(fname, "w");

    if (!OTG(log)) {
        fprintf(stderr, "Error opening %s\n", fname);
        exit(1);
    }

    objtrace_init_opcodes();

    zend_hash_init_ex(OTG(sources), 50, NULL,
            (dtor_func_t )objtrace_source_dtor, 1, 0);

    zend_set_user_opcode_handler(ZEND_NEW, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_ASSIGN, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_ASSIGN_OBJ, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_POST_INC_OBJ, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_PRE_INC_OBJ, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_POST_DEC_OBJ, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_PRE_DEC_OBJ, objtrace_opcode_handler);
    zend_set_user_opcode_handler(ZEND_UNSET_OBJ, objtrace_opcode_handler);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(objtrace) {
    UNREGISTER_INI_ENTRIES();

    if (OTG(log))
        fclose(OTG(log));

    return SUCCESS;
}

PHP_RINIT_FUNCTION(objtrace) {
    return SUCCESS;
}

PHP_FUNCTION(objtrace) {
    RETURN_INT(SUCCESS);
}

/*
 * Local Variables:
 * c-basic-offset: 4
 * tab-width: 4
 * End:
 * vim600: fdm=marker
 * vim: noet sw=4 ts=4
 */
