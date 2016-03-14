
#include <hs/hs.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "getcputime.h"


static void usage(int rc);
static void run_engines(hs_database_t *re, hs_scratch_t *scratch,
    const char *input, size_t len, int global, int repeat);


#define TIMER_START                                                          \
        begin = get_cpu_time();                                              \
        if (begin == -1) {                                                   \
            perror("get_cpu_time");                                          \
            exit(2);                                                         \
        }


#define TIMER_STOP                                                           \
        end = get_cpu_time();                                                \
        if (end == -1) {                                                     \
            perror("get_cpu_time");                                          \
            exit(2);                                                         \
        }                                                                    \
        elapsed = end - begin;


int
main(int argc, char **argv)
{
    int                  flags = HS_FLAG_DOTALL | HS_FLAG_MULTILINE;
    int                  global = 0;
    int                  repeat = 5;
    unsigned             i;
    hs_database_t       *re;
    char                *input;
    FILE                *f;
    size_t               len;
    long                 rc;
    hs_platform_info_t   plt;
    hs_scratch_t        *scratch = NULL;
    hs_compile_error_t  *err = NULL;

    int ret;

    if (argc < 3) {
        usage(1);
    }

    hs_populate_platform(&plt);

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }

       if (strncmp(argv[i], "--repeat=", sizeof("--repeat=") - 1)
                   == 0)
        {
            repeat = atoi(argv[i] + sizeof("--repeat=") - 1);
            if (repeat <= 0) {
                repeat = 5;
            }

        } else if (strncmp(argv[i], "-i", 2) == 0) {
            flags |= HS_FLAG_CASELESS;

        } else if (strncmp(argv[i], "-g", 2) == 0) {
            global = 1;

        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
    }

    if (argc - i != 2) {
        usage(1);
    }

    ret = hs_compile(argv[i], flags, HS_MODE_BLOCK, &plt, &re, &err);
    if (ret != HS_SUCCESS) {
        fprintf(stderr, "[error] compile: %s\n", argv[i]);
        return 2;
    }

    i ++;

    hs_alloc_scratch(re, &scratch);

    errno = 0;

    f = fopen(argv[i], "rb");
    if (f == NULL) {
        perror("open file");
        return 1;
    }

    if (fseek(f, 0L, SEEK_END) != 0) {
        perror("seek to file end");
        return 1;
    }

    rc = ftell(f);
    if (rc == -1) {
        perror("get file offset by ftell");
        return 1;
    }

    len = (size_t) rc;

    if (fseek(f, 0L, SEEK_SET) != 0) {
        perror("seek to file beginning");
        return 1;
    }

    input = malloc(len);
    if (input == NULL) {
        fprintf(stderr, "failed to allocate %ld bytes.\n", len);
        return 1;
    }

    if (fread(input, 1, len, f) < len) {
        if (feof(f)) {
            fprintf(stderr, "file truncated.\n");
            return 1;

        } else {
            perror("read file");
        }
    }

    if (fclose(f) != 0) {
        perror("close file");
        return 1;
    }

    run_engines(re, scratch, input, len, global, repeat);

    free(input);

    return 0;
}

struct match_cbdata {
    int matches;
    int global;
};

static int
match_cb (unsigned int id, unsigned long long from, unsigned long long to, 
    unsigned int flags, void *context)
{
    struct match_cbdata *cbdata = context;

    cbdata->matches ++;

    if (cbdata->global) {
        return 0;
    }

    return 1;
}

static void
run_engines(hs_database_t *re, hs_scratch_t *scratch,
    const char *input, size_t len, int global, int repeat)
{
    int                  i, matches = 0;
    size_t               rest;
    double               begin, end, best = -1;
    const char          *p;
    struct match_cbdata  cbdata;


    printf("Hyperscan ");

    for (i = 0; i < repeat; i++) {
        double elapsed;

        matches = 0;
        p = input;
        rest = len;
        cbdata.matches = matches;
        cbdata.global = global;

        TIMER_START

        hs_scan(re, p, rest, 0, scratch, match_cb, &cbdata);

        matches = cbdata.matches;

        TIMER_STOP

        if (i == 0 || elapsed < best) {
            best = elapsed;
        }
    }

    if (matches == 0) {
        printf("no match");

    }
    else {
        printf("match");
    }

    printf(": %.05lf ms elapsed (%d matches found, %d repeated times).\n",
           best * 1e3, matches, repeat);
}


static void
usage(int rc)
{
    fprintf(stderr, "usage: hyperscan [options] <regexp> <file>\n"
            "options:\n"
            "   -i                  use case insensitive matching\n"
            "   -g                  enable the global search mode\n"
            "   --repeat=N          repeat the test for N times; pick the best\n"
            "                       result. default to 5.\n");
    exit(rc);
}
