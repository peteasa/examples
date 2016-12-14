AC_INIT([dma-devices], [1.0], [peteasa@gmail.com], [], [http://www.github.com/peteasa/parallella/wiki])

AC_ARG_ENABLE([epiphany],
    AS_HELP_STRING([--disable-epiphany],
        [Disable epiphany build]) )

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])

AC_CONFIG_FILES(
    [Makefile]
)

## Now configure in the device
AS_IF([test "x$enable_epiphany" != "xno"],
    [
        ## Test for running on target without setting --host
        testforhost="`echo $ac_configure_args | sed 's|.*--host=.*|yes|'`"
        AS_IF([test "x$testforhost" != "xyes"],
            [
                epiphany=" --host=epiphany-elf"
                AC_SUBST( ac_configure_args, $ac_configure_args$epiphany )

                $as_echo "Configure: Added --host to ac_configure_args $ac_configure_args " >&6
            ])

        ## Test to see if running in the SDK environment
        AS_IF([test "x$SDKTARGETSYSROOT" != "x"],
            [
                sdktarget="`echo $ac_configure_args | sed "s|.*--host=\(\S*\)'.*|\1|"`"
                subepiphany="`echo $ac_configure_args | sed 's|='"$sdktarget"'|=epiphany-elf|g'`"
                AC_SUBST( ac_configure_args, $subepiphany )

                $as_echo "Configure: for =$sdktarget substituted =epiphany-elf " >&6
            ])

        $as_echo "Configure: ac_configure_args $ac_configure_args " >&6

        AC_CONFIG_SUBDIRS(
                            [epiphany]
                         )
    ])


## mandatory to have AC_OUTPUT
AC_OUTPUT
