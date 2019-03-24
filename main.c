#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>

typedef void *pointer;

void wrongOptionValue(char *opt, char *val) {
    fprintf(stdout, "Wrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

void readOptions(
        int argc,
        char **argv,
        unsigned long int *id,              /*id*/
        char **common_dir,                  /*common_dir*/
        char **input_dir,                   /*input_dir*/
        char **mirror_dir,                  /*input_dir*/
        unsigned long int *buffer_size,     /*buffer_size*/
        char **log_file                     /*log_file*/
) {
    int i;
    char *opt, *optVal;
    for (i = 1; i < argc; ++i) {
        opt = argv[i];
        optVal = argv[i + 1];
        if (strcmp(opt, "-n") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *id = (unsigned long int) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-c") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *common_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-i") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *input_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-m") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *mirror_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-b") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *buffer_size = (unsigned long int) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-l") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *log_file = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    unsigned long int id = 0, buffer_size = 0;
    char *common_dir = NULL, *input_dir = NULL, *mirror_dir = NULL, *log_file = NULL;
    struct stat s = {0};

    /*Read argument options from command line*/
    readOptions(argc, argv, &id, &common_dir, &input_dir, &mirror_dir, &buffer_size, &log_file);

    assert(id > 0);
    assert(common_dir != NULL);
    assert(input_dir != NULL);
    assert(mirror_dir != NULL);
    assert(buffer_size > 0);
    assert(log_file != NULL);

    if (!stat(input_dir, &s)) {
        if (S_ISDIR(s.st_mode)) {
            printf("'%s' is a directory.\n", input_dir);
        } else {
            printf("'%s' is not a directory.\n", input_dir);
        }
    } else {
        perror("stat()");
    }

    return 0;
}