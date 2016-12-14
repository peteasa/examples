##Ref: https://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.69/html_node/Writing-Autoconf-Input.html#Writing-Autoconf-Input

## Suggestion run autoscan to make suggestions for how to add content to this file https://www.gnu.org/savannah-checkouts/gnu/autoconf/manual/autoconf-2.69/html_node/autoscan-Invocation.html#autoscan-Invocation

## mandatory to have AC_INIT
AC_INIT([dma-host], [1.0], [peteasa@gmail.com], [], [http://www.github.com/peteasa/parallella/wiki])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
## dont need AC_PROG_RANLIB if LT_INIT used
AM_PROG_AR

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

## Add configuration for library creation
## TODO find doc for this
LT_INIT

## TODO find doc for m4 macros
AC_CONFIG_MACRO_DIRS([config/m4])

AC_CONFIG_FILES([
    Makefile
    library/Makefile
    main/Makefile
])

## mandatory to have AC_OUTPUT
AC_OUTPUT
