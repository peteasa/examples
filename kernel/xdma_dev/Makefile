obj-m += xdma.o

SRC := $(shell pwd)

# To switch off Optimization use
# xdma.o = -O0
# To output the pre-processor and macro expansion output use
# https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
# CFLAGS_xdma.o = -E -dD

ifndef $(KERNEL_SRC)
	KERNEL_SRC = $(OECORE_TARGET_SYSROOT)/usr/src/kernel
endif

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install	

clean:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) clean

