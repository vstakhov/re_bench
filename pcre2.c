
/*
 * Copyright 2012 Yichun "agentzh" Zhang
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */


#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include "getcputime.h"


static void usage(int rc);
static void run_engines(pcre2_code *re, unsigned engine_types,
    pcre2_match_data *match_data, const char *input, size_t len,
    int global, int repeat);


enum {
    ENGINE_DEFAULT = (1 << 0),
    ENGINE_JIT     = (1 << 1),
    ENGINE_DFA     = (1 << 2),
    MATCH_LIMIT    = 0,
};


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
    int                  flags = PCRE2_DOTALL | PCRE2_MULTILINE;
    int                  global = 0;
    int                  repeat = 5;
    unsigned             engine_types = 0;
    unsigned             i;
    int                  err_code;
    char                *input;
    FILE                *f;
    size_t               len;
    long                 rc;
    pcre2_code          *re;
    PCRE2_SIZE           err_offset;
    pcre2_match_data    *match_data;

    pcre2_compile_context *comp_ctx;

    if (argc < 3) {
        usage(1);
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }

        if (strncmp(argv[i], "--default",
                    sizeof("--default") - 1) == 0)
        {
            engine_types |= ENGINE_DEFAULT;

        } else if (strncmp(argv[i], "--jit", sizeof("--jit") - 1)
                   == 0)
        {
            engine_types |= ENGINE_JIT;

        } else if (strncmp(argv[i], "--dfa", sizeof("--dfa") - 1)
                   == 0)
        {
            engine_types |= ENGINE_DFA;

        } else if (strncmp(argv[i], "--repeat=", sizeof("--repeat=") - 1)
                   == 0)
        {
            repeat = atoi(argv[i] + sizeof("--repeat=") - 1);
            if (repeat <= 0) {
                repeat = 5;
            }

        } else if (strncmp(argv[i], "-i", 2) == 0) {
            flags |= PCRE2_CASELESS;

        } else if (strncmp(argv[i], "-g", 2) == 0) {
            global = 1;

        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            exit(1);
        }
    }

    if (engine_types == 0) {
        fprintf(stderr, "No engine specified.\n");
        exit(1);
    }

    if (argc - i != 2) {
        usage(1);
    }

    comp_ctx = pcre2_compile_context_create(NULL);
    if (comp_ctx == NULL) {
        fprintf(stderr, "PCRE2 cannot allocate compile context\n");
        exit(1);
    }

    re = pcre2_compile((PCRE2_SPTR8) argv[i++], /* the pattern */
                       PCRE2_ZERO_TERMINATED,   /* length */
                       flags,                   /* options */
                       &err_code,               /* for error code */
                       &err_offset,             /* for error offset */
                       comp_ctx);               /* use default character tables */
    if (re == NULL) {
        fprintf(stderr, "[error] pos %d: %d\n", (int) err_offset, err_code);
        return 2;
    }

    pcre2_compile_context_free(comp_ctx);

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

    if (engine_types & ENGINE_DFA) {
        match_data = pcre2_match_data_create(32, NULL);

    } else {
        match_data = pcre2_match_data_create_from_pattern(re, NULL);
    }

    if (match_data == NULL) {
        fprintf(stderr, "PCRE2 cannot allocate match data\n");
        exit(1);
    }

    run_engines(re, engine_types, match_data, input, len, global, repeat);

    free(input);
    pcre2_match_data_free(match_data);
    pcre2_code_free(re);

    return 0;
}


static void
run_engines(pcre2_code *re, unsigned engine_types, pcre2_match_data *match_data,
    const char *input, size_t len, int global, int repeat)
{
    int                  i, n, matches = 0;
    int                  rc = -1;
    size_t               rest;
    double               begin, end, best = -1;
    const char          *p;
    PCRE2_SIZE          *ovector;
    pcre2_match_context *match_ctx;

    ovector = pcre2_get_ovector_pointer(match_data);

    if (engine_types & ENGINE_DEFAULT) {

        printf("PCRE2 interp ");

        match_ctx = pcre2_match_context_create(NULL);
        if (match_ctx == NULL) {
            fprintf(stderr, "PCRE2 interp cannot allocate match context\n");
            exit(2);
        }

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre2_match(
                        re,                 /* the compiled pattern */
                        (PCRE2_SPTR8) p,    /* the subject string */
                        rest,               /* the length of the subject */
                        0,                  /* start at offset 0 in the subject */
                        0,                  /* default options */
                        match_data,         /* match data */
                        match_ctx);         /* match context */

                if (rc > 0) {
                    matches++;
                    p += ovector[1];
                    rest -= (int) ovector[1];
                    /*
                    fprintf(stderr, "matched at %d (rc: %d, size: %d)\n",
                            (int) (p - input), rc, ovector[1] - ovector[0]);
                    */
                }

            } while (global && rc > 0);

            TIMER_STOP

            if (i == 0 || elapsed < best) {
                best = elapsed;
            }
        }

        if (rc == 0) {
            fprintf(stderr, "capture size too small");
            exit(2);
        }

        if (rc == PCRE2_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", (int) ovector[n], (int) ovector[n + 1]);
            }
        }

        printf(": %.05lf ms elapsed (%d matches found, %d repeated times).\n",
               best * 1e3, matches, repeat);

        pcre2_match_context_free(match_ctx);
    }

    if (engine_types & ENGINE_DFA) {
        int work_space[4096];

        match_ctx = pcre2_match_context_create(NULL);
        if (match_ctx == NULL) {
            fprintf(stderr, "PCRE2 interp cannot allocate match context\n");
            exit(2);
        }

        printf("PCRE2 DFA ");

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre2_dfa_match(
                        re,                 /* the compiled pattern */
                        (PCRE2_SPTR8) p,    /* the subject string */
                        rest,               /* the length of the subject */
                        0,                  /* start at offset 0 in the subject */
                        0,                  /* default options */
                        match_data,         /* match data */
                        match_ctx,          /* match context */
                        work_space,         /* work space */
                        4096);              /* number of elements (NOT size in bytes) */

                if (rc > 0) {
                    matches++;
                    p += ovector[1];
                    rest -= ovector[1];
                }

            } while (global && rc > 0);

            TIMER_STOP

            if (i == 0 || elapsed < best) {
                best = elapsed;
            }
        }

        if (rc == 0) {
            rc = 1;
        }

        if (rc == PCRE2_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", (int) ovector[n], (int) ovector[n + 1]);
            }
        }

        printf(": %.05lf ms elapsed (%d matches found, %d repeated times).\n",
               best * 1e3, matches, repeat);

        pcre2_match_context_free(match_ctx);
    }

    if (engine_types & ENGINE_JIT) {
        pcre2_jit_stack *stack = NULL;

        match_ctx = pcre2_match_context_create(NULL);
        if (match_ctx == NULL) {
            fprintf(stderr, "PCRE2 interp cannot allocate match context\n");
            exit(2);
        }

        if (pcre2_jit_compile(re, PCRE2_JIT_COMPLETE)) {
            fprintf(stderr, "PCRE2 JIT compilation failed\n");
            exit(1);
        }

        stack = pcre2_jit_stack_create(65536, 65536, NULL);
        if (stack == NULL) {
            fprintf(stderr, "PCRE2 JIT cannot allocate JIT stack\n");
            exit(1);
        }

        pcre2_jit_stack_assign(match_ctx, NULL, stack);

        printf("PCRE2 JIT ");

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre2_jit_match(
                        re,			/* the compiled pattern */
                        (PCRE2_SPTR8) p,	/* the subject string */
                        rest,			/* the length of the subject */
                        0,			/* start at offset 0 in the subject */
                        0,			/* default options */
                        match_data,		/* match data */
                        match_ctx);		/* match context */

                if (rc > 0) {
                    matches++;
                    p += ovector[1];
                    rest -= ovector[1];
                }

            } while (global && rc > 0);

            TIMER_STOP

            if (i == 0 || elapsed < best) {
                best = elapsed;
            }
        }

        if (rc == 0) {
            fprintf(stderr, "capture size too small");
            exit(2);
        }

        if (rc == PCRE2_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", (int) ovector[n], (int) ovector[n + 1]);
            }
        }

        printf(": %.05lf ms elapsed (%d matches found, %d repeated times).\n",
               best * 1e3, matches, repeat);

        pcre2_jit_stack_free(stack);
        pcre2_match_context_free(match_ctx);
    }
}


static void
usage(int rc)
{
    fprintf(stderr, "usage: pcre [options] <regexp> <file>\n"
            "options:\n"
            "   -i                  use case insensitive matching\n"
            "   --default           use the default PCRE2 engine\n"
            "   --dfa               use the PCRE2 DFA engine\n"
            "   --jit               use the PCRE2 JIT engine\n"
            "   -g                  enable the global search mode\n"
            "   --repeat=N          repeat the test for N times; pick the best\n"
            "                       result. default to 5.\n");
    exit(rc);
}
