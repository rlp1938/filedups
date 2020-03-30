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

#include "str.h"
#include "dirs.h"
#include "files.h"
#include "gopt.h"
#include "firstrun.h"
#include "calcmd5.h"

// structs
typedef struct filerec_t {
  char *path;
  ino_t inode;
  size_t size;
  char md5[33];
  int delete_flag;
} filerec_t;

typedef struct prgvar_t {
  char *dirpath;
  char *files_list;
  filerec_t *list1;
  filerec_t *list2;
  int lc1;    // record count of list1
  int lc2;    // record count of list2
  int pages;  /* Max number of blocks of 4096 bytes to calculate
  * md5sum for a file. If pages < 1 then the entire file size will be
  * used. */
  size_t dat_size;  // initial memory allocation for files_list.
  size_t inc_size;  // If dat_size bytes is too small, add this value.
  mdata *md;        // describes a block of chars in memory.
} prgvar_t;

typedef struct size_inode_t {
  size_t size;
  ino_t inode;
} si_t;

// Globals
static char *vsn;
// headers
static void
dohelp(int forced);
static void
dovsn(void);
static void
is_this_first_run(void);
static prgvar_t
*read_config(const char *prgname);
static void
free_prgvar_t(prgvar_t *pv);
static void
setup_program(options_t *opt, int argc, char **argv, prgvar_t *pv);
static void
validate_input(const char *path);
static void
fdrecursedir(const char *dirname, prgvar_t *pv);
static int
dname_grep(const char *dname);
static void
make_filerecord_list(prgvar_t *pv);
static void
make_files_list(prgvar_t *pv);
static si_t
*get_size_inode(const char *p);
static void
delete_unique_size_file_records(prgvar_t *pv);
static int
cmpinodep(const void *p1, const void *p2);
static int
cmpsize_inodep(const void *p1, const void *p2);
static void
mem_append(const char *p, prgvar_t *pv);
static size_t
str_sizes_to_number(const char *strnum);
// TODO delete_groups ... not functioning correctly.
static void
delete_groups_of_files_sharing_size_and_inode(prgvar_t *pv);
static void
calcmd5sums(filerec_t *list, int lc, int pages);
static int
cmpmd5p(const void *p1, const void *p2);
static void
delete_unique_md5sum_records(prgvar_t *pv);
static void
serialise_duplicate_records(prgvar_t *pv);

int main(int argc, char **argv)
{ /* main */
  vsn = "1.0";
  prgvar_t *pv = read_config("filedups");
  options_t opt = process_options(argc, argv);
  setup_program(&opt, argc, argv, pv);
  char thepath[PATH_MAX];
  pv->lc1 = 0;  // redundant.
  int i;
  if (argc == 1) {
    pv->dirpath = realpath("./", thepath);
    make_files_list(pv);
    printf("%s\n", pv->dirpath);
  } else for (i = 1; argv[i] ; i++) {
    pv->dirpath = realpath(argv[i], thepath);
    validate_input(thepath);
    make_files_list(pv);
    printf("%s\n", pv->dirpath);
  }
  make_filerecord_list(pv);
  delete_unique_size_file_records(pv);
  delete_groups_of_files_sharing_size_and_inode(pv);
  calcmd5sums(pv->list1, pv->lc1, pv->pages); // list1; last used list.
  delete_unique_md5sum_records(pv);
  serialise_duplicate_records(pv);

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

static void
setup_program(options_t *opt, int argc, char **argv, prgvar_t *pv)
{ /* If --help or --version is a chosen option, action those and quit,
   * otherwise check argv[1] for validity and set up those prgvar_t
   * variables that are known at this stage.
  */
  if (opt->runhelp) dohelp(0); // will exit.
  if (opt->runvsn) dovsn(); // will exit.
  is_this_first_run();
  /* data size has been set from read_config(), alter these settings,
   * alter these settings if options require it.
  */
  size_t od = str_sizes_to_number(opt->dat_size);
  if (od) pv->dat_size = od;
  od = str_sizes_to_number(opt->dat_incr);
  if (od) pv->inc_size = od;
  /* regardless of how the data size and it's increment were set,
   * ensure that they are set to a memory page boundary. */
  size_t adjust = pv->dat_size % 4096;
  if (adjust) pv->dat_size += (4096 - adjust);
  adjust = pv->inc_size % 4096;
  if (adjust) pv->inc_size += (4096 - adjust);
  static mdata md;
  md.fro = xcalloc(pv->dat_size, 1);
  md.limit = md.fro + pv->dat_size;
  md.to = md.fro; // data block is empty.
  pv->md = &md;
} // setup_program()

static void
validate_input(const char *p)
{ /* test for sane user input, either one dir or none. Return
  valid dir path if it exists. */
  if (!exists_dir(p)) {
    fprintf(stderr, "Not a directory: %s\n", p);
    exit(EXIT_FAILURE);
  }
} // validate_input()

static void
fdrecursedir(const char *path, prgvar_t *pv)
{ /* Record eligible files in a block of memory. */
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
      fdrecursedir(joinbuf, pv);
      break;
    case DT_REG:
      mem_append(joinbuf, pv);
      pv->lc1++;
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

static void
make_files_list(prgvar_t *pv)
{ /* writes file paths to a block of memory as C strings. */
  fdrecursedir(pv->dirpath, pv);
} // make_files_list()

static void
make_filerecord_list(prgvar_t *pv)
{ /* The files to consider for duplication are recorded in a file;
   * they are to be included into a list of file records.
   * Returning the mdata pointer allows the data block to be free'd.
  */
  pv->list1 = xcalloc(pv->lc1, sizeof(struct filerec_t));
  char *cp = pv->md->fro;
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
delete_unique_size_file_records(prgvar_t *pv)
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
} // delete_unique_size_file_records()

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

static prgvar_t
*read_config(const char *prgname)
{ /* presently this is just a bullshit placeholder that acts as if it
   * really did read a config file. */
  static prgvar_t pv = {0};
  static mdata md;
  pv.dat_size = 1024 * 1024;  // 1 meg so far;
  pv.inc_size = pv.dat_size / 10; // may be replaced by options.
  pv.pages = 1; // option can vary this.
  pv.md = &md;
  return &pv;
} // read_config()

void
mem_append(const char *p, prgvar_t *pv)
{ /* appends a C string to a memory block, taking care of memory
   * re-sizing as required. I build in an 8 byte safety margin before
   * invoking a realloc() operation.
  * */
  size_t len = strlen(p);
  size_t available = pv->md->limit - pv->md->to;
  if ( len + 8 > available) { // realloc() required
    size_t addon = pv->inc_size;
    while (len + 8 > addon) addon += len; // not likely, but be sure.
    // record current block parameters
    char *fro = pv->md->fro;
    char *to = pv->md->to;
    char *limit = pv->md->limit;
    size_t newsize = limit - fro + addon;
    pv->md->fro = realloc(fro, newsize);
    if (pv->md->fro != fro) {
      off_t locdiff = pv->md->fro - fro;
      to += locdiff;
      limit += locdiff;
    }
    memset(limit, 0, addon);
    limit += addon;
    pv->md->to = to;
    pv->md->limit = limit;
  }
  strcpy(pv->md->to, p);
  pv->md->to += len + 1;
} // mem_append()

static size_t
str_sizes_to_number(const char *strnum)
{ /* convert strings of human readable form to size_t, eg 1M, 100K...*/
  size_t num = strtoul(strnum, NULL, 10);
  size_t len = strlen(strnum);
  char modifier = strnum[len-1];  // last char
  size_t mul;
  switch (modifier) {
    case 'k':
    case 'K':
      mul = 1024;
      break;
    case 'm': case 'M':
      mul = 1024 * 1024;
      break;
    case 'g': case 'G':
      mul = 1024 * 1024 * 1024;
      break;
    default:
      mul = 1;
  }
  return mul * num;
} // str_sizes_to_number()

static void
delete_groups_of_files_sharing_size_and_inode(prgvar_t *pv)
{ /* As well as unique file sizes, already dealt with, there may be
  * hard linked blocks of files which will manifest as blocks of
  * identical sizes paired with identical inodes. This next will
  * identify any such blocks.
  * As this begins, the list sorted on size and inode, is already in
  * place in pv->list2 and is counted by pv->lc2.
  */
  int count = 0;
  int i, j;
  for (i = 1; i < pv->lc2; i++) {
    if (pv->list2[i].size == pv->list2[i-1].size &&
        pv->list2[i].inode == pv->list2[i-1].inode) {
      count++;
    } else {
      if (count) {
        for (j = i-count-1; j < i; j++) {
          pv->list2[j].delete_flag = 1;
        } // for (j...)
      } // if(count)
      count = 0;
    } // else
  } // for(size and inode comparisons)
  // count the number of items to send back to pv->list1.
  for (j = 0, i = 0; j < pv->lc2; j++) {
    if (pv->list2[j].delete_flag == 0) i++;
  }
  pv->lc1 = i;
  /* write the non-deletes back to the originating array. */
  for (i = 0, j  = 0; i < pv->lc2; i++) {
    if (pv->list2[i].delete_flag == 0) {
      pv->list1[j] = pv->list2[i];
      j++;
    } // if()
  } // for(i...)
  pv->lc1 = j;
  /* Now sort the list, pv->list1 in inode order. */
  qsort(pv->list1, pv->lc1, sizeof(struct filerec_t), cmpinodep);
} // delete_groups_of_files_sharing_size_and_inode()

static void
calcmd5sums(filerec_t *list, int lc, int pages)
{ /* Controls the md5sum calculation of a list of files. */
  int i;
  strcpy(list[0].md5, calcmd5(list[0].path, pages));
  for (i = 1; i < lc-1; i++) {
    if (list[i].inode == list[i-1].inode) {
      strcpy(list[i].md5, list[i-1].md5);
    } else {
      strcpy(list[i].md5, calcmd5(list[i].path, pages));
    }
  } // for(i ...)
  if (list[lc-1].inode == list[lc-2].inode) {
    strcpy(list[lc-1].md5, list[lc-2].md5);
  } else {
    strcpy(list[lc-1].md5, calcmd5(list[lc-1].path, pages));
  }
  /* Now sort the list on md5sum w/ inode as secondary key */
  qsort(list, lc, sizeof(struct filerec_t), cmpmd5p);
} // calcmd5sums()

static int
cmpmd5p(const void *p1, const void *p2)
{ /* md5 sums are just C strings.
   * Use inode size as secondary key.
  */
  filerec_t *frp1 = (filerec_t *)p1;
  filerec_t *frp2 = (filerec_t *)p2;
  int ret = strcmp(frp1->md5, frp2->md5);
  if (ret > 0) {
    return 1;
  } else if (ret < 0) {
    return -1;
  } else if (frp1->inode > frp2->inode) {
    return 1;
  } else if (frp1->inode < frp2->inode) {
    return -1;
  }
  return 0; // Will happen where hard linked files exist.
} // cmpmd5p()

static void
delete_unique_md5sum_records(prgvar_t *pv)
{ /* mark unique md5sums for deletion. In this instance the source of
   * the 'from' data is in list1, the 'to' data is go to list2.
  */
  if (strcmp(pv->list1[0].md5, pv->list1[1].md5) != 0)
      pv->list1[0].delete_flag = 1; // the 0th record.
  int i;
  for (i = 1; i < pv->lc1-1; i++) { // all recs between 0th and last.
    if (strcmp(pv->list1[i].md5, pv->list1[i-1].md5) != 0 &&
        strcmp(pv->list1[i].md5, pv->list1[i+1].md5) != 0) {
      pv->list1[i].delete_flag = 1;
    }
  }
  if (strcmp(pv->list1[pv->lc1-1].md5, pv->list1[pv->lc1-2].md5) != 0)
    pv->list1[pv->lc1-1].delete_flag = 1; // the last record.
  pv->lc2 = 0;
  for (i = 0; i < pv->lc1; i++) {
    if (pv->list1[i].delete_flag == 0) pv->lc2++;
  }
  /* Copy records to list2, deleteable records excepted. */
  int j = 0;
  for (i = 0; i < pv->lc1; i++) {
    if (pv->list1[i].delete_flag == 0) {
      pv->list2[j] = pv->list1[i];
      j++; 
    }
  }
  pv->lc2 = j;  // redundant check.
} // delete_unique_md5sum_records()

void
serialise_duplicate_records(prgvar_t *pv)
{ /* Serialise the data records. Field separator is \t, record
   * separator is \n. Output file is simply 'duplicates.lst' written
   * into the dir we are in when filedups is run. The list of duplicates
   * is in list2. */
  FILE *fpo = fopen("duplicates.lst", "w");
  int i;
  for (i = 0; i < pv->lc2; i++) {
    fprintf(fpo, "%s\t%lu\t%lu\t%s\n", pv->list2[i].md5,
            pv->list2[i].inode, pv->list2[i].size, pv->list2[i].path);
  }
  fclose(fpo);
} // serialise_duplicate_records()
