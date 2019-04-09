OBJS	= main.o hash.o sender.o receiver.o
SOURCE	= main.c hash.c sender.c receiver.c
HEADER	=
OUT	= mirror_client
CC	 = gcc
FLAGS	 = -c -Wall

all: $(OBJS)
	$(CC) $(OBJS) -o $(OUT)

main.o: main.c
	$(CC) $(FLAGS) main.c

hash.o: hash.c
	$(CC) $(FLAGS) hash.c

sender.o: sender.c
	$(CC) $(FLAGS) sender.c

receiver.o: receiver.c
	$(CC) $(FLAGS) receiver.c
clean:
	rm -f $(OBJS) $(OUT)
