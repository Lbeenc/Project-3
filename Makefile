all:
	gcc -Wall -g -o oss oss.c
	gcc -Wall -g -o worker worker.c

clean:
	rm -f oss worker log.txt
