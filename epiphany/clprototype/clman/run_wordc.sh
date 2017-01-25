#!/bin/sh

export EPIPHANY_HOME=/usr/epiphany/epiphany-sdk
export EPIPHANY_HDF=${EPIPHANY_HOME}/bsps/current/platform.hdf
export LD_LIBRARY_PATH=/usr/epiphany/lib:/usr/local/lib:/usr/lib/epiphany-elf:/usr/lib:${LD_LIBRARY_PATH}
export PATH=./:/usr/epiphany/bin:${PATH}

wordc -e $1

