
.PHONY : clean uninstall
 
prefix ?= /usr
exec_prefix ?= $(prefix)
libdir ?= $(exec_prefix)/lib
 
XDMA_LIB_src = libxdma.c
XDMA_LIB_lib = libxdma.so
XDMA_LIB_tgt = $(DESTDIR)$(libdir)/$(XDMA_LIB_lib)

INCLUDES = -I. -I../xdma_dev

# To switch off Optimization use
# CFLAGS := $(filter-out -O2,$(CFLAGS))
# CFLAGS += -O0
# To output the pre-processor and macro expansion output use
# https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
# CFLAGS = -E -dD

DEBUG_FLAGS = -ggdb3 -DDEBUG
RELEASE_FLAGS = -O3

CFLAGS += -fPIC -Wall -Wextra -c
LDFLAGS += -shared -Wl,-soname,$(XDMA_LIB_lib)

CFLAGS += $(DEBUG_FLAGS)

all : $(XDMA_LIB_lib)
 
$(XDMA_LIB_lib) : $(XDMA_LIB_src)
	$(CC) $(CFLAGS) $(INCLUDES) *.c
	$(CC) $(LDFLAGS) -o $(XDMA_LIB_lib) *.o
	
$(XDMA_LIB_tgt) : $(XDMA_LIB_lib)
	install -d $(DESTDIR)$(libdir)
	install -m 0755 $^ $@
 
clean :
	rm $(XDMA_LIB_lib)
 
install : $(XDMA_LIB_tgt)
 
uninstall :
	rm $(XDMA_LIB_tgt)
