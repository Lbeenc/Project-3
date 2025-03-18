#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>

struct msgbuf {
    long mtype;
    int data;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: worker seconds nanoseconds\n");
        exit(1);
    }

    int termSec = atoi(argv[1]);
    int termNano = atoi(argv[2]);

    key_t key = ftok(".", 'm');
    int msgQueueID = msgget(key, 0666);
    if (msgQueueID == -1) {
        perror("Worker: Message queue error");
        exit(1);
    }

    while (1) {
        struct msgbuf message;
        msgrcv(msgQueueID, &message, sizeof(message.data), getpid(), 0);

        printf("WORKER PID:%d -- Received message at SysClock %d:%d\n", getpid(), termSec, termNano);

        if (termSec <= 2) {
            message.mtype = getpid();
            message.data = 0;
            msgsnd(msgQueueID, &message, sizeof(message.data), 0);
            printf("WORKER PID:%d -- Terminating at SysClock %d:%d\n", getpid(), termSec, termNano);
            exit(0);
        }

        termSec--;
    }
}
