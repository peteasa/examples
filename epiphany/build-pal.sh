#!/bin/sh
#
# This script should build pal libraries on the target.
# Note that pal is rapidly changing so it may not work with the latest version of pal
#

if [[ $_ = $0 ]]
then
  echo "ERROR: to set the environment variables and build pal run"
  echo "  source build-pal.sh"
  [[ $PS1 ]] && return || exit;
fi

cd ~
git clone https://github.com/parallella/pal.git
cd pal
git checkout master

# Now apply patches to handle the old compiler that I use for epiphany-elf-gcc
# Note if you are using the released sdk from the parallella site dont apply these
git apply 0001-pal-symbolmangling.patch
git apply 0002-pal-matmul-dev-epiphany-linker.patch

#
# Now run autotools and configure
#
autoreconf -i --force
./configure --enable-device-epiphany

make -k > make.log 2>&1
make install > make-install.log 2>&1

echo "WARNING: Check output of make process in make.log and make-install.log to check it worked!"
echo "If all has gone well try out the pal-hello-world example"
