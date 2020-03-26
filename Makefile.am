
#AM_CFLAGS=-Wall -Wextra -O2 -D_GNU_SOURCE=1
# Set up initially to use GDB, change to optimised afterward.
AM_CFLAGS=-Wall -Wextra -g -O0 -D_GNU_SOURCE=1

bin_PROGRAMS=filedups procdups
AM_LDFLAGS=-lmhash

filedups_SOURCES=filedups.c dirs.c dirs.h files.c files.h str.c \
str.h firstrun.h firstrun.c gopt.h gopt.c calcmd5.h calcmd5.c
filedups_LDADD=-lmhash

procdups_SOURCES=
man_MANS=filedups.1

# next lines to be hand edited
# send <whatever> to $(prefix)/share/
newdir=$(datadir)/filedups
new_DATA=dname_grep.cfg
# ensure that filedups.1 and any other config files get put in the
# tarball. Also stops `make distcheck` bringing an error.
EXTRA_DIST=filedups.1 dname_grep.cfg
