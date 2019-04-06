#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "receiver.h"

/**
 * Receiver child*/
void receiver(int senderId) {
    unsigned short int fileNameLength = 0;
    unsigned int fileSize = 0;
    unsigned long int offset = 0;
    struct stat s = {0};
    char buffer[buffer_size], ch, *fifo = NULL, *fileName = NULL, *pch = NULL, *path = NULL;
    int fd_fifo = 0, fd_file = 0, b = 0;
    ssize_t bytes = 0;
    size_t lb = 0;

    printf("\nRECEIVER PID: [%d], PARENT PID: [%d]\n", getpid(), getppid());

    if (!(fifo = malloc((strlen(common_dir) + digits(senderId) + digits(id) + 15)))) {
        perror("malloc");
    }

    /* Construct fifo filename*/
    sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, senderId, id);

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        perror("can't create fifo");
    }

    alarm(30);

    fd_fifo = open(fifo, O_RDONLY);

    alarm(0);

    while (1) {

        alarm(30);

        /* Read filename size.*/
        if ((read(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
            perror("problem in reading");
        }

        alarm(0);

        if (fileNameLength <= 0) {
            break;
        }

        if (!(fileName = malloc((size_t) fileNameLength))) {
            perror("malloc filename");
        }

        alarm(30);

        /* Read filename.*/
        if ((read(fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            perror("problem in reading");
        }

        alarm(0);

        lb = strlen(mirror_dir) + digits(senderId) + fileNameLength + 3;

        if (!(path = malloc(lb))) {
            perror("malloc r_path");
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
            if ((read(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                perror("problem in reading");
            }

            b = fileSize;

            alarm(0);

            fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR);
            if (fd_file < 0) {
                perror("Open file call fail");
            }

            while (b > 0) {
                /* Read bytes from file.*/

                alarm(30);

                bytes = read(fd_fifo, buffer, b > buffer_size ? buffer_size : b);

                alarm(0);

                /* Write the bytes in file.*/
                if (write(fd_file, buffer, (size_t) bytes) == -1) {
                    perror("Error in Writing");
                }
                b -= bytes;
            };

            fprintf(stdout, "cp %s %d\n", path, fileSize);

            close(fd_file);
        }

        free(path);
        free(fileName);
    }

    close(fd_fifo);

    unlink(fifo);

    free(fifo);
}