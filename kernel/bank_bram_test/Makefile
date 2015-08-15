
.PHONY : clean uninstall
 
prefix ?= /usr
exec_prefix ?= $(prefix)
bindir ?= $(exec_prefix)/bin
 
BANK_BRAM_TEST_src = bank_bram_test.c
BANK_BRAM_TEST_bin = bank_bram_test
BANK_BRAM_TEST_tgt = $(DESTDIR)$(bindir)/$(BANK_BRAM_TEST_bin)

# To switch off Optimization use
# CFLAGS := $(filter-out -O2,$(CFLAGS))
# CFLAGS += -O0
# To output the pre-processor and macro expansion output use
# https://gcc.gnu.org/onlinedocs/cpp/Preprocessor-Output.html
# CFLAGS = -E -dD

all : $(BANK_BRAM_TEST_bin)
 
$(BANK_BRAM_TEST_bin) : $(BANK_BRAM_TEST_src)
 
$(BANK_BRAM_TEST_tgt) : $(BANK_BRAM_TEST_bin)
	install -d $(DESTDIR)$(bindir)
	install -m 0755 $^ $@
 
clean :
	rm $(BANK_BRAM_TEST_bin)
 
install : $(BANK_BRAM_TEST_tgt)
 
uninstall :
	rm $(BANK_BRAM_TEST_tgt)
