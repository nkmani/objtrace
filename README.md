objtrace
========

PHP extension to trace objects. Since this is currently configured as an extension (and not a zend_extension) not all of the opcode hooks work. If none of the other zend_extensions (like xdebug) are disabled, it works fine.

To run:
  Enable the extension
  Set flag values in a ini file or specify them in command line
  Trace log is placed in /tmp/objtrace.ot (default; can be changed through ini settings)
  
