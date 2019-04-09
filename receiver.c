#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "receiver.h"

unsigned long int r_files = 0, r_bytes = 0;

void _r_alarm_action(int signo) {
    fprintf(stderr, "\nClient: [%d:%d], %d: alarm timeout!\n", id, getppid(), getpid());
    kill(getppid(), SIGUSR2);
    exit(EXIT_FAILURE);
}

/**
 * Receiver child*/
void receiver(int senderId) {
    unsigned short int fileNameLength = 0;
    static struct sigaction act;
    unsigned int fileSize = 0;
    unsigned long int offset = 0;
    struct stat s = {0};
    char buffer[buffer_size], ch, fifo[PATH_MAX + 1], fileName[PATH_MAX + 1], *pch = NULL, path[PATH_MAX + 1];
    int fd_fifo = 0, fd_file = 0, b = 0;
    ssize_t bytes = 0;

    //fprintf(stdout, "C[%d:%d]-R[%d:%d]\n", id, getppid(), senderId, getpid());
    //fprintf(stdout, "C[%d]-R[%d]\n", id, senderId);

    r_files = 0;
    r_bytes = 0;

    /* set up the signal handler*/
    act.sa_handler = _r_alarm_action;

    sigfillset(&(act.sa_mask));

    sigaction(SIGALRM, &act, NULL);

    /* Construct fifo filename*/
    if (sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, senderId, id) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d-[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(30);

    /* Open fifo.*/
    if ((fd_fifo = open(fifo, O_RDONLY)) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(0);

    while (1) {

        alarm(30);
        if ((bytes = read(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
            fprintf(stderr, "\n%s:%d-[%d] read error: '%s'\n", __FILE__, __LINE__, fileNameLength, strerror(errno));
            exit(EXIT_FAILURE);
        }
        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= 0) {
            break;
        }

        alarm(30);
        if ((bytes = read(fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            fprintf(stderr, "\n%s:%d-[%s] read error: '%s'\n", __FILE__, __LINE__, fileName, strerror(errno));
            exit(EXIT_FAILURE);
        }
        alarm(0);

        r_bytes += bytes;

        if (sprintf(path, "%s/%d/%s", mirror_dir, senderId, fileName) < 0) {
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

            alarm(30);
            if ((bytes = read(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                fprintf(stderr, "\n%s:%d-file size read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                exit(EXIT_FAILURE);
            }
            alarm(0);

            r_bytes += bytes;

            b = fileSize;

            if ((fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR)) < 0) {
                fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
                exit(EXIT_FAILURE);
            }

            while (b > 0) {

                alarm(30);
                if ((bytes = read(fd_fifo, buffer, b > buffer_size ? buffer_size : b)) < 0) {
                    fprintf(stderr, "\n%s:%d-fifo read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                alarm(0);

                r_bytes += bytes;

                if (write(fd_file, buffer, (size_t) bytes) == -1) {
                    fprintf(stderr, "\n%s:%d-file write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }

                b -= bytes;
            };

            r_files++;

            close(fd_file);
        }
    }

    close(fd_fifo);

    unlink(fifo);

    /* Send a signal to the parent process to inform that everything went well!*/
    //kill(getppid(), SIGUSR2);

    //fprintf(stdout, "\nC[%d:%d]-R[%d:%d]:-Files received:[%lu]-Bytes received:[%lu]\n", id, getppid(), senderId,getpid(), r_files, r_bytes);
/*    fprintf(stdout, "\nC[%d:%d] - R[%d:%d]: All files received successfully.\n", id, getppid(), senderId,
            getpid());*/
}
