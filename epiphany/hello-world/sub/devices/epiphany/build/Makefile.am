bin_PROGRAMS = e_hello_world
e_hello_world_SOURCES = ../../../../src/devices/epiphany/src/e_hello_world.c

## $(top_srcdir) is the location of the corresponding configuration file
## this is the location of the sub/devices/epiphany/build folder
## In this design the configuration and build files are kept separate to the
## source tree to keep clean of build artifacts
## When building outside the source tree gcc needs to know where the
## sources are relative to $(top_srcdir) so use ../../../../src
## http://www.gnu.org/software/autoconf/manual/automake.html#Flag-Variables-Ordering
## first either AM_CFLAGS or e_hello_world_CFLAGS will be used..
## the e_hello_world_ is specific to e_hello_world_SOURCES
## second CFLAGS is always used last so user overrides
## here we send the include file location to the C Pre Processor
## using the full path
#e_hello_world_CPPFLAGS = -I $(top_srcdir)../../../../src/library/include

## When building outside the source tree previous build products
## are also in the build tree
## TODO find reference
#LDADD = $(top_builddir)/../../library/libamlib.la

## may need to modify the following for specific applications
## $(OECORE_TARGET_SYSROOT) = '' when on target. In yocto it is set
AM_LDFLAGS=-T $(EPIPHANY_HOME)/bsps/current/fast.ldf
AM_CFLAGS=-Wall -Wextra -isystem $(OECORE_TARGET_SYSROOT)/usr/epiphany-elf/include
AM_CXXFLAGS=$(AM_CFLAGS)

LIBS = \
	-le-lib
