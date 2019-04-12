#ifndef RECEIVER_H
#define RECEIVER_H

void receiver(int sender_id, int id, char *common_dir, char *input_dir, char *mirror_dir, unsigned long int buffer_size,
              FILE *logfile);

#endif
