/*
 * filedups.c
 * 
 * Copyright 2020 Robert L (Bob) Parker <rlp1938@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdarg.h>
#include <getopt.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>

// structs
typedef struct filerec_t {
  char *path;
  ino_t inode;
  size_t size;
  char *md5;
} filerec_t;

typedef struct prgvar_t {
  char *dirpath;
  char *files_list;
  filerec_t **list1;
  filerec_t **list2;
  int pages;
} prgvar_t;

#include "dirs.h"
#include "files.h"
#include "gopt.h"
#include "firstrun.h"

// Globals
static char *vsn;
// headers
static void
dohelp(int forced);
static void
dovsn(void);
static void
is_this_first_run(void);
static void
free_prgvar_t(prgvar_t *pv);
static prgvar_t
*setup_program(options_t opt, int argc, char **argv);

int main(int argc, char **argv)
{ /* main */
  vsn = "1.0";
  is_this_first_run(); // check first run
  // data gathering
  options_t opt = process_options(argc, argv);
  prgvar_t *pv = setup_program(opt, argc, argv) ;
  return 0;
} // main()

void
dohelp(int forced)
{ /* runs the manpage and then quits. */
  char command[PATH_MAX];
  char *dev = "./filedups.1";
  char *prd = "filedups";
  if (exists_file(dev)) {
    sprintf(command, "man %s", dev);
  } else {
    sprintf(command, "man 1 %s", prd);
  }
  xsystem(command, 1);
  exit(forced);
} // dohelp()

void
dovsn(void)
{ /* print version number and quit. */
  fprintf(stderr, "filedups, version %s\n", vsn);
  exit(0);
} // dovsn()

void
is_this_first_run(void) 
{ /* test for first run and take action if it is */
  char *names[2] = { "filedups.cfg", NULL };
  if (!checkfirstrun("filedups", names)) {
    firstrun("filedups", names);
    fprintf(stderr,
          "Please edit filedups.cfg in %s/.config/filedups"
          " to meet your needs.\n",
          getenv("HOME"));
    exit(EXIT_SUCCESS);
  }
} // is_this_first_run()

void
free_prgvar_t(prgvar_t *pv)
{ /* frees the objects built on the heap. */
  if (!pv) return;
  if (pv->dirpath) free(pv->dirpath);
  if (pv->files_list) free(pv->files_list);
  if (pv->list1) free(pv->list1);
  if (pv->list2) free(pv->list2);
  free(pv);
} // free_prgvar_t()

static prgvar_t
*setup_program(options_t opt, int argc, char **argv)
{ /* If --help or --version is a chosen option, action those and quit,
   * otherwise check argv[1] for validity and set up those prgvar_t
   * variables that are known at this stage.
  */
  if (opt.runhelp) dohelp(0); // will exit.
  if (opt.runvsn) dovsn(); // will exit.
} // setup_program()
