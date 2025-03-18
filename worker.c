#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>

#define CLOCK_KEY 1234
#define MSG_KEY 5678

struct Clock {
    int seconds;
    int nanoseconds;
};

struct Message {
    long mtype;
    int data;
};

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: worker <seconds> <nanoseconds>\n");
        exit(1);
    }

    int durationSec = atoi(argv[1]);
    int durationNano = atoi(argv[2]);

    int shmid = shmget(CLOCK_KEY, sizeof(struct Clock), 0666);
    struct Clock *clockPtr = (struct Clock *)shmat(shmid, NULL, 0);

    int msqid = msgget(MSG_KEY, 0666);
    struct Message msg;
    int targetSec = clockPtr->seconds + durationSec;
    int targetNano = clockPtr->nanoseconds + durationNano;
    
    if (targetNano >= 1000000000) {
        targetSec++;
        targetNano -= 1000000000;
    }

    do {
        msgrcv(msqid, &msg, sizeof(int), getpid(), 0);

        printf("WORKER PID:%d SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n",
               getpid(), clockPtr->seconds, clockPtr->nanoseconds, targetSec, targetNano);

        if (clockPtr->seconds > targetSec ||
            (clockPtr->seconds == targetSec && clockPtr->nanoseconds >= targetNano)) {
            msg.data = 0;
            msgsnd(msqid, &msg, sizeof(int), 0);
            printf("WORKER PID:%d -- Terminating.\n", getpid());
            break;
        } else {
            msg.data = 1;
            msgsnd(msqid, &msg, sizeof(int), 0);
        }
    } while (1);

    return 0;
}

