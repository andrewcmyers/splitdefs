#ifndef _READER_H
#define _READER_H

/* A "reader" reads a file line by line. Lines may be arbitrarily long
 * and may be terminated by either a newline or a newline/carriage return.
 */

#include "str.h"
#include <stdio.h>

struct reader;

int Read_line(struct reader *, struct str *s);
  /* Returns TRUE if a line was read. "s" is initialized to contain
   * the line read, without any terminating newline, etc. Return FALSE
   * if there are no more lines, and also uninitialize the reader. "s"
   * is not initialized in this case. */
void Reader_init(struct reader *, int fd);
  /* Initialize the reader to read from the file descriptor "fd" */
void Reader_free(struct reader *);
  /* Uninitialize the reader. This is done automatically when
   * "Read_line" returns FALSE. */

/***** Private. Do not use knowledge of the remainder of this file. *****/

struct reader {
    FILE *in;
    int done;
};

#endif /* _READER_H */
