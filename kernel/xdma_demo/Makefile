
.PHONY : clean uninstall
 
prefix ?= /usr
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
 
XDMA_DEMO_APP_src = xdma-app.c
XDMA_DEMO_APP_bin = app
XDMA_DEMO_APP_tgt = $(DESTDIR)$(bindir)/$(XDMA_DEMO_APP_bin)
XDMA_DEMO_TEST_src = xdma-test.c
XDMA_DEMO_TEST_bin = test
XDMA_DEMO_TEST_tgt = $(DESTDIR)$(bindir)/$(XDMA_DEMO_TEST_bin)
XDMA_DEMO_DEMO_src = xdma-demo.c
XDMA_DEMO_DEMO_bin = demo
XDMA_DEMO_DEMO_tgt = $(DESTDIR)$(bindir)/$(XDMA_DEMO_DEMO_bin)

INCLUDES = -I. -I../xdma_lib -I../xdma_dev

# CFLAGS += -v
LDFLAGS += -L../xdma_lib -lxdma

# To switch off Optimization use
# CFLAGS := $(filter-out -O2,$(CFLAGS))
# CFLAGS += -O0
# To output the pre-processor and macro expansion output use
# https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
# CFLAGS = -E -dD

all : $(XDMA_DEMO_APP_bin) $(XDMA_DEMO_TEST_bin) $(XDMA_DEMO_DEMO_bin)
 
$(XDMA_DEMO_APP_bin) : $(XDMA_DEMO_APP_src)
	$(CC) $(CFLAGS) $(INCLUDES) $(XDMA_DEMO_APP_src) -o $(XDMA_DEMO_APP_bin) $(LDFLAGS)
	
$(XDMA_DEMO_TEST_bin) : $(XDMA_DEMO_TEST_src)
	$(CC) $(CFLAGS) $(INCLUDES) $(XDMA_DEMO_TEST_src) -o $(XDMA_DEMO_TEST_bin) $(LDFLAGS)
	
$(XDMA_DEMO_DEMO_bin) : $(XDMA_DEMO_DEMO_src)
	$(CC) $(CFLAGS) $(INCLUDES) $(XDMA_DEMO_DEMO_src) -o $(XDMA_DEMO_DEMO_bin) $(LDFLAGS)
	
$(XDMA_DEMO_APP_tgt) : $(XDMA_DEMO_APP_bin)
	install -d $(DESTDIR)$(bindir)
	install -m 0755 $^ $@
	
$(XDMA_DEMO_TEST_tgt) : $(XDMA_DEMO_TEST_bin)
	install -d $(DESTDIR)$(bindir)
	install -m 0755 $^ $@
	
$(XDMA_DEMO_DEMO_tgt) : $(XDMA_DEMO_DEMO_bin)
	install -d $(DESTDIR)$(bindir)
	install -m 0755 $^ $@
	
clean :
	rm $(XDMA_DEMO_APP_bin) $(XDMA_DEMO_TEST_bin) $(XDMA_DEMO_DEMO_bin)
 
install : $(XDMA_DEMO_APP_tgt) $(XDMA_DEMO_TEST_tgt) $(XDMA_DEMO_DEMO_tgt)
 
uninstall :
	rm $(XDMA_DEMO_APP_tgt) $(XDMA_DEMO_TEST_tgt) $(XDMA_DEMO_DEMO_tgt)
