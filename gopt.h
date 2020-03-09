/*      gopt.h
 *
 *  Copyright 2020 Robert (Bob) L Parker rlp1938@gmail.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
*/

#ifndef GOPT_H
#define GOPT_H
#include "str.h"
char *optstring;

typedef struct options_t {
  int     runhelp; // flag, dohelp(0)
  int     runvsn;  // flag, dovsn()
  int     pages;   // num
  size_t  dat_size; // data block size.
  size_t  dat_incr; // size to increase data space by.
} options_t;


options_t process_options(int argc, char **argv);

#endif
