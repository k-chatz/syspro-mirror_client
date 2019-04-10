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
FILE *logfile = NULL;

/**
 * Calculate number of digits of specific int.*/
unsigned int _digits(int n) {
    if (n == 0) return 1;
    return (unsigned int) floor(log10(abs(n))) + 1;
}

int _rmdir(char *dir) {
    int status = EXIT_FAILURE;
    __pid_t d_pid = 0;
    d_pid = fork();
    if (d_pid == 0) {
        execlp("rm", "rm", "-r", "-f", dir, NULL);
        fprintf(stderr, "\n%s:%d-[%s] execlp error: '%s'\n", __FILE__, __LINE__, dir, strerror(errno));
    } else if (d_pid > 0) {
        waitpid(d_pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            return WEXITSTATUS(status);
        }
    } else {
        fprintf(stderr, "\n%s:%d-fork error: '%s'\n", __FILE__, __LINE__, strerror(errno));
    }
    return status;
}

void wrongOptionValue(char *opt, char *val) {
    fprintf(stderr, "\nWrong value [%s] for option '%s'\n", val, opt);
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
 * @Signal_handler
 * Interupt or quit action*/
void sig_int_quit_action(int signal) {
    fprintf(stdout, "\n-Client: [%d:%d]: signal: %d - exiting...\n", id, getpid(), signal);
    quit = true;
}

/**
 * @Signal_handler
 * Child finish*/
void sig_usr_1_action(int signal) {
    fprintf(stdout, "\n-Client: [%d:%d]: child send finish signal!\n", id, getpid());
}

/**
 * @Signal_handler
 * Child alarm timeout*/
void sig_usr_2_action(int signal) {
    fprintf(stdout, "\n-Client: [%d:%d]: child send alarm timeout!\n", id, getpid());
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
    //free(fn);
}

/**
 * @inotify_create_event
 * */
void create(char *filename) {
    struct stat s = {0};
    char id_file[PATH_MAX + 1], f[strlen(filename) + 1], *fn = NULL, *f_suffix = NULL;
    int client = 0;

    strcpy(f, filename);

    client = (int) strtol(strtok(filename, "."), NULL, 10);

    if (client > 0 && client != id) {
        f_suffix = strtok(NULL, "\0");
        if (f_suffix != NULL && !strcmp(f_suffix, "id")) {
            if (HT_Insert(clientsHT, f, f, (void **) &fn)) {

                /* Construct id file*/
                if (sprintf(id_file, "%s/%s", common_dir, f) < 0) {
                    fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
                    exit(EXIT_FAILURE);
                }

                if (!stat(id_file, &s)) {
                    if (!S_ISDIR(s.st_mode)) {
                        /* Create sender.*/
                        sender_pid = fork();
                        if (sender_pid < 0) {
                            fprintf(stderr, "\n%s:%d-Sender fork error: '%s'\n", __FILE__, __LINE__, strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        if (sender_pid == 0) {
                            sender(client);
                            exit(EXIT_SUCCESS);
                        }

                        /* Create receiver.*/
                        receiver_pid = fork();
                        if (receiver_pid < 0) {
                            fprintf(stderr, "\n%s:%d-Receiver fork error: '%s'\n", __FILE__, __LINE__,
                                    strerror(errno));
                            exit(EXIT_FAILURE);
                        }
                        if (receiver_pid == 0) {
                            receiver(client);
                            exit(EXIT_SUCCESS);
                        }
                    }
                } else {
                    fprintf(stderr, "\n%s:%d-[%s] stat error: '%s'\n", __FILE__, __LINE__, id_file, strerror(errno));
                    exit(EXIT_FAILURE);
                }
            } else {
                fprintf(stderr, "\n%s:%d-HT File: [%s] already exists!\n", __FILE__, __LINE__, fn);
            }
        }
    }
}

/**
 * @inotify_delete_event
 * */
void destroy(char *filename) {
    char path[PATH_MAX + 1], *f = NULL, *folder = NULL, *f_suffix = NULL;
    int status = 0, retry = 2;

    /* Make a copy of filename.*/
    if (!(f = malloc(sizeof(char) * strlen(filename) + 1))) {
        exit(EXIT_FAILURE);
    }

    strcpy(f, filename);

    folder = strtok(filename, ".");
    f_suffix = strtok(NULL, "\0");

    if (f_suffix != NULL && !strcmp(f_suffix, "id")) {

        /* Construct path.*/
        if (sprintf(path, "%s/%s", mirror_dir, folder) < 0) {
            fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        }

        while ((status = _rmdir(path)) && retry-- > 0);

        if (status == EXIT_SUCCESS) {
            printf("Dir '%s' removed successfully!\n", path);
            if (!HT_Remove(clientsHT, f, f, false)) {
                fprintf(stderr, "\n%s:%d-HT_Remove error\n", __FILE__, __LINE__);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    char event_buffer[EVENT_BUF_LEN], id_file[PATH_MAX + 1];
    static struct sigaction quit_action, child_alarm, child_finish;
    int fd_inotify = 0, ev, wd, status = 0, tries = 3;
    struct inotify_event *event = NULL;
    struct dirent *d = NULL;
    struct stat s = {0};
    FILE *file_id = NULL;
    ssize_t bytes = 0;
    DIR *dir = NULL;
    __pid_t wpid = 0;

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
            fprintf(stderr, "\n'%s' is not a directory!\n", input_dir);
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "\n%s:%d-[%s] stat error: '%s'\n", __FILE__, __LINE__, input_dir, strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* Check if mirror_dir directory already exists.*/
    if (!stat(mirror_dir, &s)) {
        fprintf(stderr, "\n'%s' directory already exists!\n", mirror_dir);
        exit(EXIT_FAILURE);
    } else {
        mkdir(mirror_dir, S_IRUSR | S_IWUSR | S_IXUSR);
    }

    /* Create common_dir*/
    mkdir(common_dir, S_IRUSR | S_IWUSR | S_IXUSR);

    if (sprintf(id_file, "%s/%d.id", common_dir, id) < 0) {
        fprintf(stderr, "\n%s:%d-sprintf error\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }

    /* Check if [id].id file exists.*/
    if (!stat(id_file, &s)) {
        fprintf(stderr, "\n%s:%d-file '%s' already exists!\n", __FILE__, __LINE__, id_file);
        exit(EXIT_FAILURE);
    } else {
        file_id = fopen(id_file, "w");
        fprintf(file_id, "%d", (int) getpid());
        fclose(file_id);
    }

    /* Check if log_file file already exists.*/
    if (!stat(log_file, &s)) {
        fprintf(stderr, "\n%s:%d-file '%s' already exists!\n", __FILE__, __LINE__, log_file);
        exit(EXIT_FAILURE);
    } else {
        if ((logfile = fopen(log_file, "w")) == NULL) {
            fprintf(stderr, "\n%s:%d-[%s] open error: '%s'\n", __FILE__, __LINE__, log_file, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }

    fprintf(logfile, "cl %d\n", id);
    fflush(logfile);

    /* Initialize inotify.*/
    if ((fd_inotify = inotify_init()) < 0) {
        fprintf(stderr, "\n%s:%d-i-notify_init error: '%s'\n", __FILE__, __LINE__, strerror(errno));
    }

    /* Set custom signal handler for SIGINT (^c) & SIGQUIT (^\) signals.*/
    quit_action.sa_handler = sig_int_quit_action;
    sigfillset(&(quit_action.sa_mask));
    sigaction(SIGINT, &quit_action, NULL);
    sigaction(SIGQUIT, &quit_action, NULL);
    sigaction(SIGHUP, &quit_action, NULL);

    /* Set custom signal handler for SIGUSR1 (Child alarm timeout) signal.*/
    child_finish.sa_handler = sig_usr_1_action;
    sigfillset(&(child_finish.sa_mask));
    sigaction(SIGUSR1, &child_finish, NULL);

    /* Set custom signal handler for SIGUSR2 (Child alarm timeout) signal.*/
    child_alarm.sa_handler = sig_usr_2_action;
    sigfillset(&(child_alarm.sa_mask));
    sigaction(SIGUSR2, &child_alarm, NULL);

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

    /* Read i-notify events*/
    while (!quit) {
        if ((bytes = read(fd_inotify, event_buffer, EVENT_BUF_LEN)) < 0) {
            fprintf(stderr, "\n%s:%d-i-notify event read error: '%s'\n", __FILE__, __LINE__, strerror(errno));
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

    while ((wpid = wait(&status)) > 0);

    fprintf(logfile, "cl %d\n", id);
    fflush(logfile);

    if (unlink(id_file) < 0) {
        fprintf(stderr, "\n%s:%d-[%s] unlink error: '%s'\n", __FILE__, __LINE__, id_file, strerror(errno));
    }

    while ((status = _rmdir(mirror_dir)) && tries-- > 0);

    if (status == EXIT_FAILURE) {
        fprintf(stderr, "\n%s:%d- _rmdir error'\n", __FILE__, __LINE__);
    }

    fclose(logfile);

    return 0;
}
