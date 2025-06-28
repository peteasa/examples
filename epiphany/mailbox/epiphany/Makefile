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
SRCS		= e_mailbox.c

# Base name of the final target- choose the name with care!
TARGET_BASE	= e_mailbox

# List of include search paths
INCLUDES	= -I ${EPIPHANY_HOME}/include

# List of libraries
LIB_PATHS   	= -B ${EPIPHANY_HOME}/lib -B ${EPIPHANY_HOME}/lib/epiphany-elf/11.3.0
LIBS        	= -le-lib

# Build the names of the elf files
ELFS		= $(SRCS:.c=.elf)

# base location for release
prefix ?= $(EPIPHANY_HOME)/usr

# location of the final target
TARGET_NAME 	= $(TARGET_BASE).elf
TARGET_LOCATION = $(prefix)/epiphany/bin
TARGET 		= $(TARGET_LOCATION)/$(TARGET_NAME)

# locations for compiler and linker rules
exec_prefix ?= $(prefix)
includedir ?= $(exec_prefix)/include
libdir ?= $(exec_prefix)/lib
bindir ?= $(exec_prefix)/bin

CROSS_COMPILE=$(EXOTIC_TARGET_SYS)-
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
CFLAGS          = $(INCLUDES) $(LIB_PATHS) $(LIBS)
CXXFLAGS    	= $(CFLAGS)
LDFLAGS		= -T /usr/share/epiphany/bsps/current/fast.ldf

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
