#include "config.h"

#define STR_IMPORT
#include "str.h"

#include "reader.h"

#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "truefalse.h"

void usage(char *p) {
    fprintf(stderr, "Usage: %s [-D defn] [-I include] [-d dir] "
		    "[-p prefix] [-v] <.h file>\n", p);
    exit(EXIT_FAILURE);
}

#define TRY(expr) if (0 > (expr)) { perror(#expr); exit(EXIT_FAILURE); }
#define TRYM(expr,msg) if (0 > (expr)) { perror(msg); exit(EXIT_FAILURE); }

static void preprocess(struct str *line, FILE *cpp_in);
  /* Prepare one line for input to CPP */

static void postprocess(struct str *line, struct str *dir,
		char *dprefix);
  /* Process one line of the CPP output, updating the
     file corresponding to the named macro if necessary. */

static int pipe_fork(int infd, /*OUTPUT*/ int *outfd);
  /* Fork a new process that reads from "infd" and writes
     to "*outfd". Sets "*outfd" in both processes; return
     value is as for "fork()". */

static void quote(char *s, struct str *out);
  /* Set "out" to contain the quoted version of the string in "s". */
static void unquote(char *p, struct str *out);
  /* Set "out" to recover a string from its quoted version in "s". */

static int compare_files(char *fn1, char *fn2);
  /* Return whether two files have the same contents */

int verbose = FALSE;

static void verbose_cmd(char *argv[]);

char cpp_prefix[] = CPP;

int main(int argc, char *argv[]) {
    struct str dir;
    int diropt = FALSE;
    char *infile;
    char *dprefix = "SPLITDEFS_";
    int c;
    char *cpp_argv[MAX_CPP_ARGS];
    int num_cpp_args;
    int fd;
    FILE *cpp_in;
    int cpp_out, cpp_input[2];
    struct reader rd;
    struct str line;
    char *p;

    { /* construct the argv prefix for the cpp invocation */
	num_cpp_args = 0;
	p = cpp_prefix;
	for (;;) {
	    cpp_argv[num_cpp_args++] = p;
	    while (*p && *p != ' ') p++;
	    if (!*p) break;
	    *p = 0; /* null-terminate this word of the cpp string */
	    p++;
	}
    }

    while (-1 != (c = getopt(argc, argv, "D:I:p:vd:"))) {
	switch (c) {
	    case 'D':
	    case 'I': { struct str opt; Str_init(&opt);
			if (c == 'D')
			    concat(&opt, "-D");
			else
			    concat(&opt, "-I");
			concat(&opt, optarg);
			if (num_cpp_args > MAX_CPP_ARGS) {
			    fprintf(stderr, "Too many cpp arguments\n");
			    exit(EXIT_FAILURE);
			}
			cpp_argv[num_cpp_args++] = c(&opt);
		      } break;
	    case 'd': new_concat(&dir, optarg, "/", 0);
		      diropt = TRUE;
		      break;
	    case 'p': dprefix = optarg;
		      break;
	    case 'v': verbose = TRUE;
		      puts("splitdefs version " VERSION);
		      break;
	    default:
		usage("splitdefs");
		break;
	}
    }
    if (argc != optind + 1) {
	usage("splitdefs");
    }
    infile = argv[optind];

    if (!diropt) new_concat(&dir, infile, ".d/", 0);
    if (0 != access(c(&dir), R_OK | X_OK)) {
	if (verbose) printf("mkdir %s\n", c(&dir));
	TRY(mkdir(c(&dir), 0777));
    }

    fd = open(infile, O_RDONLY);
    if (fd < 0) {
	perror(infile);
	exit(EXIT_FAILURE);
    }

    TRY(pipe(cpp_input));
#ifdef CPP_ARG
    cpp_argv[num_cpp_args++] = CPP_ARG;
#endif
    cpp_argv[num_cpp_args] = 0;

    if (verbose) verbose_cmd(cpp_argv);
    if (pipe_fork(cpp_input[0], &cpp_out) == 0) {
        close(cpp_input[1]);
	TRYM(execvp(cpp_argv[0], cpp_argv),"exec'ing preprocessor " CPP);
    }

    cpp_in = fdopen(cpp_input[1], "w");

    Reader_init(&rd, fd);
    while (1) {
	if (!Read_line(&rd, &line)) break;
	preprocess(&line, cpp_in);
	Str_free(&line);
    }

    fclose(cpp_in);

    Reader_init(&rd, cpp_out);
    while (1) {
	if (!Read_line(&rd, &line)) break;
	postprocess(&line, &dir, dprefix);
	Str_free(&line);
    }
    Str_free(&dir);
    return 0;
}

int pipe_fork(int infd, int *output_infd) {
    int ret, output[2];
    TRY(pipe(output));
    *output_infd = output[0];
    ret = fork();
    if (ret == 0) {
	TRY(close(0));
	TRY(close(1));
	TRY(dup2(infd, 0));
	TRY(dup2(output[1], 1));
	TRY(close(infd));
	TRY(close(output[1]));
	TRY(close(output[0]));
    } else {
	TRY(close(infd));
	TRY(close(output[1]));
    }
    return ret;
}

int id_starter(char c) {
    return (isalpha(c) || c == '_') ? TRUE : FALSE;
}

void preprocess(struct str *line, FILE *cpp_in) {
    char *p, *end;
    p = c(line);
    end = p + length(line);
    if (0 == strncmp(p, "/* #", 4) &&
	(end - p) > 2 &&
	(0 == strcmp(end - 2, "*/"))) {
	p += 3;
	end -= 2;
    }
    if (p[0] != '#') goto verbatim;
    do p++; while (p != end && isspace(*p)); /* skip any whitespace after '#' */
    if (p == end) goto verbatim;

    if ((0 == strncmp("define", p, 6) && isspace(p[6])) ||
        (0 == strncmp("undef", p, 5) && isspace(p[5]))) {
	struct str s1, s2;
	Str_substr(&s1, p, end - p);
	quote(c(&s1), &s2);
	fprintf(cpp_in, PREFIX "%s\n", c(&s2));
	Str_free(&s2);
	Str_free(&s1);
    }

verbatim:
    fprintf(cpp_in, "%s\n", c(line));
    return;
}

static void postprocess(struct str *line, struct str *dir,
		char *dprefix) {
    char *c;
    char *begin, *id, *temp_name;
    char *end = c(line) + line->len;
    struct str macro, fname, defn, qdefn;
    FILE *tmpfile;
    int is_define = FALSE, is_undef = FALSE;

/* Find the macro name */
    if (0 != strncmp(PREFIX "'", c(line), strlen(PREFIX "'"))) return;
    begin = c = c(line) + strlen(PREFIX "'");
    if (0 == strncmp("define", c, 6)) { is_define = TRUE; c += 6; }
    if (0 == strncmp("undef", c, 5)) { is_undef = TRUE; c += 5; }
    while (c < end && isspace(*c)) c++;
    if (!id_starter(*c)) {
	fprintf(stderr, "Bad macro name in line: %s\n", begin);
	return;
    }
    id = c;
    do c++; while (c < end && (id_starter(*c) || isdigit(*c)));
    Str_substr(&macro, id, c - id);
    while (*end != '\'' && end >= c) end--;
    Str_init(&defn);
    Str_init(&qdefn);
    Str_substr(&qdefn, c, end - c);
    unquote(c(&qdefn), &defn);

/* create temporary file */
    temp_name = tempnam(c(dir), 0);
    if (verbose) printf("macro %s: creating %s\n", c(&macro), temp_name);
    tmpfile = fopen(temp_name, "w");
    fprintf(tmpfile, "#ifndef %s%s_H\n", dprefix, c(&macro));
    fprintf(tmpfile, "#define %s%s_H\n", dprefix, c(&macro));
    if (is_define) {
	fprintf(tmpfile, "\n#define %s%s\n\n", c(&macro), c(&defn));
    } else if (is_undef) {
	fprintf(tmpfile, "\n#undef %s\n\n", c(&macro));
    }
    fprintf(tmpfile, "#endif /* %s%s_H */\n", dprefix, c(&macro));
    fclose(tmpfile);


/* compare existing file with temporary file, replace if new or changed */
    new_concat(&fname, c(dir), c(&macro), ".h", 0); 
    if (!compare_files(temp_name, c(&fname))) {
	if (verbose) printf("mv %s %s\n", temp_name, c(&fname));
	TRY(rename(temp_name, c(&fname)));
    } else {
	if (verbose) printf("rm %s\n", temp_name);
	TRY(unlink(temp_name));
    }

    Str_free(&fname);
    Str_free(&macro);
    Str_free(&defn);
    Str_free(&qdefn);
    free(temp_name);
}

static int needs_quoting(char *p) {
    for (; *p; p++) {
	switch (*p) {
	    case '\'': return TRUE;
	    case '\\': return TRUE;
	    case '\"': return TRUE;
	    case ' ': return TRUE;
	}
    }
    return FALSE;
}

static void quote(char *p, struct str *out) {
    Str_init(out);
    concat(out, "\'");
    for (; *p; p++) {
	switch (*p) {
	    case '\'':
		concat(out, "\\\'");
		break;
	    case '\\':
		concat(out, "\\\\");
		break;
	    default:
		Str_concat1(out, *p);
		break;
	}
    }
    concat(out, "\'");
}

static void unquote(char *p, struct str *out) {
    Str_init(out);
    for (; *p; p++) {
	if (*p == '\\') {
	    p++;
	    assert(*p);
	}
	Str_concat1(out, *p);
    }
}

static void verbose_cmd(char *argv[]) {
    char **c = &argv[0];
    printf("%s", *c++);
    while (*c) {
	if (needs_quoting(*c)) {
	    struct str cq;
	    quote(*c, &cq);
	    printf(" %s", c(&cq));
	    Str_free(&cq);
	} else {
	    printf(" %s", *c);
	}
	c++;
    }
    putchar('\n');
}

static int compare_files(char *fn1, char *fn2) {
    FILE *f1 = fopen(fn1, "r");
    FILE *f2 = fopen(fn2, "r");
    if (!f1 || !f2) {
	if (f1) fclose(f1);
	if (f2) fclose(f2);
	return FALSE;
    }
    for (;;) {
	int c1 = fgetc(f1);
	int c2 = fgetc(f2);
	if (c1 != c2) {
	    fclose(f1); fclose(f2);
	    return FALSE;
	}
	if (c1 == -1) {
	    fclose(f1); fclose(f2);
	    return TRUE;
	}
    }
}
