CC = gcc
CFLAGS = -Wall -g
TARGETS = oss worker

all: $(TARGETS)

oss: oss.c
	$(CC) $(CFLAGS) -o oss oss.c

worker: worker.c
	$(CC) $(CFLAGS) -o worker worker.c

clean:
	rm -f $(TARGETS) *.o
