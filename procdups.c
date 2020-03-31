/*
 * procdups.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct fdata {
  char *begin;
  char *finis;
} fdata;

typedef struct opdata {
  int first;    // first index of interest into list.
  int last;     // last index of interest into list.    
  char **list;  // array of C strings.
} opdata;

static fdata
*readfile(const char *path);
static void
*xcalloc(size_t count, size_t size);
static
int lines2cstr(fdata *fd);
static void
dosystem(const char *cmd);
static void
actondups(char **items);
static char
*showline(const char *s);
static void
rewrite_dups(char **list, int last);
static void
hardlink_dups(char **list, int first, int last);
static ino_t
get_inode(const char *items);
static char
*get_path(const char *items);
static void
delete_dups(char **list, int first, int last);

int main(int argc, char **argv)
{
  fdata *fd;
  if (argc == 1) {
    fd = readfile("duplicates.lst");
  } else {
    fd = readfile(argv[1]);
  }
  int lc = lines2cstr(fd);
  char **strarray = xcalloc(lc, sizeof(char **));
  int i;
  char *cp = fd->begin;
  for (i = 0; i < lc; i++) {
    strarray[i] = cp;
    cp += strlen(cp) + 1;
  }
  actondups(strarray);
  free(strarray);
  free(fd);
  return 0;
} // main()

fdata
*readfile(const char *path)
{ /* eats a file in one lump. */
  fdata *fd = xcalloc(1, sizeof(struct fdata));
  struct stat sb;
  if (stat(path, &sb) == -1 ) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  if (!(S_ISREG(sb.st_mode))) {
    fprintf(stderr, "Not a regular file: %s\n", path);
    exit(EXIT_FAILURE);
  }
  size_t _s = sb.st_size;
  void *p = xcalloc(1, _s);
  if (!p) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  FILE *fpi = fopen(path, "r");
  if (!fpi) {
    perror(path);
    exit(EXIT_FAILURE);
  }
  size_t bytes_read = fread(p, 1, _s, fpi);
  if (bytes_read != _s) {
    perror("fread");
    exit(EXIT_FAILURE);
  }
  fd->begin = p;
  fd->finis = p + _s;
  return fd;
} // readfile()

static void
*xcalloc(size_t _nmemb, size_t _size)
{ /* calloc() with error handling */
  void *p = calloc(_nmemb, _size);
  if (!p) {
    perror("calloc");
    exit(EXIT_FAILURE);
  }
  return p;
} // xcalloc()

static
int lines2cstr(fdata *fd)
{ /* replaces all '\n' with '\0' and returns the count of it. */
  size_t i, s;
  int count = 0;
  s = fd->finis - fd->begin;
  for (i = 0; i < s; i++) {
    if (fd->begin[i] == '\n') {
      fd->begin[i] = '\0';
      count++;
    }
  }
  return count;
} // lines2cstr()

void dosystem(const char *cmd)
{
  const int status = system(cmd);

    if (status == -1) {
        fprintf(stderr, "system failed to execute: %s\n", cmd);
        exit(EXIT_FAILURE);
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        fprintf(stderr, "%s failed with non-zero exit\n", cmd);
        exit(EXIT_FAILURE);
    }

    return;
} // dosystem()

static void
actondups(char **items)
{ /* Display groups of dup, get user requirements, and act as
  * specified */
  int i = 0;
  while (items[i]) {
    dosystem("/usr/bin/clear");
    char sumbuf[33];
    strncpy(sumbuf, items[i], 32);
    sumbuf[32] = '\0';
    fprintf(stdout, "md5sum: %s\n", sumbuf);
    fprintf(stdout, "%s\n", showline(items[i]));
    int first = i;
    int j = i + 1;
    while ((items[j] && strncmp(items[first], items[j], 8) == 0)) {
      fprintf(stdout, "%s\n", showline(items[j]));
      j++;
    }
    int last = j;
    fputs("Replies are case insensitive.\n", stdout);
    fprintf(stdout, "Quit without rewrite (q)\nRewrite "
    " duplicates.lst then quit (s)\n"
    "Show next group, no action on this one (N)\n"
    "Hard link all of this group together (L)\n"
    "Delete all files in this displayed block (d)\n"
    "? ");
    char ans[4];
    fgets(ans, 4, stdin);
    switch (ans[0]) {
      case 'q': // quit without rewriting 'duplicates.lst'
      case 'Q':
        return;
        break;
      case 's': // rewrite 'duplicates.lst' and then quit.
      case 'S':
        rewrite_dups(items, last);
        return;
        break;
      case 'n': // show next group without doing anything.
      case 'N':
        break;
      case 'l': // Hard link all items together.
      case 'L':
        hardlink_dups(items, first, last);
        break;
      case 'd': // Delete all files in this block.
      case 'D':
        delete_dups(items, first, last);
        break;
      default:
        break;
    } // switch()
    i = j;
  } // while(items[i])
} // actondups()

static char
*showline(const char *s)
{ /* breaks the data line into separate fields for display. */
  char work[PATH_MAX + 128];
  static char out[2 * PATH_MAX];
  strcpy(work, s+33);
  char *fr = work;
  char *to = strchr(work, '\t'); *to = '\0';
  strcpy(out, "inode ");
  strcat(out, fr);
  fr = to + 1;
  to = strchr(fr, '\t'); *to = '\0';
  strcat(out, "\tsize ");
  strcat(out, fr);
  fr = strstr(to+1, "/home");
  strcat(out, "\n");
  strcat(out, fr);
  return out;
} // showline()

static void
rewrite_dups(char **list, int last)
{ /* last is the index of the last dups record displayed; required to
  * rewrite 'dups.lst' from the record folowing. */
  FILE *fpo = fopen("dups.lst", "w");
  if (!fpo) {
    perror("dups.lst");
    exit(EXIT_FAILURE);
  }
  int i;
  for (i = last; list[i]; i++) {
    fprintf(fpo, "%s\n", list[i]);
  }
  fclose(fpo);
} // rewrite_dups()

static void
hardlink_dups(char **list, int first, int last)
{ /* Using the first path as the master, delete and link all other
   * paths within this group to that.*/
  int i;
  int masteridx = first;
  ino_t masterinode = get_inode(list[masteridx]);
  char *masterpath = get_path(list[masteridx]);
  for (i = first+1; i < last; i++) {
    ino_t inode = get_inode(list[i]);
    if (inode != masterinode) { // hardlinked blocks may exist.
      char *p = get_path(list[i]);
      if (unlink(p) == -1) {
        perror(p);  // a file may go AWL since list creation.
      } else {
        sync();
        link(masterpath, p);
      }
    } // if(inode ...)
  } // for()
} // hardlink_dups()

static ino_t
get_inode(const char *line)
{ /* Extracts the inode from the list item containing it. Fields are
   * 1. md5sum, 2, inode as string, 3. file size as string, 4. path.
   * All separated by <tab>.
  */
  char *inostr = strchr(line, '\t');
  /* The string form of the inode is wrapped in '\t', strtol will
   * automatically work with that. */
  ino_t res = strtoul(inostr, NULL, 10);
  return res;
} // get_inode()

static char
*get_path(const char *line)
{ /* Extracts the path from list item containing it. See get_inode().
   * */
   char *ret = strstr(line, "/home");
   return ret;
} // get_path()

static void
delete_dups(char **list, int first, int last)
{ /* Delete all displayed files in this block. */
  int i;
  for (i = first; i < last; i++) {
    char *p = get_path(list[i]);
    if (unlink(p) == -1) {
      perror(p);  // It's ok for a file to go AWL in this operation.
    }
  }
  sync();
} // delete_dups()
