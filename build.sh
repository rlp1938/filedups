#!/usr/bin/env bash
#
# build.sh - script to build a group of programs.
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
gcc -Wall -Wextra -O0 -g -c filedups.c
gcc -Wall -Wextra -O0 -g -c calcmd5.c
gcc -Wall -Wextra -O0 -g -c dirs.c
gcc -Wall -Wextra -O0 -g -c files.c
gcc -Wall -Wextra -O0 -g -c str.c
gcc -Wall -Wextra -O0 -g -c firstrun.c
gcc -Wall -Wextra -O0 -g -c gopt.c
gcc filedups.o calcmd5.o dirs.o files.o str.o firstrun.o gopt.o \
-lmhash -o filedups

#gcc -Wall -Wextra -O0 -g -o procdups procdups.c

