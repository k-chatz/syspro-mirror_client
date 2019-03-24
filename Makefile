OBJS	= main.o
SOURCE	= main.c
HEADER	=
OUT	= mirror_client
CC	 = gcc
FLAGS	 = -c -Wall

all: $(OBJS)
	$(CC) $(OBJS) -o $(OUT)

main.o: main.c
	$(CC) $(FLAGS) main.c

clean:
	rm -f $(OBJS) $(OUT)
