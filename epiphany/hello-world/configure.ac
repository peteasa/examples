## mandatory to have AC_INIT
AC_INIT([hello-world-all], [1.0], [peteasa@gmail.com])

AC_ARG_ENABLE([epiphany],
    AS_HELP_STRING([--disable-epiphany],
        [Disable epiphany build]) )

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

## Check if cross compiling in poky-sdk
AS_IF([test "x$CONFIGURE_FLAGS" != "x"],
    [
        testforhost="`echo $ac_configure_args | sed 's|.*--host=.*|yes|'`"
        AS_IF([test "x$testforhost" != "xyes"],
            [
                AC_SUBST( ac_configure_args, $ac_configure_args$CONFIGURE_FLAGS )

                $as_echo "Configure: Added CONFIGURE_FLAGS to ac_configure_args $ac_configure_args " >&6
            ])
    ])

AC_CONFIG_FILES(
    [Makefile]
)

## Now configure in the subsystem folder
AC_CONFIG_SUBDIRS(
    [sub/host]
    [sub/devices]
)

## mandatory to have AC_OUTPUT
AC_OUTPUT
