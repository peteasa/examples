##Ref: https://www.gnu.org/software/automake/manual/html_node/amhello_0027s-Makefile_002eam-Setup-Explained.html#amhello_0027s-Makefile_002eam-Setup-Explained

## bin ie install in bindir see https://www.gnu.org/software/automake/manual/html_node/Standard-Directory-Variables.html#Standard-Directory-Variables
bin_PROGRAMS = dma

##
## $(top_srcdir) points to the location of the configure.ac configure script -
##     dma/sub/host
## $(top_srcdir)/Makefile.am contains the SUBDIR names so
## $(top_srcdir)/main/Makefile.am is used to create Makefile.in 
## The actual source location relative to $(top_srcdir)/main is
##     ../../../src/main/src
## $(top_builddir) points to the location that the configure script was run in
##     dma/build/sub/host
## The Makefile is created with the same SUBDIR names so
## $(top_builddir)/main/Makefile is used to build the source
##
## Given that the actual source is located $(top_srcdir)/main/$(R)
## Then the same relative position is also used in the build tree
## $(top_srcdir)/main/$(R) is the location of the actual source files
## $(top_builddir)/main/$(R) is the location of the corresponding build product
## In this case
##     $(R) == ../../../src/main/src/
##
dma_SOURCES = ../../../src/main/src/dma.c

## http://www.gnu.org/software/autoconf/manual/automake.html#Flag-Variables-Ordering
## first either AM_CFLAGS or dma_CFLAGS will be used..
## the dma_ is specific dma_SOURCES
## second CFLAGS is always used last so user overrides
## here we send the include file location to the C Pre Processor
## using the full path
dma_CPPFLAGS = -I $(top_srcdir)/../../src/library/include

## When building outside the source tree previous build products
## are also in the build tree
## TODO find reference
LDADD = $(top_builddir)/library/libecomms.la

LIBS = \
	-le-hal \
	-le-loader \
	-lpthread

