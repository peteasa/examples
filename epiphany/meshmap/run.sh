#!/bin/sh

#dumping disassembly
epiphany-elf-objdump -D $EPIPHANY_HOME/usr/epiphany/bin/e_mesh_map.elf > DUMP

/usr/epiphany/bin/mesh_map $EPIPHANY_HOME/usr/epiphany/bin/e_mesh_map.elf

