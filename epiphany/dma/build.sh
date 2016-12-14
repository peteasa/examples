#!/bin/sh

TOPDIR="$( cd "$( dirname "$0" )" && pwd )"

buildit ()
{
    mkdir -p $TOPDIR/build/$1
    cd $1
    autoreconf -i --force

    cd $TOPDIR/build/$1
    $TOPDIR/$1/configure --prefix=$TOPDIR/local
    make install
    cd $TOPDIR
}

#buildit devices

#buildit host

buildit .

