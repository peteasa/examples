#!/bin/sh

#dumping disassembly
epiphany-elf-objdump -D $EPIPHANY_HOME/usr/epiphany/bin/e_mutex.elf > DUMP

/usr/epiphany/bin/mutex $EPIPHANY_HOME/usr/epiphany/bin/e_mutex.elf

