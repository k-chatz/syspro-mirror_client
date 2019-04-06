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
#include <fcntl.h>
#include "hash.h"

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

/**
 * Interupt or quit action*/
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

/**
 * Destroy child*/
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
        }
    }
    free(f);
}

int main(int argc, char *argv[]) {
    char event_buffer[EVENT_BUF_LEN], *buffer = NULL;
    FILE *fd_common_dir = NULL, *fd_log_file = NULL;
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

    fclose(fd_common_dir);

    fclose(fd_log_file);

    return 0;
}
