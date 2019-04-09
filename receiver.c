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
    char buffer[buffer_size], ch, *fifo = NULL, *fileName = NULL, *pch = NULL, *path = NULL;
    int fd_fifo = 0, fd_file = 0, b = 0;
    ssize_t bytes = 0;
    size_t lb = 0;

    //fprintf(stdout, "C[%d:%d]\tR[%d:%d]\n", id, getppid(), senderId, getpid());
    //fprintf(stdout, "C[%d]\tR[%d]\n", id, senderId);

    r_files = 0;
    r_bytes = 0;

    /* set up the signal handler*/
    act.sa_handler = _r_alarm_action;

    sigfillset(&(act.sa_mask));

    sigaction(SIGALRM, &act, NULL);

    if (!(fifo = malloc((strlen(common_dir) + digits(senderId) + digits(id) + 15)))) {
        exit(EXIT_FAILURE);
    }

    /* Construct fifo filename*/
    sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, senderId, id);

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d\t[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
        exit(EXIT_FAILURE);
    }

    alarm(30);

    /* Open fifo.*/
    if ((fd_fifo = open(fifo, O_RDONLY)) < 0) {
        fprintf(stderr, "\n%s:%d\t[%s] open error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
        exit(1);
    }

    alarm(0);


    while (1) {

        alarm(30);

        /* Read filename size.*/
        if ((bytes = read(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
            fprintf(stderr, "\n%s:%d\t[%d] read error: '%s'\n", __FILE__, __LINE__, fileNameLength, strerror(errno));
            exit(EXIT_FAILURE);
        }

        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= 0) {
            break;
        }

        if (!(fileName = malloc((size_t) fileNameLength))) {
            exit(EXIT_FAILURE);
        }

        alarm(30);

        /* Read filename.*/
        if ((bytes = read(fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            fprintf(stderr, "\n%s:%d\t[%s] read error: '%s'\n", __FILE__, __LINE__, fileName, strerror(errno));
            exit(EXIT_FAILURE);
        }

        alarm(0);

        r_bytes += bytes;

        lb = strlen(mirror_dir) + digits(senderId) + fileNameLength + 3;

        if (!(path = malloc(lb))) {
            exit(EXIT_FAILURE);
        }

        snprintf(path, lb, "%s/%d/%s", mirror_dir, senderId, fileName);

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

            /* Read file size.*/
            if ((bytes = read(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                fprintf(stderr, "\n%s:%d\tfile size read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                exit(EXIT_FAILURE);
            }

            r_bytes += bytes;

            b = fileSize;

            alarm(0);

            fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR);
            if (fd_file < 0) {
                fprintf(stderr, "\n%s:%d\t[%s] open error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
                exit(EXIT_FAILURE);
            }

            while (b > 0) {
                /* Read bytes from file.*/

                alarm(30);

                /* Read file from fifo.*/
                if ((bytes = read(fd_fifo, buffer, b > buffer_size ? buffer_size : b)) < 0) {
                    fprintf(stderr, "\n%s:%d\tfifo read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }

                r_bytes += bytes;

                alarm(0);

                /* Write the bytes in file.*/
                if (write(fd_file, buffer, (size_t) bytes) == -1) {
                    fprintf(stderr, "\n%s:%d\tfile write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                b -= bytes;
            };

            r_files++;

            close(fd_file);
        }

        free(path);
        free(fileName);
    }


    close(fd_fifo);

    unlink(fifo);

    free(fifo);

    /* Send a signal to the parent process to inform that everything went well!*/
    //kill(getppid(), SIGUSR2);

    //fprintf(stdout, "\nC[%d:%d]\tR[%d:%d]:\tFiles received:[%lu]\tBytes received:[%lu]\n", id, getppid(), senderId,getpid(), r_files, r_bytes);
/*    fprintf(stdout, "\nC[%d:%d] - R[%d:%d]: All files received successfully.\n", id, getppid(), senderId,
            getpid());*/
}
