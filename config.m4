
PHP_ARG_ENABLE(objtrace, whether to enable objtrace support,
[ --enable-objtrace      Enable objtrace support])

if test "$PHP_OBJTRACE" = "yes"; then
  AC_DEFINE(HAVE_OBJTRACE, 1, [Whether you have ObjTrace])
  PHP_NEW_EXTENSION(objtrace, objtrace.c, $ext_shared)
fi
