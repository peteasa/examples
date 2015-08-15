#################################################################
#
# Example make file to build host part of a project
# Note the install rule will install the TARGET in 
# $(OECORE_TARGET_SYSROOT)/usr/epiphany/bin
#
# Dont use a conflicting name with an existing component!
#
#################################################################

# names of all source files
SRCS 		= hello_world.c

# name of the final target- choose the name with care!
TARGET_BASE	= hello_world

# List of include search paths
INCLUDES    	= 

# List of libraries
LIB_PATHS   	= 
LIBS        	= \
	-le-hal \
	-le-loader

# base location for release
prefix ?= $(OECORE_NATIVE_SYSROOT)/usr

# location of the final target
TARGET_LOCATION = $(prefix)/epiphany/bin
TARGET = $(TARGET_LOCATION)/$(TARGET_BASE)

# locations for compiler and linker rules
exec_prefix ?= $(prefix)
includedir ?= $(exec_prefix)/include
libdir ?= $(exec_prefix)/lib
bindir ?= $(exec_prefix)/bin

# Ref: https://www.gnu.org/software/make/manual/html_node/Options-Summary.html
# To debug try:-  make -d
# To see all the rules:-  make --print-data-base
# To display but not execute:-  make --print-data-base

# To switch off Optimization use
# CFLAGS := $(filter-out -O2,$(CFLAGS))
# CFLAGS += -O0
# To output the pre-processor and macro expansion output use
# https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
# CFLAGS = -E -dD

EXTRACFLAGS  	= 
CXXFLAGS 	+= 
CFLAGS 		+= $(LIB_PATHS) $(LIBS)
LDFLAGS		+=

.PHONY : clean uninstall

all : $(TARGET)

$(TARGET_BASE) : $(SRCS)

$(TARGET) : $(TARGET_BASE)
	install -d $(TARGET_LOCATION)
	install -m 0755 $^ $@

clean :
	rm -f $(TARGET_BASE)

install : $(TARGET)

uninstall :
	rm -f $(TARGET)
