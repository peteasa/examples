To build this project decide where you want the build products for example:

$ TOPDIR="$( cd "$( dirname "$0" )" && pwd )"
$ mkdir -p $TOPDIR/build

Prepare the configure scripts in each of the sub projects:

$ autoreconf -i --force

Decide where in the system to install the bin files and configure the build system for example:

$ cd $TOPDIR/build
$ $TOPDIR/configure --prefix=$TOPDIR/local
$ make > make.log 2>&1 ; make install > make-install.log 2>&1

There are various sub projects in this project and each can be built separately in a similar way. 

See configure --help for other options
