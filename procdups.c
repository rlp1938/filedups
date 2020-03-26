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
*parseline(const char *s);
static void
rewrite_dups(char **list, int last);

int main(void)
{
  fdata *fd = readfile("duplicates.lst");
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
    fprintf(stdout, "%s\n", parseline(items[i]));
    int first = i;
    int j = i + 1;
    while ((items[j] && strncmp(items[first], items[j], 8) == 0)) {
      fprintf(stdout, "%s\n", parseline(items[j]));
      j++;
    }
    int last = j;
    fputs("Replies are case insesnitive.\n", stdout);
    fprintf(stdout, "Quit without rewrite (q)  Rewrite dups.lst then"
    " quit (s)\nShow next group, no action on this one (N)"
    "\n? ");
    char ans[4];
    fgets(ans, 4, stdin);
    switch (ans[0]) {
      case 'q': // quit without rewriting 'dups.lst'
      case 'Q':
        return;
        break;
      case 's': // rewrite 'dups.lst' and then quit.
      case 'S':
        rewrite_dups(items, last);
        return;
        break;
      case 'n': // show next group without doing anything.
      case 'N':
        break;
      default:
        break;
    } // switch()
    i = j;
  } // while(items[i])
} // actondups()

static char
*parseline(const char *s)
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
} // parseline()

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
