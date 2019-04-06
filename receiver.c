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
    int fd_fifo = 0, fd_file = 0;
    ssize_t bytes = 0;
    size_t lb = 0;

    printf("\nRECEIVER PID: [%d], PARENT PID: [%d]\n", getpid(), getppid());

    if (!(fifo = malloc((strlen(common_dir) + digits(senderId) + digits(id) + 15)))) {
        perror("malloc");
    }

    /* Construct fifo filename*/
    sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, senderId, id);

    printf("FIFO: [%s]\n", fifo);

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        perror("can't create fifo");
    }

    fd_fifo = open(fifo, O_RDONLY);

    while (1) {

        /* Read filename size.*/
        if ((read(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
            perror("problem in reading");
        }

        if (fileNameLength <= 0) {
            break;
        }

        if (!(fileName = malloc((size_t) fileNameLength))) {
            perror("malloc filename");
        }

        /* Read filename.*/
        if ((read(fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            perror("problem in reading");
        }

        lb = strlen(mirror_dir) + fileNameLength + 2;

        if (!(path = malloc(lb))) {
            perror("malloc r_path");
        }

        snprintf(path, lb, "%s/%s", mirror_dir, fileName);

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

            /* Read file size.*/
            if ((read(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                perror("problem in reading");
            }

            fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR);
            if (fd_file < 0) {
                perror("Open file call fail");
            }

            while (fileSize > 0) {
                /* Read bytes from file.*/
                bytes = read(fd_fifo, buffer, fileSize > buffer_size ? buffer_size : fileSize);
                printf("\nn:[%d], buffer: [%s], file_size: [%d]\n", (int) bytes, buffer, fileSize);

                /* Write the bytes in file.*/
                if (write(fd_file, buffer, (size_t) bytes) == -1) {
                    perror("Error in Writing");
                }

                fileSize -= bytes;
            };

            close(fd_file);
        }

        free(path);
        free(fileName);
    }

    close(fd_fifo);
    free(buffer);
}