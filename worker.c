/* worker.c - Child Process */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <string.h>

struct msgbuf {
    long mtype;
    int data;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: worker <seconds> <nanoseconds>\n");
        exit(1);
    }
    
    int termSeconds = atoi(argv[1]);
    int termNano = atoi(argv[2]);
    
    int shm_id = shmget(IPC_PRIVATE, sizeof(int), 0666);
    int *sysClock = (int *)shmat(shm_id, NULL, 0);
    
    int msg_id = msgget(IPC_PRIVATE, 0666);
    struct msgbuf message;
    
    int iterations = 0;
    while (1) {
        msgrcv(msg_id, &message, sizeof(message.data), getpid(), 0);
        
        printf("WORKER PID:%d PPID:%d SysClockS:%d TermTimeS:%d\n", getpid(), getppid(), *sysClock, termSeconds);
        
        if (*sysClock >= termSeconds) {
            printf("WORKER PID:%d -- Terminating after %d iterations\n", getpid(), iterations);
            message.mtype = 1;
            message.data = 0;
            msgsnd(msg_id, &message, sizeof(message.data), 0);
            break;
        }
        
        message.mtype = 1;
        message.data = 1;
        msgsnd(msg_id, &message, sizeof(message.data), 0);
        iterations++;
    }
    
    shmdt(sysClock);
    return 0;
}
