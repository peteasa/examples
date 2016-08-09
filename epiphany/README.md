# Based on https://github.com/adapteva/epiphany-examples

I keep a modified copy of these sources here because I have fixed bugs and modified the make process for use with Yocto.

### files and folders:

fpga/bitstreams: - bitstreams created from parallella-fpga source that include support for hdmi
kernel: - examples of kernel drivers
epiphany: - examples of applications that run on Epiphany

epiphany/setup-epiphany-sdk-in-poky-sdk.sh - a shell script to setup the build environment for builds with epiphany-elf-gcc on a build machine with poky-sdk installed
epiphany/setup-epiphany-sdk-on-target.sh - the equivalent script to setup the build environment with epiphany-elf-gcc on the target
epiphany/build-pal.sh - a shell script to load and build pal libraries on target machine
epiphany/0001-pal-symbolmangling.patch - patch file used when compiling with older v4.8 epiphany-elf-gcc
epiphany/0002-pal-matmul-dev-epiphany-linker.patch - patch file used when compiling with older v4.8 epiphany-elf-gcc

