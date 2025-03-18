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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(1);
    }

    int termSeconds = atoi(argv[1]);
    int termNano = atoi(argv[2]);
    int sysClockS = 0, sysClockNano = 0;

    key_t key = ftok("oss.c", 65);
    int msg_id = msgget(key, 0666);

    printf("WORKER PID:%d -- Started, will run for %d sec %d ns\n", getpid(), termSeconds, termNano);

    while (1) {
        struct msgbuf message;
        msgrcv(msg_id, &message, sizeof(message.data), getpid(), 0);

        if (sysClockS > termSeconds || (sysClockS == termSeconds && sysClockNano >= termNano)) {
            message.data = 0;  // Indicate termination
            printf("WORKER PID:%d -- Terminating\n", getpid());
            msgsnd(msg_id, &message, sizeof(message.data), 0);
            break;
        }

        message.data = 1;  // Continue execution
        msgsnd(msg_id, &message, sizeof(message.data), 0);
    }

    return 0;
}

}
