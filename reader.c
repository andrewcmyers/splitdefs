#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include "truefalse.h"
#include "reader.h"

void Reader_init(struct reader *rd, int fd) {
    rd->in = fdopen(fd, "r");
    rd->done = FALSE;
}

void Reader_free(struct reader *rd) {
    fclose(rd->in);
    rd->in = 0;
    rd->done = TRUE;
}

int Read_line(struct reader *rd, struct str *line) {
    char buffer[256];
    int len, read_any = FALSE;
    if (rd->done) return FALSE;
    Str_init(line);
    while (1) {
	char *c = fgets(buffer, sizeof(buffer), rd->in);
	if (!c) {
	    if (!read_any) { Str_free(line); return FALSE; }
	    else { rd->done = TRUE; break; } /* remember for next call */
	}
	read_any = TRUE;
	len = strlen(buffer);
	Str_concat(line, buffer);
	if (len == 0 || buffer[len - 1] == '\n') break;
    }
    len = Str_length(line);
    if (len > 0 && Str_chars(line)[len - 1] == '\n') 
	Str_trim(line, len - 1);
    return TRUE;
}
