all: oss worker

oss: oss.c
	gcc oss.c -o oss -lrt

worker: worker.c
	gcc worker.c -o worker -lrt

clean:
	rm -f oss worker
