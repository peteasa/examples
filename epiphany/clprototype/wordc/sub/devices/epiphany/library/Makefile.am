##
## $(top_srcdir) points to the location of the configure.ac configure script -
##     wordc/sub/devices/epiphany
## $(top_srcdir)/Makefile.am contains the SUBDIR names and the Makefile
## is added to the AC_CONFIG_FILES in $(top_srcdir)/configure.ac so
## $(top_srcdir)/library/Makefile.am is used to create Makefile.in 
## The actual source location relative to $(top_srcdir)/library is
##     ../../../../src/library/src
## $(top_builddir) points to the location that the configure script was run in
##     wordc/build/sub/devices/epiphany
## The Makefile is created with the same SUBDIR names so
## $(top_builddir)/library/Makefile is used to build the source
##
## Given that the actual source is located $(top_srcdir)/library/$(R)
## Then the same relative position is also used in the build tree
## $(top_srcdir)/library/$(R) is the location of the actual source files
## $(top_builddir)/library/$(R) is the location of the corresponding build product
## In this case
##     $(R) == ../../../../src/library/src/
##

lib_LIBRARIES = libecomms.a
libecomms_a_SOURCES = \
	../../../../src/library/src/ebase.c \
	../../../../src/library/include/ebase.h \
	../../../../src/library/include/emsgbase.h \
	../../../../src/library/src/echnmem.c \
	../../../../src/library/include/echnmem.h \
	../../../../src/library/src/uchnmem.c \
	../../../../src/library/include/uchnmem.h \
	../../../../src/library/src/ecommsbase.c \
	../../../../src/library/include/ecommsbase.h \
	../../../../src/library/src/uchannel.c \
	../../../../src/library/include/uchannel.h \
	../../../../src/library/src/eccomnmem.c \
	../../../../src/library/include/eccomnmem.h \
	../../../../src/library/src/ecommand.c \
	../../../../src/library/include/ecommand.h \
	../../../../src/library/include/libecomms.h

#lib_LTLIBRARIES = libecomms.la
#libecomms_la_SOURCES = 

## The include file location is passed to the C Pre Processor
## using the full path
libecomms_a_CPPFLAGS = -I $(top_srcdir)/../../../src/library/include
