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


#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

#define TRIES 3

char *common_dir = NULL, *input_dir = NULL, *mirror_dir = NULL, *log_file = NULL;
int id = 0;
__pid_t s_pid = 0, r_pid = 0;
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

void sender() {
    char *buffer = NULL;
    int senderId = id, receiverId = 0;
    printf("\nHI! I'm the sender, my pid is [%d], my parent is [%d]\n", getpid(), getppid());
    buffer = malloc((size_t) (strlen(common_dir) + digits(senderId) + digits(receiverId) + 15));
    sprintf(buffer, "%s/id%d_to_id%d.fifo", common_dir, senderId, receiverId);
    puts(buffer);

    free(buffer);
}

void receiver() {
    printf("\nHI! I'm the receiver, my pid is [%d], my parent is [%d]\n", getpid(), getppid());
    sleep(10);
}

void in_create(struct inotify_event *event) {
    __pid_t s_pid, r_pid;
    struct stat s = {0};
    char buffer[50];

    if (event->mask & IN_ISDIR) {
        printf("Directory: [%s] created.\n", event->name);
    } else {
        printf("File: [%s] created.\n", event->name);
        sprintf(buffer, "%s/%s", common_dir, event->name);

        //if is file
        //if is *.id file

        if (!stat(buffer, &s)) {
            if (!S_ISDIR(s.st_mode)) {

                fprintf(stdout, "'%s' is a *.id file, now I try to fork my self!\n", buffer);





                /* Create sender.*/
                s_pid = fork();
                if (s_pid < 0) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
                if (s_pid == 0) {
                    sender();
                    exit(EXIT_SUCCESS);
                }

                /* Create receiver.*/
                r_pid = fork();
                if (r_pid < 0) {
                    perror("fork");
                    exit(EXIT_FAILURE);
                }
                if (r_pid == 0) {
                    receiver();
                    exit(EXIT_SUCCESS);
                }


            }
        }
    }
}

void sig_int_quit_action(int signo) {
    char *buffer = NULL;
    printf("sig_int_quit_action ::: signo: %d\n", signo);

    rmdir(mirror_dir);

    buffer = malloc((size_t) (strlen(common_dir) + digits(id)) + 5);
    sprintf(buffer, "%s/%d.id", common_dir, id);

    if (unlink(buffer) < 0) {
        perror(buffer);
    }

    if (r_pid) {
        kill(r_pid, SIGUSR2);
    }

    if (s_pid) {
        kill(s_pid, SIGUSR2);
    }

    free(buffer);
    quit = true;
}

int main(int argc, char *argv[]) {
    char event_buffer[EVENT_BUF_LEN], *buffer = NULL;
    unsigned long int buffer_size = 0;
    FILE *fid = NULL, *flog = NULL;
    int inotfd = 0, ev, wd;
    struct stat s = {0};
    ssize_t bytes;
    static struct sigaction act;
    __pid_t d_pid = 0;
    char *folder = NULL;

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
    buffer = malloc((size_t) (strlen(common_dir) + digits(id)) + 5);
    sprintf(buffer, "%s/%d.id", common_dir, id);

    /* Check if [id].id file exists.*/
    if (!stat(buffer, &s)) {
        fprintf(stderr, "'%s' already exists!\n", buffer);
        exit(EXIT_FAILURE);
    } else {
        fid = fopen(buffer, "w");
        fprintf(fid, "%d", (int) getpid());
    }
    free(buffer);


    /* Check if log_file file already exists.*/
    if (!stat(log_file, &s)) {
        fprintf(stderr, "'%s' file already exists!\n", log_file);
        exit(EXIT_FAILURE);
    } else {
        flog = fopen(log_file, "w");
    }

    /* Initialize inotify.*/
    inotfd = inotify_init();
    if (inotfd < 0) {
        perror("inotify_init");
    }

    /* Set custom signal action for SIGINT (^c) & SIGQUIT (^\) signals.*/
    act.sa_handler = sig_int_quit_action;
    sigfillset(&(act.sa_mask));
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGQUIT, &act, NULL);

    /* Add common_dir at watch list to detect changes.*/
    wd = inotify_add_watch(inotfd, common_dir, IN_CREATE | IN_DELETE);

    while (!quit) {
        bytes = read(inotfd, event_buffer, EVENT_BUF_LEN);
        if (bytes < 0) {
            perror("read inotify event");
        }
        ev = 0;
        while (ev < bytes) {
            struct inotify_event *event = (struct inotify_event *) &event_buffer[ev];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    if (!(event->mask & IN_ISDIR)) {
                        printf("File: [%s] created.\n", event->name);

                        //TODO: New file was added... call scan_for_ids() funtion to find the new file.


                        /********************************************FORK**********************************************/
                        buffer = malloc((size_t) (strlen(common_dir) + strlen(event->name)) + 2);
                        sprintf(buffer, "%s/%s", common_dir, event->name);

                        //if is file
                        //if is *.id file

                        if (!stat(buffer, &s)) {
                            if (!S_ISDIR(s.st_mode)) {

                                fprintf(stdout, "'%s' is a *.id file, now I try to fork my self!\n", buffer);

                                /* Create sender.*/
                                s_pid = fork();
                                if (s_pid < 0) {
                                    perror("fork");
                                    exit(EXIT_FAILURE);
                                }
                                if (s_pid == 0) {
                                    sender();
                                    exit(EXIT_SUCCESS);
                                }


                                /* Create receiver.*/
                                r_pid = fork();
                                if (r_pid < 0) {
                                    perror("fork");
                                    exit(EXIT_FAILURE);
                                }
                                if (r_pid == 0) {
                                    receiver();
                                    exit(EXIT_SUCCESS);
                                }

                            }
                        }

                        free(buffer);
                        /**********************************************************************************************/

                    }
                } else if (event->mask & IN_DELETE) {
                    if (!(event->mask & IN_ISDIR)) {
                        printf("File: [%s] deleted.\n", event->name);
                        d_pid = fork();
                        if (d_pid < 0) {
                            perror("fork");
                            exit(EXIT_FAILURE);
                        }
                        if (d_pid == 0) {
                            folder = strtok(event->name, ".");
                            if (!strcmp(strtok(NULL, "\0"), "id")) {
                                /* Allocate space for target dir.*/
                                buffer = malloc((size_t) (strlen(mirror_dir) + strlen(folder)) + 2);
                                sprintf(buffer, "%s/%s", mirror_dir, folder);
                                printf("\nbuffer: [%s]\n", buffer);
                                rmdir(buffer);
                                free(buffer);
                                buffer = NULL;
                            }
                            exit(EXIT_SUCCESS);
                        }
                    }
                }
            }
            ev += EVENT_SIZE + event->len;
        }

        //TODO: call scan_for_ids() function.
    }

    /* Remove common_dir from watch list.*/
    inotify_rm_watch(inotfd, wd);

    /* Close the inotify instance.*/
    close(inotfd);

    fclose(fid);

    fclose(flog);

    return 0;
}
