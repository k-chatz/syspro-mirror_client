#ifndef SENDER_H
#define SENDER_H

extern char *common_dir, *input_dir, *mirror_dir, *log_file;
extern unsigned long int buffer_size;
extern int id;
FILE *logfile;

extern unsigned int digits(int n);

void sender(int rid);

#endif
