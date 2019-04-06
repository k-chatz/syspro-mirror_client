#include <malloc.h>
#include <string.h>
#include <zconf.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include "sender.h"

/**
 * Read directory & subdirectories recursively*/
void rec_cp(int fd_fifo, const char *path) {
    struct dirent *d = NULL;
    char *r_path = NULL;
    DIR *dir = NULL;
    size_t lb;

    int fileNameLength = 0, fd_file = 0;
    unsigned int fileSize = 0;
    struct stat s = {0};
    char buffer[buffer_size];
    ssize_t n = 0;
    char *dirName = NULL;
    char *fileName = NULL;

    if ((dir = opendir(path))) {
        while ((d = readdir(dir))) {

            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
                continue;
            }

            lb = strlen(path) + strlen(d->d_name) + 2;

            if (!(r_path = malloc(lb))) {
                perror("malloc");
            }

            /* Construct real path.*/
            snprintf(r_path, lb, "%s/%s", path, d->d_name);

            /* Get file statistics*/
            if (!stat(r_path, &s)) {

                fileSize = (unsigned int) s.st_size;

                fileName = r_path + strlen(input_dir) + 1;

                if (S_ISDIR(s.st_mode)) {
                    printf("directory: [%s], size: [%d]\n", fileName, (int) s.st_size);

                    fileNameLength = (unsigned short int) strlen(fileName) + 1;

                    /* Write length of filename/directory to pipe.*/
                    alarm(30);
                    if (write(fd_fifo, &fileNameLength, sizeof(unsigned short int)) < 0) {
                        perror("Error in Writing");
                    }
                    alarm(0);


                    if (!(dirName = malloc((size_t) fileNameLength))) {
                        perror("malloc");
                    }

                    strcpy(dirName, fileName);
                    strcat(dirName, "/");

                    alarm(30);

                    /* Write relative path of directory to pipe.*/
                    if (write(fd_fifo, dirName, (size_t) fileNameLength) < 0) {
                        perror("Error in Writing");
                    }

                    alarm(0);

                    free(dirName);
                    rec_cp(fd_fifo, r_path);

                } else if (S_ISREG(s.st_mode)) {
                    printf("file: [%s], size: [%d]\n", fileName, (int) s.st_size);

                    fileNameLength = (unsigned short int) strlen(fileName);

                    alarm(30);

                    /* Write length of filename to pipe.*/
                    if (write(fd_fifo, &fileNameLength, sizeof(unsigned short int)) < 0) {
                        perror("Error in Writing");
                    }

                    alarm(0);

                    alarm(30);

                    /* Write relative path to pipe.*/
                    if (write(fd_fifo, fileName, strlen(fileName)) < 0) {
                        perror("Error in Writing");
                    }

                    alarm(0);

                    /* Open file*/
                    if ((fd_file = open(r_path, O_RDONLY)) < 0) {
                        printf("Open call fail");
                    }

                    alarm(30);

                    /* Write file size.*/
                    if (write(fd_fifo, &fileSize, sizeof(unsigned int)) < 0) {
                        perror("Error in Writing");
                    }

                    alarm(0);

                    if (fileSize > 0) {
                        do {
                            if ((n = read(fd_file, buffer, buffer_size)) > 0) {

                                alarm(30);

                                /* Write file.*/
                                if (write(fd_fifo, buffer, (size_t) n) < 0) {
                                    perror("Error in Writing");
                                }

                                alarm(0);

                            }
                        } while (n == buffer_size);
                    }
                }
            } else {
                perror("File not found");
            }
            free(r_path);
        }
        closedir(dir);
    }
}

/**
 * Sender child*/
void sender(int receiverId) {
    unsigned short int fileNameLength = 0;
    char *fifo = NULL;
    int fd_fifo = 0;

    printf("\nSENDER PID: [%d], PARENT PID: [%d]\n", getpid(), getppid());

    if (!(fifo = malloc((strlen(common_dir) + digits(id) + digits(receiverId) + 15)))) {
        perror("malloc");
    }

    /* Construct fifo filename*/
    sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, id, receiverId);

    printf("FIFO: [%s]\n", fifo);

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        perror("can't create fifo");
    }

    alarm(30);

    /* Open fifo*/
    if ((fd_fifo = open(fifo, O_WRONLY)) < 0) {
        perror("Open fifo error");
        exit(1);
    }

    alarm(0);

    /* Write to fifo for each file or folder.*/
    rec_cp(fd_fifo, input_dir);

    fileNameLength = 0;

    alarm(30);

    if (write(fd_fifo, &fileNameLength, sizeof(unsigned short int)) < 0) {
        perror("Error in Writing");
        exit(2);
    }

    alarm(0);

    /* Close fifo*/
    close(fd_fifo);

    free(fifo);
}