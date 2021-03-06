#################################################################
#
# Example make file to build epiphany part of a project
# Note the install rule will install the TARGET in 
# $(OECORE_TARGET_SYSROOT)/usr/epiphany/bin
#
# Dont use a conflicting name with an existing component!
#
#################################################################

# SRC file names
SRCS		= hello_task.c

# Base name of the final target- choose the name with care!
TARGET_BASE	= hello_task

# List of include search paths
INCLUDES    	= \
	-I/usr/local/epiphany-elf/include/pal \
	-I../include

# List of libraries
LIB_PATHS   	= -Wl,-L/usr/local/epiphany-elf/lib
LIBS        	= -lpal -le-lib

# Build the names of the elf files
ELFS		= $(SRCS:.c=.elf)

# base location for release
prefix ?= $(OECORE_NATIVE_SYSROOT)/usr

# location of the final target
TARGET_NAME 	= $(TARGET_BASE).elf
TARGET_LOCATION = $(prefix)/epiphany/bin
TARGET 		= $(TARGET_LOCATION)/$(TARGET_NAME)

# locations for compiler and linker rules
exec_prefix ?= $(prefix)
includedir ?= $(exec_prefix)/$(EXOTIC_TARGET_SYS)/include
libdir ?= $(exec_prefix)/$(EXOTIC_TARGET_SYS)/lib
bindir ?= $(exec_prefix)/bin/$(EXOTIC_TARGET_SYS)

CROSS_COMPILE=$(bindir)/$(EXOTIC_TARGET_PREFIX)
CXX = $(CROSS_COMPILE)g++
CC  = $(CROSS_COMPILE)gcc
AR  = $(CROSS_COMPILE)ar
LD  = $(CROSS_COMPILE)ld
AS  = $(CROSS_COMPILE)as
OBJCPY = $(CROSS_COMPILE)objcopy

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

## Override the flags for an epiphany build
EXTRACFLAGS  	= 
CFLAGS          = $(LIB_PATHS) $(LIBS) $(INCLUDES)
CXXFLAGS    	= $(CFLAGS)
LDFLAGS		= -T $(EPIPHANY_HOME)/bsps/current/fast.ldf

.PHONY : clean uninstall

# Rule for building .elf files
%.elf: %.c
	$(CC) $(LDFLAGS) $< -o $@ ${CFLAGS}

all : $(TARGET)

clean : uninstall
	rm -f $(ELFS)

$(TARGET) : $(TARGET_NAME)
	install -d $(TARGET_LOCATION)
	install -m 0644 $^ $@

install : $(TARGET)

uninstall :
	rm -f $(TARGET)
