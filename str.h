#ifndef _STR_H
#define _STR_H

/* A "str" is a cheap growable string for C programs. It may be
 * arbitrarily long. */

struct str;

/* #define STR_IMPORT before including this header to get short names for
 * some of the functions defined here.
 */
#ifdef STR_IMPORT
#define length(x) Str_length(x)
#define c(x) Str_chars(x)
#define concat(x,y) Str_concat(x,y)
#define share(x,y) Str_share(x,y)
#define substr(x,y) Str_substr(x,y)
#define new_concat Str_new_concat
#endif /* STR_IMPORT */

/*
 * The following functions exist, but are implemented as macros:

int Str_length(struct str *s);
  // Return the number of characters in "s", not counting the null termination.
char *Str_chars(struct str *s);
  // Return a pointer to the (null-terminated) characters of "s".

*/

void Str_init(struct str *s);
  /* Initialize an empty string. */
void Str_concat(struct str *s, char *c);
  /* Concatenate the characters of the null-terminated string
     "c" onto "s". */
void Str_concat1(struct str *s, char c);
  /* Concatenate the character "c" onto "s". */
void Str_new(struct str *s, char *c);
  /* Initialize a new string that is a copy of the
     null-terminated string in "c". */
void Str_substr(struct str *s, char *c, int len);
  /* Initialize a new string containing the first "len" characters of "c". */
void Str_free(struct str *s);
  /* Destroy the string and release its storage. Must be reinitialized before
     any subsequent use. */
void Str_share(struct str *s, char *c);
  /* Initialize a string that shares the storage of "c". "c" may not be
     modified or deleted as long as this string exists. */
void Str_new_concat(struct str *s, char *c1, ...);
  /* Initialize "s" to contain the concatenation of the
   * strings c1, ..., cn. The final argument passed must be 0. */
void Str_trim(struct str *s, int l);
  /* Set the length of "s" to "l" (not including the final null). This may
   * not increase the length of the string. */

/***** Private. Do not use knowledge of the remainder of this file. *****/

struct str {
    char *p; /* null-terminated */
    int len; /* includes the null, so always > 0 */
    int alloc;
};

#define Str_length(s) ((s)->len - 1)
#define Str_chars(s) ((s)->p)

#endif /* _STR_H */
