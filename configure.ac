#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([filedups], [1.0], [rlp1938@gmail.com])
: ${CFLAGS=""}
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([str.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memchr memmove memset mkdir realpath strchr strdup])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
