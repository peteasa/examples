## Ref https://www.gnu.org/software/automake/manual/html_node/Subdirectories.html
## Specify the order that the sub directories are built in.  Note the order is important
## first the library has to be built then the things that depend on it
## In this case library, main then the top level (because of the dot) then the tests
## subdirectory is built
#SUBDIRS = library main . tests
SUBDIRS = library main
#dist_doc_DATA = README

## dont need to add this as it is added by autoreconf ACLOCAL_AMFLAGS = -I config/m4
