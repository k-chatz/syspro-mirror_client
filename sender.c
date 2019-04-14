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

#define COLOR "\x1B[32m"
#define RESET "\x1B[0m"

#define TIMEOUT 30

unsigned long int s_files = 0, s_bytes = 0;
int s_fd_fifo = 0, s_fd_file = 0, s_fifo_status = 0, r_id = 0;

char s_fifo[PATH_MAX + 1];

void s_clean_up() {
    /* Close fifo if is open.*/
    if (s_fd_fifo > 0) {
        close(s_fd_fifo);
    }

    /* Delete fifo file.*/
    if (s_fifo_status == 0) {
        unlink(s_fifo);
    }

    /* Close file.*/
    if (s_fd_file > 0) {
        close(s_fd_file);
    }
}

void _s_alarm_action(int signal) {
    //fprintf(stderr, "C[%d] SENDER[%d:%d] ALARM TIMEOUT!\n", getppid(), r_id, getpid());
    union sigval sigval = {.sival_int = r_id};
    write(2, "SENDER ALARM TIMEOUT\n", 21);
    s_clean_up();
    if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
        fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
    }
    _exit(EXIT_FAILURE);
}

/**
 * Read directory & subdirectories recursively*/
void rec_cp(const char *_p, char *input_dir, unsigned long int buffer_size) {
    /* Send a signal to the parent process to inform that child got an error.*/
    union sigval sigval = {.sival_int = r_id};
    char buffer[buffer_size], dirName[PATH_MAX + 1], *fileName = NULL, path[PATH_MAX + 1];
    int fileNameLength = 0;
    __uint32_t fileSize = 0;
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
                s_clean_up();
                if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                    fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                }
                exit(EXIT_FAILURE);
            }

            /* Get file statistics*/
            if (!stat(path, &s)) {

                fileSize = (unsigned int) s.st_size;

                fileName = path + strlen(input_dir) + 1;

                if (S_ISDIR(s.st_mode)) {

                    fileNameLength = (__uint16_t) strlen(fileName) + 1;

                    alarm(TIMEOUT);
                    if ((bytes = write(s_fd_fifo, &fileNameLength, sizeof(__uint16_t))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }
                    alarm(0);

                    s_bytes += bytes;

                    strcpy(dirName, fileName);
                    strcat(dirName, "/");

                    alarm(TIMEOUT);
                    if ((bytes = write(s_fd_fifo, dirName, (size_t) fileNameLength)) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }
                    alarm(0);

                    s_bytes += bytes;

                    rec_cp(path, input_dir, buffer_size);
                } else if (S_ISREG(s.st_mode)) {

                    fileNameLength = (__uint16_t) strlen(fileName);

                    alarm(TIMEOUT);
                    if ((bytes = write(s_fd_fifo, &fileNameLength, sizeof(__uint16_t))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }
                    alarm(0);

                    s_bytes += bytes;

                    alarm(TIMEOUT);
                    if ((bytes = write(s_fd_fifo, fileName, strlen(fileName))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }
                    alarm(0);

                    s_bytes += bytes;

                    /* Open file*/
                    if ((s_fd_file = open(path, O_RDONLY)) < 0) {
                        fprintf(stderr, "\n%s:%d-file %s open error: '%s'\n", __FILE__, __LINE__, path,
                                strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }

                    alarm(TIMEOUT);
                    if ((bytes = write(s_fd_fifo, &fileSize, sizeof(__uint32_t))) < 0) {
                        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                        s_clean_up();
                        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                        }
                        exit(EXIT_FAILURE);
                    }
                    alarm(0);

                    s_bytes += bytes;

                    if (fileSize > 0) {
                        do {
                            if ((n = read(s_fd_file, buffer, buffer_size)) > 0) {

                                alarm(TIMEOUT);
                                if ((bytes = write(s_fd_fifo, buffer, (size_t) n)) < 0) {
                                    fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__,
                                            strerror(errno));
                                    s_clean_up();
                                    if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                                        fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                                    }
                                    exit(EXIT_FAILURE);
                                }
                                alarm(0);

                                s_bytes += bytes;
                            }
                        } while (n == buffer_size);
                    }
                    s_files++;
                }
            } else {
                fprintf(stderr, "\n%s:%d-[%s] stat error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
                s_clean_up();
                if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
                    fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
                }
                exit(EXIT_FAILURE);
            }
        }
        closedir(dir);
    }
}

/**
 * Sender child*/
void sender(int receiver_id, int id, char *common_dir, char *input_dir, unsigned long int buffer_size, FILE *logfile) {
    __uint16_t fileNameLength = 0;
    static struct sigaction act;
    union sigval sigval = {.sival_int = receiver_id};

    r_id = receiver_id;
    s_fd_fifo = 0;
    s_fd_file = 0;
    s_files = 0;
    s_bytes = 0;
    s_fifo_status = 0;
    ssize_t bytes = 0;

    fprintf(stdout, COLOR"C[%d:%d] SENDER[%d:%d] STARTED"RESET"\n", id, getppid(), r_id, getpid());

    /* set up the signal handler*/
    act.sa_handler = _s_alarm_action;
    sigfillset(&(act.sa_mask));
    sigaction(SIGALRM, &act, NULL);

    /* Construct fifo filename*/
    if (sprintf(s_fifo, "%s/id%d_to_id%d.fifo", common_dir, id, r_id) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        s_clean_up();
        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
        }
        exit(EXIT_FAILURE);
    }

    /* Create fifo*/
    if ((s_fifo_status = mkfifo(s_fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d-[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, s_fifo, strerror(errno));
        s_clean_up();
        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
        }
        exit(EXIT_FAILURE);
    }

    alarm(TIMEOUT);
    if ((s_fd_fifo = open(s_fifo, O_WRONLY)) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] fifo open error: '%s'\n", __FILE__, __LINE__, s_fifo, strerror(errno));
        s_clean_up();
        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
        }
        exit(EXIT_FAILURE);
    }
    alarm(0);

    /* Write to fifo for each file or folder.*/
    rec_cp(input_dir, input_dir, buffer_size);

    fileNameLength = 0;

    alarm(TIMEOUT);
    if ((bytes = write(s_fd_fifo, &fileNameLength, sizeof(__uint16_t))) < 0) {
        fprintf(stderr, "\n%s:%d-fifo write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
        s_clean_up();
        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
        }
        exit(EXIT_FAILURE);
    }
    alarm(0);

    s_bytes += bytes;

    /* Close fifo*/
    close(s_fd_fifo);

    /* Delete fifo file.*/
    if (s_fifo_status == 0) {
        unlink(s_fifo);
    }

    /* Send a signal to the parent process to inform that everything went well!*/
    if ((sigqueue(getppid(), SIGUSR1, sigval)) < 0) {
        fprintf(stderr, "\n%s:%d-sigqueue error\n", __FILE__, __LINE__);
        if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
            fprintf(stderr, "\n%s:%d-SIGUSR2 error\n", __FILE__, __LINE__);
        }
        exit(EXIT_FAILURE);
    }

    fprintf(stdout, COLOR"C[%d:%d] SENDER[%d:%d] FINISH - Send %lu files (Total bytes %lu)"RESET"\n",
            id, getppid(), r_id, getpid(), s_files, s_bytes);

    fprintf(logfile, "bs %lu\n", s_bytes);
    fflush(logfile);

    fprintf(logfile, "fs %lu\n", s_files);
    fflush(logfile);
}
