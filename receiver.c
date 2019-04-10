#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "receiver.h"

#define TIMEOUT 30

unsigned long int r_files = 0, r_bytes = 0;
int r_fd_fifo = 0, r_fd_file = 0;
char r_fifo[PATH_MAX + 1];

void _r_alarm_action(int signo) {
    fprintf(stderr, "\nClient: [%d:%d], %d: alarm timeout!\n", id, getppid(), getpid());

    close(r_fd_fifo);

    unlink(r_fifo);

    kill(getppid(), SIGUSR2);
    exit(EXIT_FAILURE);
}

/**
 * Receiver child*/
void receiver(int sid) {
    __uint16_t fileNameLength = 0;
    __uint32_t fileSize = 0;
    static struct sigaction act;
    unsigned long int offset = 0;
    struct stat s = {0};
    char buffer[buffer_size], ch, fileName[PATH_MAX + 1], *pch = NULL, path[PATH_MAX + 1];
    int b = 0, f = 0;
    ssize_t bytes = 0;

    fprintf(stdout, "C[%d:%d]-R[%d:%d]\n", id, getppid(), sid, getpid());

    r_files = 0;
    r_bytes = 0;

    /* set up the signal handler*/
    act.sa_handler = _r_alarm_action;

    sigfillset(&(act.sa_mask));

    sigaction(SIGALRM, &act, NULL);

    /* Construct fifo filename*/
    if (sprintf(r_fifo, "%s/id%d_to_id%d.fifo", common_dir, sid, id) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    /* Create fifo*/
    if ((f = mkfifo(r_fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d-[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, r_fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(TIMEOUT);

    /* Open fifo.*/
    if ((r_fd_fifo = open(r_fifo, O_RDONLY)) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, r_fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(0);

    while (1) {

        alarm(TIMEOUT);
        if ((bytes = read(r_fd_fifo, &fileNameLength, sizeof(__uint16_t))) < 0) {
            fprintf(stderr, "\n%s:%d-[%d] read error: '%s'\n", __FILE__, __LINE__, fileNameLength, strerror(errno));
            exit(EXIT_FAILURE);
        }
        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= 0) {
            break;
        }

        alarm(TIMEOUT);
        if ((bytes = read(r_fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            fprintf(stderr, "\n%s:%d-[%s] read error: '%s'\n", __FILE__, __LINE__, fileName, strerror(errno));
            exit(EXIT_FAILURE);
        }
        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= PATH_MAX) {
            fileName[fileNameLength] = '\0';
        }

        if (sprintf(path, "%s/%d/%s", mirror_dir, sid, fileName) < 0) {
            fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
        }

        /* Make dirs if not exists.*/
        pch = strchr(path, '/');
        while (pch != NULL) {
            offset = pch - path + 1;
            ch = path[offset];
            path[offset] = '\0';
            //printf("\nTry with: [%s]\n", path);
            if (stat(path, &s)) {
                mkdir(path, S_IRUSR | S_IWUSR | S_IXUSR);
            }
            path[offset] = ch;
            pch = strchr(pch + 1, '/');
        }

        /* Check if path is file.*/
        if (fileName[fileNameLength - 1] != '/') {

            alarm(TIMEOUT);
            if ((bytes = read(r_fd_fifo, &fileSize, sizeof(__uint32_t))) < 0) {
                fprintf(stderr, "\n%s:%d-file size read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            alarm(0);

            r_bytes += bytes;

            b = fileSize;

            if ((r_fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR)) < 0) {
                fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
                exit(EXIT_FAILURE);
            }

            while (b > 0) {

                alarm(TIMEOUT);
                if ((bytes = read(r_fd_fifo, buffer, b > buffer_size ? buffer_size : b)) < 0) {
                    fprintf(stderr, "\n%s:%d-fifo read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                alarm(0);

                r_bytes += bytes;

                if (write(r_fd_file, buffer, (size_t) bytes) == -1) {
                    fprintf(stderr, "\n%s:%d-file write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }

                b -= bytes;
            };

            r_files++;

            close(r_fd_file);
        }
    }

    close(r_fd_fifo);

    if (f == 0) {
        unlink(r_fifo);
    }

    /* Send a signal to the parent process to inform that everything went well!*/
    kill(getppid(), SIGUSR1);

    fprintf(stdout, "\nC%d:%d-R[%d:%d]:-FINISH - Receive %lu files (Total bytes: %lu)\n", id, getppid(), sid,
            getpid(), r_files, r_bytes);

    fprintf(logfile, "br %lu\n", r_bytes);
    fprintf(logfile, "fr %lu\n", r_files);
    fflush(logfile);
}
