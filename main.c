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
#include <stdbool.h>
#include <fcntl.h>
#include "hash.h"
#include "sender.h"
#include "receiver.h"
#include <math.h>
#include <wait.h>

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

#define TRIES 3

typedef void *pointer;

Hashtable clientsHT = NULL;
char *common_dir = NULL, *input_dir = NULL, *mirror_dir = NULL, *log_file = NULL;
unsigned long int buffer_size = 0;
int id = 0;
__pid_t sender_pid = 0, receiver_pid = 0;
bool quit = false;

/**
 * Calculate number of digits of specific int.*/
unsigned int digits(int n) {
    if (n == 0) return 1;
    return (unsigned int) floor(log10(abs(n))) + 1;
}

void wrongOptionValue(char *opt, char *val) {
    fprintf(stdout, "Wrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

/**
 * Read options from command line*/
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

/**
 * Interupt or quit action*/
void sig_int_quit_action(int signal) {
    char *id_path = NULL;
    size_t lb = 0;

    printf("sig_int_quit_action ::: signo: %d\n", signal);

    rmdir(mirror_dir);

    lb = (size_t) (strlen(common_dir) + digits(id)) + 5;
    if (!(id_path = malloc(lb))) {
        exit(1);
    }

    sprintf(id_path, "%s/%d.id", common_dir, id);

    if (unlink(id_path) < 0) {
        perror(id_path);
    }

    if (receiver_pid) {
        kill(receiver_pid, SIGUSR2);
    }

    if (sender_pid) {
        kill(sender_pid, SIGUSR2);
    }

    free(id_path);

    quit = true;
}

/**
 * @Callback HT Create*/
char *clientCreate(char *fn) {
    return fn;
}

/**
 * @Callback HT Compare*/
int clientCompare(char *fn1, char *fn2) {
    return strcmp(fn1, fn2);
}

/**
 * @Callback HT Hash*/
unsigned long int clientHash(char *key, unsigned long int capacity) {
    int i, sum = 0;
    size_t keyLength = strlen(key);
    for (i = 0; i < keyLength; i++) {
        sum += key[i];
    }
    return sum % capacity;
}

/**
 * @Callback HT Destroy*/
void clientDestroy(char *fn) {
    free(fn);
}

/**
 * Check */
void create(char *filename) {
    struct stat s = {0};
    char *buffer = NULL, *f = NULL, *fn = NULL, *f_suffix = NULL;
    int client = 0;
    size_t lb;

    lb = strlen(common_dir) + strlen(filename) + 2;

    /* Make a copy of filename.*/
    f = malloc(sizeof(char) * strlen(filename) + 1);
    strcpy(f, filename);

    client = (int) strtol(strtok(filename, "."), NULL, 10);
    if (client > 0 && client != id) {
        f_suffix = strtok(NULL, "\0");
        if (f_suffix != NULL && !strcmp(f_suffix, "id")) {
            if (HT_Insert(clientsHT, f, f, (void **) &fn)) {

                if (!(buffer = malloc(lb))) {
                    exit(1);
                }

                /* Construct id file*/
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

            } else {
                fprintf(stderr, "\n---HT File: [%s] already exists!---\n", fn);
            }
        }
    }
}

/**
 * Destroy child*/
void destroy(char *filename) {
    char *path = NULL, *f = NULL, *folder = NULL, *f_suffix = NULL;
    int status = 0;
    __pid_t d_pid = 0;

    /* Make a copy of filename.*/
    f = malloc(sizeof(char) * strlen(filename) + 1);

    strcpy(f, filename);

    folder = strtok(filename, ".");
    f_suffix = strtok(NULL, "\0");

    if (f_suffix != NULL && !strcmp(f_suffix, "id")) {

        /* Allocate space for target dir.*/
        if (!(path = malloc((size_t) (strlen(mirror_dir) + strlen(folder)) + 2))) {
            exit(EXIT_FAILURE);
        }

        /* Construct path.*/
        sprintf(path, "%s/%s", mirror_dir, folder);

        d_pid = fork();
        if (d_pid == 0) {
            execlp("rm", "-r", "-f", path, NULL);
            perror("execlp");
        } else if (d_pid > 0) {
            waitpid(d_pid, &status, 0);
            if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                if (HT_Remove(clientsHT, f, f, false)) {
                    printf("Dir '%s' removed successfully!\n", path);
                }
            }
        } else {
            fprintf(stderr, "Fork error!");
            exit(EXIT_FAILURE);
        }
    }
    free(f);
}

int main(int argc, char *argv[]) {
    char event_buffer[EVENT_BUF_LEN], *buffer = NULL;
    FILE *file_id = NULL, *file_log = NULL;
    int fd_inotify = 0, ev, wd;
    struct stat s = {0};
    ssize_t bytes;
    static struct sigaction act;
    size_t lb = 0;
    struct dirent *d = NULL;
    DIR *dir = NULL;
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
        mkdir(mirror_dir, S_IRUSR | S_IWUSR | S_IXUSR);
    }

    /* Create common_dir*/
    mkdir(common_dir, S_IRUSR | S_IWUSR | S_IXUSR);

    /* Prepare *.id file path*/
    lb = (size_t) (strlen(common_dir) + digits(id)) + 5;
    if ((buffer = malloc(lb))) {
        sprintf(buffer, "%s/%d.id", common_dir, id);

        /* Check if [id].id file exists.*/
        if (!stat(buffer, &s)) {
            fprintf(stderr, "'%s' already exists!\n", buffer);
            exit(EXIT_FAILURE);
        } else {
            file_id = fopen(buffer, "w");
            fprintf(file_id, "%d", (int) getpid());
            fclose(file_id);
        }
        free(buffer);

    } else {
        exit(EXIT_FAILURE);
    }

    /* Check if log_file file already exists.*/
    if (!stat(log_file, &s)) {
        fprintf(stderr, "'%s' file already exists!\n", log_file);
        exit(EXIT_FAILURE);
    } else {
        file_log = fopen(log_file, "w");
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

    /* Initialize clients hashtable*/
    HT_Init(
            &clientsHT,
            100,
            512,
            (pointer (*)(pointer)) clientCreate,
            (int (*)(pointer, pointer)) clientCompare,
            (unsigned long (*)(pointer, unsigned long int)) clientHash,
            (unsigned long (*)(pointer)) clientDestroy
    );

    /* Search for not processed clients.*/
    if ((dir = opendir(common_dir))) {
        while ((d = readdir(dir))) {
            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, "..")) {
                continue;
            }
            create(d->d_name);
        }
        closedir(dir);
    }

    printf("\n:READ EVENTS:\n\n");

    while (!quit) {
        if ((bytes = read(fd_inotify, event_buffer, EVENT_BUF_LEN)) < 0) {
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
                        //quit = true;
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

    fclose(file_log);
    return 0;
}
