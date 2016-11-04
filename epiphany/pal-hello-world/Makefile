

DIRS =	epiphany \
	host

.PHONY: all $(DIRS)
.PHONY: clean $(DIRS)
.PHONY: install $(DIRS)

all: $(DIRS)

clean: $(DIRS)

install: $(DIRS)

uninstall: $(DIRS)

$(DIRS):
	@echo "**************************************************"
	@echo Building target: $@
	@echo Goals: $(MAKECMDGOALS)
	@echo "**************************************************"
	$(MAKE) -C $@ $(MAKECMDGOALS)
