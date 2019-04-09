#include <string.h>
#include <zconf.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include "sender.h"

#define TIMEOUT 30

void _s_alarm_action(int signo) {
    fprintf(stderr, "\nClient: [%d:%d], %d: alarm timeout!\n", id, getppid(), getpid());
    kill(getppid(), SIGUSR2);
    exit(EXIT_FAILURE);
}

/**
 * Read directory & subdirectories recursively*/
void rec_cp(int fd_fifo, const char *_p, unsigned long *s_bytes, unsigned long *s_files) {
    char buffer[buffer_size], dirName[PATH_MAX + 1], *fileName = NULL, path[PATH_MAX + 1];
    int fileNameLength = 0, fd_file = 0;
    unsigned int fileSize = 0;
    ssize_t bytes = 0, n = 0;
    struct dirent *d = NULL;
    struct stat s = {0};
    DIR *dir = NULL;

    if ((dir = opendir(_p))) {
        while ((d = readdir(dir))) {

            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
                continue;
            }

            /* Construct real path.*/
            if (sprintf(path, "%s/%s", _p, d->d_name) < 0) {
                fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
                exit(EXIT_FAILURE);
            }

            /* Get file statistics*/
            if (!stat(path, &s)) {

                fileSize = (unsigned int) s.st_size;

                fileName = path + strlen(input_dir) + 1;

                if (S_ISDIR(s.st_mode)) {

                    fileNameLength = (unsigned short int) strlen(fileName) + 1;

                    alarm(TIMEOUT);
                    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    strcpy(dirName, fileName);
                    strcat(dirName, "/");

                    alarm(TIMEOUT);
                    if ((bytes = write(fd_fifo, dirName, (size_t) fileNameLength)) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    rec_cp(fd_fifo, path, s_bytes, s_files);
                } else if (S_ISREG(s.st_mode)) {

                    fileNameLength = (unsigned short int) strlen(fileName);

                    alarm(TIMEOUT);
                    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    alarm(TIMEOUT);
                    if ((bytes = write(fd_fifo, fileName, strlen(fileName))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    /* Open file*/
                    if ((fd_file = open(path, O_RDONLY)) < 0) {
                        fprintf(stderr, "\n%s:%d-file %s open error: '%s'\n", __FILE__, __LINE__, path,
                                strerror(errno));
                    }

                    alarm(TIMEOUT);
                    if ((bytes = write(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    if (fileSize > 0) {
                        do {
                            if ((n = read(fd_file, buffer, buffer_size)) > 0) {

                                alarm(TIMEOUT);
                                if ((bytes = write(fd_fifo, buffer, (size_t) n)) < 0) {
                                    fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__,
                                            strerror(errno));
                                }
                                alarm(0);

                                *s_bytes += bytes;
                            }
                        } while (n == buffer_size);
                    }
                    (*s_files)++;
                }
            } else {
                fprintf(stderr, "\n%s:%d-[%s] stat error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
            }
        }
        closedir(dir);
    }
}

/**
 * Sender child*/
void sender(int rid) {
    unsigned short int fileNameLength = 0;
    unsigned long int s_files = 0, s_bytes = 0;
    static struct sigaction act;
    char s_fifo[PATH_MAX + 1];
    int fd_fifo = 0, f = 0;
    ssize_t bytes = 0;


    fprintf(stdout, "C[%d:%d]-S[%d:%d]\n", id, getppid(), rid, getpid());

    /* set up the signal handler*/
    act.sa_handler = _s_alarm_action;

    sigfillset(&(act.sa_mask));

    sigaction(SIGALRM, &act, NULL);

    /* Construct fifo filename*/
    if (sprintf(s_fifo, "%s/id%d_to_id%d.fifo", common_dir, id, rid) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    /* Create fifo*/
    if ((f = mkfifo(s_fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d-[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, s_fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(TIMEOUT);

    /* Open fifo*/
    if ((fd_fifo = open(s_fifo, O_WRONLY)) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] fifo open error: '%s'\n", __FILE__, __LINE__, s_fifo, strerror(errno));
        exit(1);
    }

    alarm(0);

    /* Write to fifo for each file or folder.*/
    rec_cp(fd_fifo, input_dir, &s_bytes, &s_files);

    fileNameLength = 0;

    alarm(TIMEOUT);

    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
        exit(2);
    }

    s_bytes += bytes;

    alarm(0);

    /* Close fifo*/
    close(fd_fifo);

    if (f == 0) {
        unlink(s_fifo);
    }

    kill(getppid(), SIGUSR1);

    fprintf(stdout, "\nC%d:%d-S[%d:%d]:-FINISH - Send %lu files (Total bytes %lu)\n", id, getppid(), rid, getpid(),
            s_files,
            s_bytes);
}
