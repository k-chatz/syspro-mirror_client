#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "receiver.h"

#define TIMEOUT 10

unsigned long int r_files = 0, r_bytes = 0;
int r_fd_fifo = 0, r_fd_file = 0, r_fifo_status = 0, s_id = 0;
char r_fifo[PATH_MAX + 1];

void r_term() {
    /* Send a signal to the parent process to inform that child got an error.*/
    union sigval sigval = {.sival_int = s_id};
    /* Close fifo if is open.*/
    if (r_fd_fifo > 0) {
        close(r_fd_fifo);
    }

    /* Delete fifo file.*/
    if (r_fifo_status == 0) {
        unlink(r_fifo);
    }

    /* Close file.*/
    if (r_fd_file > 0) {
        close(r_fd_file);
    }

    if ((sigqueue(getppid(), SIGUSR2, sigval)) < 0) {
        fprintf(stderr, "\n%s:%d-sigqueue error\n", __FILE__, __LINE__);
    }

    _exit(EXIT_FAILURE);
}

void _r_alarm_action(int signal) {
    fprintf(stderr, "C[%d] RECEIVER[%d:%d] ALARM TIMEOUT!\n", getppid(), s_id, getpid());
    r_term();
}

/**
 * Receiver child*/
void receiver(int sender_id, int id, char *common_dir, char *input_dir, char *mirror_dir, unsigned long int buffer_size,
              FILE *logfile) {
    char path[PATH_MAX + 1], fileName[PATH_MAX + 1], buffer[buffer_size], ch, *pch = NULL;
    unsigned long int offset = 0;
    static struct sigaction act;
    struct stat s = {0};
    int b = 0;
    union sigval sigval = {.sival_int = sender_id};
    s_id = sender_id;
    __uint16_t fileNameLength = 0;
    __uint32_t fileSize = 0;
    ssize_t bytes = 0;

    fprintf(stdout, "C[%d:%d] RECEIVER[%d:%d] STARTUP\n", id, getppid(), s_id, getpid());

    r_fd_fifo = 0;
    r_fd_file = 0;
    r_files = 0;
    r_bytes = 0;
    r_fifo_status = 0;

    /* set up the signal handler*/
    act.sa_handler = _r_alarm_action;
    sigfillset(&(act.sa_mask));
    sigaction(SIGALRM, &act, NULL);

    /* Construct fifo filename*/
    if (sprintf(r_fifo, "%s/id%d_to_id%d.fifo", common_dir, s_id, id) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        r_term();
    }

    /* Create fifo*/
    if ((r_fifo_status = mkfifo(r_fifo, S_IRUSR | S_IWUSR | S_IXUSR) < 0) && (errno != EEXIST)) {
        fprintf(stderr, "\n%s:%d-[%s] mkfifo error: '%s'\n", __FILE__, __LINE__, r_fifo, strerror(errno));
        r_term();
    }

    alarm(TIMEOUT);
    if ((r_fd_fifo = open(r_fifo, O_RDONLY)) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, r_fifo, strerror(errno));
        r_term();
    }
    alarm(0);

    while (1) {

        alarm(TIMEOUT);
        if ((bytes = read(r_fd_fifo, &fileNameLength, sizeof(__uint16_t))) < 0) {
            fprintf(stderr, "\n%s:%d-[%d] read error: '%s'\n", __FILE__, __LINE__, fileNameLength, strerror(errno));
            r_term();
        }
        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= 0) {
            break;
        }

        alarm(TIMEOUT);
        if ((bytes = read(r_fd_fifo, fileName, (size_t) fileNameLength)) < 0) {
            fprintf(stderr, "\n%s:%d-[%s] read error: '%s'\n", __FILE__, __LINE__, fileName, strerror(errno));
            r_term();
        }
        alarm(0);

        r_bytes += bytes;

        if (fileNameLength <= PATH_MAX) {
            fileName[fileNameLength] = '\0';
        }

        if (sprintf(path, "%s/%d/%s", mirror_dir, s_id, fileName) < 0) {
            fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
            r_term();
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
                r_term();
            }
            alarm(0);

            r_bytes += bytes;

            b = fileSize;

            if ((r_fd_file = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR | S_IXUSR)) < 0) {
                fprintf(stderr, "\n%s:%d-file '%s' open error: '%s'\n", __FILE__, __LINE__, path, strerror(errno));
                r_term();
            }

            while (b > 0) {
                alarm(TIMEOUT);
                if ((bytes = read(r_fd_fifo, buffer, b > buffer_size ? buffer_size : b)) < 0) {
                    fprintf(stderr, "\n%s:%d-fifo read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    r_term();
                }
                alarm(0);

                r_bytes += bytes;

                if (write(r_fd_file, buffer, (size_t) bytes) == -1) {
                    fprintf(stderr, "\n%s:%d-file write error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                    r_term();
                }

                b -= bytes;
            };

            r_files++;

            close(r_fd_file);
        }
    }

    close(r_fd_fifo);

    if (r_fifo_status == 0) {
        unlink(r_fifo);
    }

    /* Send a signal to the parent process to inform that everything went well!*/
    if ((sigqueue(getppid(), SIGUSR1, sigval)) < 0) {
        fprintf(stderr, "\n%s:%d-sigqueue error\n", __FILE__, __LINE__);
    }

    fprintf(stdout, "\nC%d:%d-RECEIVER[%d:%d]:-FINISH - Receive %lu files (Total bytes: %lu)\n",
            id, getppid(), s_id, getpid(), r_files, r_bytes);

    fprintf(logfile, "br %lu\n", r_bytes);
    fflush(logfile);

    fprintf(logfile, "fr %lu\n", r_files);
    fflush(logfile);
}
