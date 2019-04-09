#include <malloc.h>
#include <string.h>
#include <zconf.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include "sender.h"

void _s_alarm_action(int signo) {
    fprintf(stderr, "\nClient: [%d:%d], %d: alarm timeout!\n", id, getppid(), getpid());
    kill(getppid(), SIGUSR2);
    exit(EXIT_FAILURE);
}

/**
 * Read directory & subdirectories recursively*/
void rec_cp(int fd_fifo, const char *path, unsigned long *s_bytes, unsigned long *s_files) {
    char buffer[buffer_size], *dirName = NULL, *fileName = NULL, *r_path = NULL;
    int fileNameLength = 0, fd_file = 0;
    unsigned int fileSize = 0;
    ssize_t bytes = 0, n = 0;
    struct dirent *d = NULL;
    struct stat s = {0};
    DIR *dir = NULL;
    size_t lb;

    if ((dir = opendir(path))) {
        while ((d = readdir(dir))) {

            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
                continue;
            }

            lb = strlen(path) + strlen(d->d_name) + 2;

            if (!(r_path = malloc(lb))) {
                //kill(getppid(), SIGUSR1);
                exit(EXIT_FAILURE);
            }

            /* Construct real path.*/
            snprintf(r_path, lb, "%s/%s", path, d->d_name);

            /* Get file statistics*/
            if (!stat(r_path, &s)) {

                fileSize = (unsigned int) s.st_size;

                fileName = r_path + strlen(input_dir) + 1;

                if (S_ISDIR(s.st_mode)) {

                    fileNameLength = (unsigned short int) strlen(fileName) + 1;

                    alarm(30);
                    /* Write length of filename/directory to pipe.*/
                    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
                        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }
                    alarm(0);

                    *s_bytes += bytes;

                    if (!(dirName = malloc((size_t) fileNameLength))) {
                        fprintf(stderr, "\n%s:%d\tmalloc error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }

                    strcpy(dirName, fileName);
                    strcat(dirName, "/");

                    alarm(30);
                    /* Write relative path of directory to pipe.*/
                    if ((bytes = write(fd_fifo, dirName, (size_t) fileNameLength)) < 0) {
                        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }

                    alarm(0);

                    *s_bytes += bytes;

                    free(dirName);
                    rec_cp(fd_fifo, r_path, s_bytes, s_files);

                } else if (S_ISREG(s.st_mode)) {

                    fileNameLength = (unsigned short int) strlen(fileName);

                    alarm(30);

                    /* Write length of filename to pipe.*/
                    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
                        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }

                    alarm(0);

                    *s_bytes += bytes;

                    alarm(30);

                    /* Write relative path to pipe.*/
                    if ((bytes = write(fd_fifo, fileName, strlen(fileName))) < 0) {
                        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }

                    alarm(0);

                    *s_bytes += bytes;

                    /* Open file*/
                    if ((fd_file = open(r_path, O_RDONLY)) < 0) {
                        fprintf(stderr, "\n%s:%d\tfile %s open error: '%s'\n", __FILE__, __LINE__, r_path,
                                strerror(errno));
                    }

                    alarm(30);

                    /* Write file size.*/
                    if ((bytes = write(fd_fifo, &fileSize, sizeof(unsigned int))) < 0) {
                        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    }

                    alarm(0);

                    *s_bytes += bytes;

                    if (fileSize > 0) {
                        do {
                            if ((n = read(fd_file, buffer, buffer_size)) > 0) {

                                alarm(30);

                                /* Write file.*/
                                if ((bytes = write(fd_fifo, buffer, (size_t) n)) < 0) {
                                    fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__,
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
                fprintf(stderr, "\n%s:%d\t[%s] stat error: '%s'\n", __FILE__, __LINE__, r_path, strerror(errno));
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
    unsigned long int s_files = 0, s_bytes = 0;
    static struct sigaction act;
    char *fifo = NULL;
    int fd_fifo = 0;
    ssize_t bytes = 0;

    //fprintf(stdout, "C[%d:%d]\tS[%d:%d]\n", id, getppid(), receiverId, getpid());
    //fprintf(stdout, "C[%d]\tS[%d]\n", id, receiverId);

    /* set up the signal handler*/
    act.sa_handler = _s_alarm_action;

    sigfillset(&(act.sa_mask));

    sigaction(SIGALRM, &act, NULL);

    if (!(fifo = malloc((strlen(common_dir) + digits(id) + digits(receiverId) + 15)))) {
        fprintf(stderr, "\n%s:%d\tmalloc error: '%s'\n", __FILE__, __LINE__, strerror(errno));
    }

    /* Construct fifo filename*/
    sprintf(fifo, "%s/id%d_to_id%d.fifo", common_dir, id, receiverId);

    /* Create fifo*/
    if ((mkfifo(fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d\t[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
    }

    alarm(30);

    /* Open fifo*/
    if ((fd_fifo = open(fifo, O_WRONLY)) < 0) {
        fprintf(stderr, "\n%s:%d\t[%s] fifo open error: '%s'\n", __FILE__, __LINE__, fifo, strerror(errno));
        exit(1);
    }

    alarm(0);

    /* Write to fifo for each file or folder.*/
    rec_cp(fd_fifo, input_dir, &s_bytes, &s_files);

    fileNameLength = 0;

    alarm(30);

    if ((bytes = write(fd_fifo, &fileNameLength, sizeof(unsigned short int))) < 0) {
        fprintf(stderr, "\n%s:%d\tfifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
        exit(2);
    }

    s_bytes += bytes;

    alarm(0);

    /* Close fifo*/
    close(fd_fifo);

    unlink(fifo);

    free(fifo);

    //kill(getppid(), SIGUSR2);

    //fprintf(stdout, "\nC[%d:%d]\tS[%d:%d]:\tFiles send:[%lu]\tBytes send:[%lu]\n", id, getppid(), receiverId,getpid(), s_files, s_bytes);
/*    fprintf(stdout, "\nC[%d:%d] - S[%d:%d]: All files send successfully.\n", id, getppid(), receiverId,
            getpid());*/

}
