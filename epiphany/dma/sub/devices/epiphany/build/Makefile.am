##Ref: https://www.gnu.org/software/automake/manual/html_node/amhello_0027s-Makefile_002eam-Setup-Explained.html#amhello_0027s-Makefile_002eam-Setup-Explained

## bin ie install in bindir see https://www.gnu.org/software/automake/manual/html_node/Standard-Directory-Variables.html#Standard-Directory-Variables
bin_PROGRAMS = e_dma

##
## $(top_srcdir) points to the location of the configure.ac configure script -
##     dma/sub/devices/epiphany
## $(top_srcdir)/Makefile.am contains the SUBDIR names and the Makefile
## is added to the AC_CONFIG_FILES in $(top_srcdir)/configure.ac so
## $(top_srcdir)/build/Makefile.am is used to create Makefile.in 
## The actual source location relative to $(top_srcdir)/build is
##     ../../../../src/devices/epiphany/src
## $(top_builddir) points to the location that the configure script was run in
##     dma/build/sub/devices/epiphany
## The Makefile is created with the same SUBDIR names so
## $(top_builddir)/build/Makefile is used to build the source
##
## Given that the actual source is located $(top_srcdir)/build/$(R)
## Then the same relative position is also used in the build tree
## $(top_srcdir)/build/$(R) is the location of the actual source files
## $(top_builddir)/build/$(R) is the location of the corresponding build product
## In this case
##     $(R) == ../../../../src/devices/epiphany/src/
##
e_dma_SOURCES = ../../../../src/devices/epiphany/src/e_dma.c

## http://www.gnu.org/software/autoconf/manual/automake.html#Flag-Variables-Ordering
## first either AM_CFLAGS or e_dma_CFLAGS will be used..
## the e_dma_ is specific to e_dma_SOURCES
## second CFLAGS is always used last so user overrides
## here we send the include file location to the C Pre Processor
## using the full path
e_dma_CPPFLAGS = -I $(top_srcdir)/../../../src/library/include

## When building outside the source tree previous build products
## are also in the build tree
##
#LDADD = $(top_builddir)/library/libecomms.a

## may need to modify the following for specific applications
## $(OECORE_TARGET_SYSROOT) = '' when on target. In yocto it is set
# to get a map file -Wl,-Map=e_dma.map,--cref
# to get verbose linker output -Wl,--verbose
# to get detailed output from the compiler -v
# Not sure why these are needed --param ggc-min-expand=96 --param ggc-min-heapsize=123747 -flto 
AM_LDFLAGS=-T $(EPIPHANY_HOME)/bsps/current/fast.ldf
AM_CFLAGS=-O2 -Wall -Wextra -isystem $(OECORE_TARGET_SYSROOT)/usr/epiphany-elf/include
AM_CXXFLAGS=$(AM_CFLAGS)

LIBS = \
	-le-lib
