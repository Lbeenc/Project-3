#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>

struct Clock {
    int seconds;
    int nanoseconds;
};

struct Message {
    long type;
    int value;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <seconds> <nanoseconds>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int term_sec = atoi(argv[1]);
    int term_nano = atoi(argv[2]);

    key_t shm_key = ftok("shmfile", 65);
    int shm_id = shmget(shm_key, sizeof(struct Clock), 0666);
    struct Clock *clock_shm = (struct Clock *)shmat(shm_id, NULL, 0);

    key_t msg_key = ftok("msgfile", 65);
    int msg_id = msgget(msg_key, 0666);

    printf("WORKER PID:%d PPID:%d TermTimeS:%d TermTimeNano:%d -- Just Starting\n",
           getpid(), getppid(), term_sec, term_nano);

    int iterations = 0;
    while (1) {
        struct Message msg;
        msgrcv(msg_id, &msg, sizeof(msg.value), getpid(), 0);
        printf("WORKER PID:%d SysClockS:%d SysClockNano:%d -- %d iterations passed\n",
               getpid(), clock_shm->seconds, clock_shm->nanoseconds, ++iterations);

        if (clock_shm->seconds >= term_sec && clock_shm->nanoseconds >= term_nano) {
            msg.value = 0;
            msgsnd(msg_id, &msg, sizeof(msg.value), 0);
            printf("WORKER PID:%d -- Terminating\n", getpid());
            break;
        } else {
            msg.value = 1;
            msgsnd(msg_id, &msg, sizeof(msg.value), 0);
        }
    }

    shmdt(clock_shm);
    exit(0);
}

