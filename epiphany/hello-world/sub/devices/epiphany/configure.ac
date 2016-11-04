AC_INIT([hello-world-epiphany], [1.0], [peteasa@gmail.com], [], [http://www.github.com/peteasa/parallella/wiki])

AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])

## in poky SDK allow the configure tools to think that we are cross compiling for arm
## setting the correct compilers fails for some reason if we do the right thing
AC_PROG_CC

## Test to see if running in the SDK environment
AS_IF([test "x$SDKTARGETSYSROOT" != "x"],
    [
        ## now override the tools set by the poky sdk
        AC_SUBST(CC, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-gcc)
        AC_SUBST(CXX, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-g++)
        AC_SUBST(CPP, "$OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-gcc -E")
        AC_SUBST(TARGET_PREFIX, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-)
        AC_SUBST(AS, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-as)
        AC_SUBST(AR, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-ar)
        AC_SUBST(NM, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-nm)
        AC_SUBST(LD, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-ld)
        AC_SUBST(STRIP, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-strip)
        AC_SUBST(OBJCOPY, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-objcopy)
        AC_SUBST(RANLIB, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-ranlib)
        AC_SUBST(OBJDUMP, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-objdump)
        AC_SUBST(CROSS_COMPILE, $OECORE_NATIVE_SYSROOT/usr/bin/epiphany-elf/epiphany-elf-)

        ## following might be different for each build are set in the Makefile
        ## these override values set by the poky sdk
        AC_SUBST(CFLAGS, "")
        AC_SUBST(CXXFLAGS, "")
        AC_SUBST(LDFLAGS, "")
        # LIBS

        ## Recheck for compiler now that we have the right one
        ## First remove the cached value so that the system does not use it
        $as_unset ac_cv_prog_CC
        AC_PROG_CC
    ])

## Now test that the correct tools are in place
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_RANLIB
AM_PROG_AR
AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

AC_CONFIG_FILES([
    Makefile
    build/Makefile
])

## mandatory to have AC_OUTPUT
AC_OUTPUT
