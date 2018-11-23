#include "config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#if HAVE_ASSERT_H
#include <assert.h>
#endif

#include "str.h"

static char null[1] = { 0 };

void Str_init(struct str *s) {
    s->p = &null[0];
    s->len = 1;
    s->alloc = 0;
}

static void resize(struct str *s, int sz) {
/* Enlarge the allocation of "s" so it can contain at least "sz"
   characters (including the null). Do not modify "len" */

    if (sz > s->alloc) {
	size_t newalloc = 4;
	char *newp;
	while (newalloc < sz) newalloc <<= 1;
	newp = (char *)malloc(newalloc);
	if (s->len) memcpy(newp, s->p, s->len);
	if (s->alloc) free(s->p);
	s->p = newp;
	s->alloc = newalloc;
    }
}

void Str_concat(struct str *s, char *c) {
    int L = strlen(c);
    resize(s, s->len + L);
    memcpy(s->p + s->len - 1, c, L + 1);
    s->len += L;
}

void Str_concat1(struct str *s, char c) {
    resize(s, s->len + 1);
    s->len++;
    s->p[s->len - 2] = c;
    s->p[s->len - 1] = 0;
}


void Str_new(struct str *s, char *c) {
    Str_init(s);
    Str_concat(s, c);
}

void Str_substr(struct str *s, char *c, int len) {
    Str_init(s);
    assert(len <= strlen(c));
    resize(s, len + 1);
    memcpy(s->p, c, len);
    s->p[len] = 0;
}

void Str_free(struct str *s) {
    if (s->alloc) free(s->p);
#ifndef NDEBUG
    s->alloc = s->len = 0;
    s->p = 0;
#endif
}

void Str_share(struct str *s, char *c) {
    s->p = c;
    s->len = strlen(c) + 1;
    s->alloc = 0;
}

void Str_new_concat(struct str *s, char *c1, ...) {
    va_list ap;
    char *c;

    Str_init(s);
    Str_concat(s, c1);
    va_start(ap, c1);
    while ((c = va_arg(ap, char *))) {
	Str_concat(s, c);
    }
    va_end(ap);
}

void Str_trim(struct str *s, int l) {
    assert(l < s->len);
    if (s->len == 1) return; // nothing to trim
    s->p[l] = 0;
    s->len = l + 1;
}
