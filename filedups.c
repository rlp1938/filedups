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
  int delete_flag;
} filerec_t;

typedef struct prgvar_t {
  char *dirpath;
  char *files_list;
  filerec_t *list1;
  filerec_t *list2;
  int lc1;  // record count of list1
  int lc2;  // record count of list2
  int pages;
} prgvar_t;

typedef struct size_inode_t {
  size_t size;
  ino_t inode;
} si_t;

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
static char
*validate_input(int argc, char **argv);
static void
fdrecursedir(const char *dirname, FILE *fpo);
static int
dname_grep(const char *dname);
static char
*make_files_list(const char *dir);
static mdata
*file_data_to_list(prgvar_t *pv);
static si_t
*get_size_inode(const char *p);
static void
delete_unique_size(prgvar_t *pv);
static int
cmpinodep(const void *p1, const void *p2);
static int
cmpsize_inodep(const void *p1, const void *p2);


int main(int argc, char **argv)
{ /* main */
  vsn = "1.0";
  options_t opt = process_options(argc, argv);
  prgvar_t *pv = setup_program(opt, argc, argv);
  printf("%s\n", pv->dirpath);
  pv->files_list = make_files_list(pv->dirpath);  // writes a file.
  mdata *fd = file_data_to_list(pv);
  delete_unique_size(pv);
  // delete the /tmp file.
  // free the files data block.
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
  char *names[2] = { "dname_grep.cfg", NULL };
  if (!checkfirstrun("filedups", names)) {
    firstrun("filedups", names);
    fprintf(stderr,
          "Please edit dname_grep.cfg in %s/.config/filedups"
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
  is_this_first_run();
  prgvar_t *pv = xcalloc(1, sizeof(struct prgvar_t));
  pv->dirpath = validate_input(argc, argv); // if error, no return.
  return pv;
} // setup_program()

static char
*validate_input(int argc, char **argv)
{ /* test for sane user input, either one dir or none. Return
  valid dir path if it exists. */
  static char p[PATH_MAX];
  if (argc > 2) {
    fprintf(stderr, "Input just one dir or by default none.\n");
    exit(EXIT_FAILURE);
  }
  if (argv[1]) realpath(argv[1], p); else realpath("./", p);
  if (!exists_dir(p)) {
    fprintf(stderr, "Not a directory: %s\n", p);
    exit(EXIT_FAILURE);
  }
  return p;
} // validate_input()

static void
fdrecursedir(const char *path, FILE *fpo)
{ /* Record eligible files in a temporary file. */
  DIR *dp = opendir(path);
  if (!dp) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  struct dirent *de;
  while ((de = readdir(dp))) {
    if (strcmp(de->d_name, ".") == 0 ) continue;
    if (strcmp(de->d_name, "..") == 0) continue;
    if (dname_grep(de->d_name) == 0) continue;
    char joinbuf[PATH_MAX];
    strcpy(joinbuf, path);
    strcat(joinbuf, "/");
    strcat(joinbuf, de->d_name);
    switch (de->d_type) {
    case DT_DIR:
      fdrecursedir(joinbuf, fpo);
      break;
    case DT_REG:
      fprintf(fpo, "%s\n", joinbuf);
      break;
    default:
      break;  // no interest in anything except regular files and dirs.
    } // switch()
  } // while()
  closedir(dp);
} // fdrecursedir()

static int
dname_grep(const char *dname)
{ /* Presently this is just a stub that does nothing.
   * The intention is to act on a list of compiled regexs and return
   * 0 on a match, -1 if a match is never found.
   * The idea is to exclude specified dnames.
   * */
  return -1;
} // dname_grep()

static char
*make_files_list(const char *dir)
{ /* opens a temporary file and records the list of files in it. */
  static char fn[NAME_MAX];
  sprintf(fn, "/tmp/filedups%lu.lst", time(NULL));
  FILE *fpo = dofopen(fn, "w");
  fdrecursedir(dir, fpo);
  dofclose(fpo);
  return fn;
} // make_files_list()

static mdata
*file_data_to_list(prgvar_t *pv)
{ /* The files to consider for duplication are recorded in a file;
   * they are to be included into a list of file records.
   * Returning the mdata pointer allows the data block to be free'd.
  */
  mdata *fd = readfile(pv->files_list, 1, 0);
  pv->lc1 = memlinestostr(fd);
  pv->list1 = xcalloc(pv->lc1, sizeof(struct filerec_t));
  char *cp = fd->fro;
  int i;
  for (i = 0; i < pv->lc1; i++) {
    si_t *sit = get_size_inode(cp);
    if (sit) { // possibly file has gone AWL.
      pv->list1[i].path = cp;
      pv->list1[i].inode = sit->inode;
      pv->list1[i].size = sit->size;
      cp += strlen(cp) + 1;
    } // if()
  } // for()
  return fd;
} // file_data_to_list()

static si_t
*get_size_inode(const char *p)
{ /* stat the file and return the relevant data, or if the file has
   * disappeared give an error message and return NULL.
   * File loss is a real possibilty when processing $HOME, eg browser
   * cache files reach end of lifetime. 
  */
  static si_t sit;
  struct stat sb;
  if (stat(p, &sb) == -1) {
    fprintf(stderr, "File dissappeared: %s\n", p);
    return NULL;
  }
  sit.size = sb.st_size;
  sit.inode = sb.st_ino;
  return &sit;
} // get_size_inode()

static void
delete_unique_size(prgvar_t *pv)
{ /* sort the list of file records on size and delete those having
   * singular size.
  */
  qsort(pv->list1, pv->lc1, sizeof(struct filerec_t), cmpsize_inodep);
  if (pv->list1[0].size != pv->list1[1].size)
    pv->list1[0].delete_flag = 1;
  int i;
  for (i = 1; i < pv->lc1-1; i++) {
    if (pv->list1[i].size != pv->list1[i-1].size &&
        pv->list1[i].size != pv->list1[i+1].size) {
      pv->list1[i].delete_flag = 1;
    }
  } // for(i...)
  if (pv->list1[pv->lc1-1].size != pv->list1[pv->lc1-2].size)
    pv->list1[pv->lc1-1].delete_flag = 1;
  /* Now count the records to retain, those with size > 0, and have
   * delete flag not set. */
  pv->lc2 = 0;
  for (i = 0; i < pv->lc1; i++) {
    if (pv->list1[i].size > 0 && pv->list1[i].delete_flag == 0)
      pv->lc2++;
  } // for(i...)
  /* Records are in order of size, and inode as a secondary key. These
   * records will be placed in a second list, pv->list2 for further
   * action.
  */
  pv->list2 = xcalloc(pv->lc2, sizeof(struct filerec_t));
  int j = 0;
  for (i = 0; i < pv->lc1; i++) {
    if (pv->list1[i].size > 0 && pv->list1[i].delete_flag == 0) {
      pv->list2[j] = pv->list1[i];
      j++;
    } // if()
  } // for()
} // delete_unique_size()

static int
cmpinodep(const void *p1, const void *p2)
{
  filerec_t *frp1 = (filerec_t *)p1;
  filerec_t *frp2 = (filerec_t *)p2;

  /* I can not just rely on a simple subtaction because I am operating
   * on 8 byte numbers which can generate results that overflow an int.
  */
  if (frp1->inode > frp2->inode) {
    return 1;
  } else if (frp1->inode < frp2->inode) {
    return -1;
  }
  return 0;
} // cmpinodep()

static int
cmpsize_inodep(const void *p1, const void *p2)
{ /* Will treat the inode number as a second place key. */
  filerec_t *frp1 = (filerec_t *)p1;
  filerec_t *frp2 = (filerec_t *)p2;

  /* I can not just rely on a simple subtaction because I am operating
   * on 8 byte numbers which can generate results that overflow an int.
  */
  if (frp1->size > frp2->size) {
    return 1;
  } else if (frp1->size < frp2->size) {
    return -1;
  } else {
    if (frp1->inode > frp2->inode) {
      return 1;
    } else if (frp1->inode < frp2->inode) {
      return -1;
    }
  }
  return 0;
} // cmpsize_inodep()
