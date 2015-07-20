
/*
 * Copyright 2012 Yichun "agentzh" Zhang
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */


#include <re2/re2.h>
#include <re2/stringpiece.h>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <ctime>
#include <cstdlib>


static void usage(int rc);
static void run_engine(RE2 *re, char *input, size_t len, int global,
    int repeat);


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
    int                  i, global = 0, repeat = 5;
    RE2                 *re;
    char                *re_str, *p;
    char                *input;
    FILE                *f;
    size_t               len;
    long                 rc;

    if (argc < 3) {
        usage(1);
    }

    for (i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            break;
        }

        if (strncmp(argv[i], "-g", 2) == 0) {
            global = 1;
            continue;
        }

        if (strncmp(argv[i], "--repeat=", sizeof("--repeat=") - 1) == 0) {
            repeat = atoi(argv[i] + sizeof("--repeat=") - 1);
            if (repeat <= 0) {
                repeat = 5;
            }

            continue;
        }

        fprintf(stderr, "unknown option: %s\n", argv[i]);
        exit(1);
    }

    if (argc - i != 2) {
        usage(1);
    }

    re_str = argv[i++];
    len = strlen(re_str);

    p = (char *) malloc(len + 1 + sizeof("(?ms)()") - 1);
    if (p == NULL) {
        return 2;
    }

    p[0] = '(';
    p[1] = '?';
    p[2] = 's';
    p[3] = 'm';
    p[4] = ')';
    p[5] = '(';
    memcpy(&p[6], re_str, len);
    p[len + 6] = ')';
    p[len + 7] = '\0';

    //fprintf(stderr, "regex: %s\n", p);

    re = new RE2(p);
    if (re == NULL) {
        return 2;
    }

    free(p);

    if (!re->ok()) {
        delete re;
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

    //fprintf(stderr, "len = %d\n", (int) len);

    if (fseek(f, 0L, SEEK_SET) != 0) {
        perror("seek to file beginning");
        return 1;
    }

    input = (char *) malloc(len + 1);
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

    input[len] = '\0';

    if (fclose(f) != 0) {
        perror("close file");
        return 1;
    }

    run_engine(re, input, len, global, repeat);

    delete re;
    free(input);
    return 0;
}


static void
run_engine(RE2 *re, char *input, size_t len, int global, int repeat)
{
    int                  i, matches = 0;
    bool                 rc = 0;
    size_t               rest;
    re2::StringPiece     cap;
    re2::StringPiece     subj;
    struct timespec      begin, end;
    double               best = -1;
    const char          *p;

    printf("RE2 PartialMatch ");

    for (i = 0; i < repeat; i++) {
        double elapsed;

        matches = 0;
        rest = len;
        subj.set(input, len);

        TIMER_START

        do {
            size_t      size;

            rc = RE2::PartialMatch(subj, *re, &cap);

            if (rc) {
                matches++;
                p = cap.data();
                size = cap.size();
                rest = len - (p - input + size);
                subj.set(p + size, rest);
                /* fprintf(stderr, "matched at %d (rc: %d, size: %d)\n", (int) (p - input), (int) rc, (int) cap.size()); */
            }

        } while (global && rc);

        TIMER_STOP

        if (i == 0 || elapsed < best) {
            best = elapsed;
        }
    }

    if (rc) {
        p = cap.data();
        printf("match (%ld, %ld)", (long) (p - input),
               (long) (p - input + cap.size()));

    } else {
        printf("no match");
    }

    printf(": %.02lf ms elapsed (%d matches found, %d repeated times).\n",
           best, matches, repeat);
}


static void
usage(int rc)
{
    fprintf(stderr, "usage: [options] re2 <regexp> <file>\n"
            "   -g                  enable the global search mode\n"
            "   --repeat=N          repeat the test for N times; pick the best\n"
            "                       result. default to 5.\n");
    exit(rc);
}
