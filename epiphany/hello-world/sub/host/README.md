To build this sub project run the following commands, either in tree or in a separate build folder:

$ autoreconf -i --force
$ $TOPDIR/sub/host/configure --prefix=$TOPDIR/local
$ make > make.log 2>&1 ; make install > make-install.log 2>&1

See configure --help for other options
