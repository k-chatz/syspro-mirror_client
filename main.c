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

#define EVENT_SIZE (sizeof(struct inotify_event))
#define EVENT_BUF_LEN (1024 * (EVENT_SIZE + 16))

char *common_dir = NULL, *input_dir = NULL, *mirror_dir = NULL, *log_file = NULL;
unsigned long int id = 0;

void wrongOptionValue(char *opt, char *val) {
    fprintf(stdout, "Wrong value [%s] for option '%s'\n", val, opt);
    exit(EXIT_FAILURE);
}

void readOptions(
        int argc,
        char **argv,
        unsigned long int *id,              /*id*/
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
                *id = (unsigned long int) strtol(optVal, NULL, 10);
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
    printf("\nHI! I'm the sender, my pid is [%d], my parent is [%d]\n", getpid(), getppid());

    sleep(1000);
}


void receiver() {
    printf("\nHI! I'm the receiver, my pid is [%d], my parent is [%d]\n", getpid(), getppid());

    sleep(1000);
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

void in_delete(struct inotify_event *event) {
    __pid_t dpid;
    struct stat s = {0};
    char buffer[50];

    if (event->mask & IN_ISDIR) {
        printf("Directory: [%s] deleted.\n", event->name);
    } else {
        printf("File: [%s] deleted.\n", event->name);

        sprintf(buffer, "%s/%s", common_dir, event->name);

        fprintf(stdout, "'%s' is a *.id file, now I try to fork my self!\n", buffer);

        dpid = fork();
        if (dpid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (dpid == 0) {
            //Destroy childs for certain id
            exit(EXIT_SUCCESS);
        } else if (dpid > 0) {
            printf("HI! I'm parent, my pid is: [%d], I create child with pid [%d]\n", getpid(), dpid);
        }
    }
}

int main(int argc, char *argv[]) {
    char buffer[50], buf[EVENT_BUF_LEN];
    unsigned long int buffer_size = 0;
    FILE *fid = NULL, *flog = NULL;
    int inotfd = 0, i, wd;
    struct stat s = {0};
    ssize_t bytes;

    /*Read argument options from command line*/
    readOptions(argc, argv, &id, &common_dir, &input_dir, &mirror_dir, &buffer_size, &log_file);

    assert(id > 0);
    assert(common_dir != NULL);
    assert(input_dir != NULL);
    assert(mirror_dir != NULL);
    assert(buffer_size > 0);
    assert(log_file != NULL);

    /**
     * Check if input_dir directory exists.*/
    if (!stat(input_dir, &s)) {
        if (!S_ISDIR(s.st_mode)) {
            fprintf(stderr, "'%s' is not a directory!\n", input_dir);
            exit(EXIT_FAILURE);
        }
    } else {
        perror(input_dir);
        exit(EXIT_FAILURE);
    }

    /**
     * Check if mirror_dir directory already exists.*/
    if (!stat(mirror_dir, &s)) {
        fprintf(stderr, "'%s' directory already exists!\n", mirror_dir);
        exit(EXIT_FAILURE);
    } else {
        mkdir(mirror_dir, 0777);
    }

    /**
     * Create common_dir*/
    mkdir(common_dir, 0777);

    /**
     * Prepare *.id file path*/
    sprintf(buffer, "%s/%lu.id", common_dir, id);

    /**
     * Check if [id].id file exists.*/
    if (!stat(buffer, &s)) {
        fprintf(stderr, "'%s' already exists!\n", buffer);
        exit(EXIT_FAILURE);
    } else {
        fid = fopen(buffer, "w");
        fprintf(fid, "%d", (int) getpid());
    }

    /**
     * Check if log_file file already exists.*/
    if (!stat(log_file, &s)) {
        fprintf(stderr, "'%s' file already exists!\n", log_file);
        exit(EXIT_FAILURE);
    } else {
        flog = fopen(log_file, "w");
    }

    inotfd = inotify_init();
    if (inotfd < 0) {
        perror("inotify_init");
    }

    wd = inotify_add_watch(inotfd, common_dir, IN_CREATE | IN_DELETE);

    while (1) {
        bytes = read(inotfd, buf, EVENT_BUF_LEN);
        if (bytes < 0) {
            perror("read");
        }

        i = 0;
        while (i < bytes) {
            struct inotify_event *event = (struct inotify_event *) &buf[i];
            if (event->len) {
                if (event->mask & IN_CREATE) {
                    in_create(event);
                } else if (event->mask & IN_DELETE) {
                    in_delete(event);
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }

    /*removing the “/tmp” directory from the watch list.*/
    inotify_rm_watch(inotfd, wd);

    /*closing the INOTIFY instance*/
    close(inotfd);


    fclose(fid);
    fclose(flog);

    return 0;
}
