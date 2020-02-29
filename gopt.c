/*      gopt.c
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
#include "str.h"
#include "files.h"
#include "gopt.h"


options_t process_options(int argc, char **argv)
{
  optstring = ":hvp:";  // initialise

  options_t opts = {0}; // will clang bitch?
  // add any non-zero, non-NULL default values.
  opts.pages = 1;
  int c;

  while(1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
    {"help",  0,  0,  'h' },
    {"version",  0,  0,  'v' },
    {"pages",  0,  0,  'p' },
    {0,  0,  0,  0 }
    };

    c = getopt_long(argc, argv, optstring,
                    long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0:
      switch (option_index) {
    } // switch(option_index)
    break;
    case 'h':
      opts.runhelp =  1;
    break;
    case 'v':
      opts.runvsn =  1;
    break;
    case 'p':
      opts.pages =  strtol(optarg, NULL, 10);
    break;
    case ':':
      fprintf(stderr, "Option %s requires an argument\n",
          argv[this_option_optind]);
      opts.runhelp = 1;
    break;
    case '?':
      fprintf(stderr, "Unknown option: %s\n",
           argv[this_option_optind]);
      opts.runhelp = 1;
    break;
    } // switch(c)
  } // while()
  return opts;
} // process_options()
