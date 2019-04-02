#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/inotify.h>
#include <errno.h>
#include <zconf.h>
#include <signal.h>
#include <dirent.h>
#include <math.h>
#include <stdbool.h>
#include "hash.h"

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))
#define TRIES 3

typedef void *pointer;

Hashtable clientsHT = NULL;
char *common_dir = NULL, *input_dir = NULL, *mirror_dir = NULL, *log_file = NULL;
int id = 0;
__pid_t sender_pid = 0, receiver_pid = 0;
bool quit = false;

unsigned int digits(int n) {
    if (n == 0) return 1;
    return (unsigned int) floor(log10(abs(n))) + 1;
}

void wrongOptionValue(char *opt, char *val) {
    fprintf(stdout, "Wrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

void readOptions(
        int argc,
        char **argv,
        int *id,                            /*id*/
        char **common_dir,                  /*common_dir*/
        char **input_dir,                   /*input_dir*/
        char **mirror_dir,                  /*input_dir*/
        unsigned long int *buffer_size,     /*buffer_size*/
        char **log_file                     /*log_file*/
) {
    int i;
    char *opt, *optVal;
    for (i = 1; i < argc; ++i) {
        opt = argv[i];
        optVal = argv[i + 1];
        if (strcmp(opt, "-n") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *id = (int) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-c") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *common_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-i") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *input_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-m") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *mirror_dir = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-b") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *buffer_size = (unsigned long int) strtol(optVal, NULL, 10);
            } else {
                wrongOptionValue(opt, optVal);
            }
        } else if (strcmp(opt, "-l") == 0) {
            if (optVal != NULL && optVal[0] != '-') {
                *log_file = optVal;
            } else {
                wrongOptionValue(opt, optVal);
            }
        }
    }
}

void sender(int receiverId) {
    char *buffer = NULL;
    size_t lb = 0;
    printf("\nSENDER PID: [%d], PARENT PID: [%d]\n", getpid(), getppid());
    lb = (size_t) (strlen(common_dir) + digits(id) + digits(receiverId) + 15);
    if ((buffer = malloc(lb))) {
        sprintf(buffer, "%s/id%d_to_id%d.fifo", common_dir, id, receiverId);
        printf("FIFO: [%s]\n", buffer);
        if (mkfifo(buffer, 0755)) {
            perror(buffer);
        }

        //TODO: Open fifo

        //TODO: Read fifo

        //TODO: Close fifo

        free(buffer);
    } else {
        perror("malloc");
    }
}

void receiver(int senderId) {
    char *buffer = NULL;
    size_t lb = 0;
    printf("\nRECEIVER PID: [%d], PARENT PID: [%d]\n", getpid(), getppid());
    lb = (size_t) (strlen(common_dir) + digits(senderId) + digits(id) + 15);
    if ((buffer = malloc(lb))) {
        sprintf(buffer, "%s/id%d_to_id%d.fifo", common_dir, senderId, id);
        printf("FIFO: [%s]\n", buffer);
        if (mkfifo(buffer, 0755)) {
            perror(buffer);
        }

        //TODO: Open fifo

        //TODO: Read fifo

        //TODO: Close fifo

        free(buffer);
    } else {
        perror("malloc");
    }
}

void sig_int_quit_action(int signal) {
    char *buffer = NULL;
    size_t lb = 0;

    printf("sig_int_quit_action ::: signo: %d\n", signal);

    rmdir(mirror_dir);

    lb = (size_t) (strlen(common_dir) + digits(id)) + 5;
    if ((buffer = malloc(lb))) {

        sprintf(buffer, "%s/%d.id", common_dir, id);

        if (unlink(buffer) < 0) {
            perror(buffer);
        }

        if (receiver_pid) {
            kill(receiver_pid, SIGUSR2);
        }

        if (sender_pid) {
            kill(sender_pid, SIGUSR2);
        }

        free(buffer);
    } else {
        perror("malloc");
    }


    quit = true;
}

char *clientCreate(char *fn) {
    return fn;
}

int clientCompare(char *fn1, char *fn2) {
    return strcmp(fn1, fn2);
}

unsigned long int clientHash(char *key, unsigned long int capacity) {
    int i, sum = 0;
    size_t keyLength = strlen(key);
    for (i = 0; i < keyLength; i++) {
        sum += key[i];
    }
    return sum % capacity;
}

void clientDestroy(char *fn) {
    free(fn);
}

/*
 * Check */
void create(char *filename) {
    struct stat s = {0};
    char *buffer = NULL, *f = NULL, *fn = NULL;
    int client = 0;
    size_t lb;

    lb = strlen(common_dir) + strlen(filename) + 2;

    /* Make a copy of filename.*/
    f = malloc(sizeof(char) * strlen(filename) + 1);
    strcpy(f, filename);

    client = (int) strtol(strtok(filename, "."), NULL, 10);
    if (client > 0 && client != id) {

        /* Check file suffix to determine if it ends with '.id'.*/
        if (!strcmp(strtok(NULL, "\0"), "id")) {

            if (HT_Insert(clientsHT, f, f, (void **) &fn)) {
                if ((buffer = malloc(lb))) {
                    sprintf(buffer, "%s/%s", common_dir, f);
                    if (!stat(buffer, &s)) {
                        if (!S_ISDIR(s.st_mode)) {

                            /* Create sender.*/
                            sender_pid = fork();
                            if (sender_pid < 0) {
                                perror("fork");
                                exit(EXIT_FAILURE);
                            }
                            if (sender_pid == 0) {
                                sender(client);
                                exit(EXIT_SUCCESS);
                            }

                            /* Create receiver.*/
                            receiver_pid = fork();
                            if (receiver_pid < 0) {
                                perror("fork");
                                exit(EXIT_FAILURE);
                            }
                            if (receiver_pid == 0) {
                                receiver(client);
                                exit(EXIT_SUCCESS);
                            }
                            sleep(1);
                        }
                    } else {
                        perror("stat");
                    }
                    free(buffer);
                }
            } else {
                fprintf(stderr, "\n---HT File: [%s] already exists!---\n", fn);
            }
        }
    }
}

void destroy(char *filename) {
    char *buffer = NULL, *f = NULL, *folder = NULL;
    size_t lb = 0;
    __pid_t d_pid = 0;

    /* Make a copy of filename.*/
    f = malloc(sizeof(char) * strlen(filename) + 1);
    strcpy(f, filename);

    folder = strtok(filename, ".");
    if (!strcmp(strtok(NULL, "\0"), "id")) {

        d_pid = fork();
        if (d_pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (d_pid == 0) {

            /* Allocate space for target dir.*/
            lb = (size_t) (strlen(mirror_dir) + strlen(folder)) + 2;
            if ((buffer = malloc(lb))) {
                sprintf(buffer, "%s/%s", mirror_dir, folder);
                printf("\nbuffer: [%s]\n", buffer);
                rmdir(buffer);
                execlp("rm", "-r", "-f", buffer, NULL);
                perror("execlp");
                free(buffer);
            } else {
                perror("malloc");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
    }

    free(f);
}

int main(int argc, char *argv[]) {
    char event_buffer[EVENT_BUF_LEN], *buffer = NULL;
    unsigned long int buffer_size = 0;
    FILE *fd_common_dir = NULL, *fd_log_file = NULL;
    int fd_inotify = 0, ev, wd;
    struct stat s = {0};
    ssize_t bytes;
    static struct sigaction act;
    size_t lb = 0;

    struct dirent *d = NULL;
    DIR *dir = NULL;


    char *filename = NULL;
    struct inotify_event *event = NULL;


    printf("pid: %d\n", getpid());

    /* Read argument options from command line*/
    readOptions(argc, argv, &id, &common_dir, &input_dir, &mirror_dir, &buffer_size, &log_file);

    assert(id > 0);
    assert(common_dir != NULL);
    assert(input_dir != NULL);
    assert(mirror_dir != NULL);
    assert(buffer_size > 0);
    assert(log_file != NULL);

    /* Check if input_dir directory exists.*/
    if (!stat(input_dir, &s)) {
        if (!S_ISDIR(s.st_mode)) {
            fprintf(stderr, "'%s' is not a directory!\n", input_dir);
            exit(EXIT_FAILURE);
        }
    } else {
        perror(input_dir);
        exit(EXIT_FAILURE);
    }

    /* Check if mirror_dir directory already exists.*/
    if (!stat(mirror_dir, &s)) {
        fprintf(stderr, "'%s' directory already exists!\n", mirror_dir);
        exit(EXIT_FAILURE);
    } else {
        mkdir(mirror_dir, 0777);
    }

    /* Create common_dir*/
    mkdir(common_dir, 0777);

    /* Prepare *.id file path*/
    lb = (size_t) (strlen(common_dir) + digits(id)) + 5;
    if ((buffer = malloc(lb))) {
        sprintf(buffer, "%s/%d.id", common_dir, id);

        /* Check if [id].id file exists.*/
        if (!stat(buffer, &s)) {
            fprintf(stderr, "'%s' already exists!\n", buffer);
            exit(EXIT_FAILURE);
        } else {
            fd_common_dir = fopen(buffer, "w");
            fprintf(fd_common_dir, "%d", (int) getpid());
        }
        free(buffer);

    } else {
        perror("malloc");
        exit(EXIT_FAILURE);
    }


    /* Check if log_file file already exists.*/
    if (!stat(log_file, &s)) {
        fprintf(stderr, "'%s' file already exists!\n", log_file);
        exit(EXIT_FAILURE);
    } else {
        fd_log_file = fopen(log_file, "w");
    }

    /* Initialize inotify.*/
    fd_inotify = inotify_init();
    if (fd_inotify < 0) {
        perror("i-notify_init");
    }

    /* Set custom signal action for SIGINT (^c) & SIGQUIT (^\) signals.*/
    act.sa_handler = sig_int_quit_action;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    /* Add common_dir at watch list to detect changes.*/
    wd = inotify_add_watch(fd_inotify, common_dir, IN_CREATE | IN_DELETE);

    /*Initialize clients hashtable*/
    HT_Init(
            &clientsHT,
            100,
            512,
            (pointer (*)(pointer)) clientCreate,
            (int (*)(pointer, pointer)) clientCompare,
            (unsigned long (*)(pointer, unsigned long int)) clientHash,
            (unsigned long (*)(pointer)) clientDestroy
    );

    printf("\n::::::::::::::::::::::::::::::::::::::::::::::::::\n");
    // Search for not processed clients.
    if ((dir = opendir(common_dir))) {
        while ((d = readdir(dir))) {
            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
                continue;
            }
            create(d->d_name);
        }
        closedir(dir);
    }
    printf("\n::::::::::::::::::::::::::::::::::::::::::::::\n\n");

    while (!quit) {
        bytes = read(fd_inotify, event_buffer, EVENT_BUF_LEN);
        if (bytes < 0) {
            perror("read i-notify event");
        }
        ev = 0;
        while (ev < bytes) {
            event = (struct inotify_event *) &event_buffer[ev];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    if (!(event->mask & IN_ISDIR)) {
                        create(event->name);
                    }
                } else if (event->mask & IN_DELETE) {
                    if (!(event->mask & IN_ISDIR)) {
                        destroy(event->name);
                    }
                }
                ev += EVENT_SIZE + event->len;
            }
        }
    }

    /* Remove common_dir from watch list.*/
    inotify_rm_watch(fd_inotify, wd);

    /* Close the i-notify instance.*/
    close(fd_inotify);

    fclose(fd_common_dir);

    fclose(fd_log_file);

    return 0;
}
