#!/bin/bash
#
# autogen.sh - script to generate autotools in a dir
#
# Copyright 2020 Robert L (Bob) Parker rlp1938@gmail.com
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.# See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
# MA 02110-1301, USA.
#

# pre-req 'sudo apt install -y autotools-dev autoconf
# template files required INSTALL NEWS README AUTHORS Changelog
# just allow 'COPYING' to be what it is.
autoscan
mv configure.scan configure.ac
# customise it for this project
sed -i 's/FULL-PACKAGE-NAME/filedups/' configure.ac
sed -i 's/VERSION/1.0/' configure.ac
sed -i 's/BUG-REPORT-ADDRESS/rlp1938@gmail.com/' configure.ac
sed -i '/AC_CONFIG_SRCDIR/ i : ${CFLAGS=""}' configure.ac
sed -i '/AC_CONFIG_SRCDIR/ i AM_INIT_AUTOMAKE' configure.ac
autoheader
aclocal
automake --add-missing --copy
autoconf
