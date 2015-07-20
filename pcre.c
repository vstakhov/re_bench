
/*
 * Copyright 2012 Yichun "agentzh" Zhang
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */


#include <pcre.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>


static void usage(int rc);
static void run_engines(pcre *re, unsigned engine_types, int* ovector,
    int ovecsize, const char *input, size_t len, int global, int repeat);


enum {
    ENGINE_DEFAULT = (1 << 0),
    ENGINE_JIT     = (1 << 1),
    ENGINE_DFA     = (1 << 2),
    MATCH_LIMIT    = 0,
};


#define TIMER_START                                                          \
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &begin) == -1) {         \
            perror("clock_gettime");                                         \
            exit(2);                                                         \
        }


#define TIMER_STOP                                                           \
        if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end) == -1) {           \
            perror("clock_gettime");                                         \
            exit(2);                                                         \
        }                                                                    \
        elapsed = (end.tv_sec - begin.tv_sec) * 1e3 + (end.tv_nsec - begin.tv_nsec) * 1e-6;


int
main(int argc, char **argv)
{
    int                  flags = PCRE_DOTALL | PCRE_MULTILINE;
    int                  global = 0;
    int                 *ovector;
    int                  ovecsize, repeat = 5;
    unsigned             engine_types = 0;
    unsigned             i;
    int                  err_offset = -1;
    pcre                *re;
    int                  ncaps;
    char                *input;
    FILE                *f;
    size_t               len;
    long                 rc;
    const char          *errstr;

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
            flags |= PCRE_CASELESS;

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

    re = pcre_compile(argv[i++], flags, &errstr, &err_offset, NULL);
    if (re == NULL) {
        fprintf(stderr, "[error] pos %d: %s\n", err_offset, errstr);
        return 2;
    }

    if (pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &ncaps) < 0) {
        fprintf(stderr, "failed to get capture count.\n");
        return 2;
    }

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

    ovecsize = (ncaps + 1) * 3;
    ovector = malloc(ovecsize * sizeof(int));
    if (ovector == NULL) {
        perror("malloc");
        return 1;
    }

    run_engines(re, engine_types, ovector, ovecsize, input, len, global, repeat);

    free(ovector);
    free(input);
    pcre_free(re);

    return 0;
}


static void
run_engines(pcre *re, unsigned engine_types, int *ovector, int ovecsize,
    const char *input, size_t len, int global, int repeat)
{
    int                  i, n, matches = 0;
    int                  rc = -1;
    size_t               rest;
    pcre_extra          *extra;
    struct timespec      begin, end;
    double               best = -1;
    const char          *errstr = NULL, *p;

    if (engine_types & ENGINE_DEFAULT) {

        printf("PCRE interp ");

        extra = pcre_study(re, 0, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }

        extra->match_limit = MATCH_LIMIT;

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre_exec(re, extra, p, rest, 0, 0, ovector, ovecsize);

                if (rc > 0) {
                    matches++;
                    p += ovector[1];
                    rest -= ovector[1];
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

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %.02lf ms elapsed (%d matches found, %d repeated times).\n",
               best, matches, repeat);

        if (extra) {
            pcre_free_study(extra);
            extra = NULL;
        }
    }

    if (engine_types & ENGINE_JIT) {

        printf("PCRE JIT ");

        extra = pcre_study(re, PCRE_STUDY_JIT_COMPILE, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }
        extra->match_limit = MATCH_LIMIT;

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre_exec(re, extra, p, rest, 0, 0, ovector, ovecsize);

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

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %.02lf ms elapsed (%d matches found, %d repeated times).\n",
               best, matches, repeat);

        if (extra) {
            pcre_free_study(extra);
            extra = NULL;
        }
    }

    if (engine_types & ENGINE_DFA) {
        int ws[100];

        ovecsize = 2;

        printf("PCRE DFA ");

        extra = pcre_study(re, 0, &errstr);
        if (errstr != NULL) {
            fprintf(stderr, "failed to study the regex: %s", errstr);
            exit(2);
        }

        for (i = 0; i < repeat; i++) {
            double elapsed;

            matches = 0;
            p = input;
            rest = len;

            TIMER_START

            do {
                rc = pcre_dfa_exec(re, extra, p, rest, 0, 0, ovector, ovecsize,
                                   ws, sizeof(ws)/sizeof(ws[0]));

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

        if (rc == PCRE_ERROR_NOMATCH) {
            printf("no match");

        } else if (rc < 0) {
            printf("error: %d", rc);

        } else if (rc > 0) {
            printf("match");
            for (i = 0, n = 0; i < rc; i++, n += 2) {
                printf(" (%d, %d)", ovector[n], ovector[n + 1]);
            }
        }

        printf(": %.02lf ms elapsed (%d matches found, %d repeated times).\n",
               best, matches, repeat);

        if (extra) {
            pcre_free_study(extra);
            extra = NULL;
        }
    }
}


static void
usage(int rc)
{
    fprintf(stderr, "usage: pcre [options] <regexp> <file>\n"
            "options:\n"
            "   -i                  use case insensitive matching\n"
            "   --default           use the default PCRE engine\n"
            "   --dfa               use the PCRE DFA engine\n"
            "   --jit               use the PCRE JIT engine\n"
            "   -g                  enable the global search mode\n"
            "   --repeat=N          repeat the test for N times; pick the best\n"
            "                       result. default to 5.\n");
    exit(rc);
}
