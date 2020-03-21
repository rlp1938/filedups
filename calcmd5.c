#include <mhash.h>
#include "calcmd5.h"

char
*calcmd5(const char *path, int pages)
{
  size_t bytes_read;
  MHASH td;
  unsigned char buffer[4096];
  unsigned char hash[16]; /* fits MD5 */
  static char result[33];
  FILE *fpi = fopen(path, "r");
  if (!fpi) {
    perror(path); // It's ok if a file or so goes AWL during processing.
    strcpy(result, "");
    return result;
  }

  td = mhash_init(MHASH_MD5);
  if (td == MHASH_FAILED) {
    perror("Hash init failed");
    exit(1);
  }
  /* I don't necessarily calculate the hash of the entire file, but
   * rather the number of 4096 byte pages specified. However, if the
   * pages specified is less than 1, the whole file is hashed. */
  int i;
  if (pages > 0) {
    for (i = 0; i < pages; i++) {
      bytes_read = fread(buffer, 1, 4096, fpi);
      if (bytes_read < 4096) break; // file size too small anyway.
    }
  } else {
    while ((bytes_read = fread(buffer, 1, 4096, fpi)) > 0) {
      mhash(td, buffer, bytes_read);
    } // while
  } // else

  mhash_deinit(td, hash);
  int j;
  for (i = 0, j = 0; i < 16; i++, j += 2) {
    sprintf(&result[j], "%.2x", hash[i]);
  }
  fclose(fpi);
  return result;
} // calcmd5()
