##Ref: https://www.gnu.org/software/automake/manual/html_node/amhello_0027s-Makefile_002eam-Setup-Explained.html#amhello_0027s-Makefile_002eam-Setup-Explained

## bin ie install in bindir see https://www.gnu.org/software/automake/manual/html_node/Standard-Directory-Variables.html#Standard-Directory-Variables
bin_PROGRAMS = hello_world
hello_world_SOURCES = ../../../src/main/src/hello_world.c

## $(top_srcdir) is the location of the corresponding configuration file
## In this design the configuration and build files are kept separate to the
## source tree to keep clean of build artifacts
## When building outside the source tree gcc needs to know where the
## sources are relative to $(top_srcdir) so use ../../../src
## http://www.gnu.org/software/autoconf/manual/automake.html#Flag-Variables-Ordering
## first either AM_CFLAGS or hello_world_CFLAGS will be used..
## the hello_world_ is specific to hello_world_SOURCES
## second CFLAGS is always used last so user overrides
## here we send the include file location to the C Pre Processor
## using the full path
#hello_world_CPPFLAGS = -I $(top_srcdir)../../../src/library/include

## When building outside the source tree previous build products
## are also in the build tree
## TODO find reference
#LDADD = $(top_builddir)/library/libamlib.la

LIBS = \
	-le-hal \
	-le-loader \
	-lpthread
