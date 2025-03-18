#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 20
#define CLOCK_KEY 1234
#define MSG_KEY 5678

struct Clock {
    int seconds;
    int nanoseconds;
};

struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int messagesSent;
};

struct Message {
    long mtype;
    int data;
};

struct PCB processTable[MAX_PROCESSES];
int shmid, msqid;
struct Clock *clockPtr;
FILE *logFile;

void cleanup() {
    msgctl(msqid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    fclose(logFile);
}

void signalHandler(int signo) {
    printf("\nTerminating all processes...\n");
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processTable[i].occupied) {
            kill(processTable[i].pid, SIGKILL);
        }
    }
    cleanup();
    exit(0);
}

void incrementClock(int activeChildren) {
    if (activeChildren > 0)
        clockPtr->nanoseconds += (250000000 / activeChildren);
    else
        clockPtr->nanoseconds += 250000000;
    while (clockPtr->nanoseconds >= 1000000000) {
        clockPtr->seconds++;
        clockPtr->nanoseconds -= 1000000000;
    }
}

int main(int argc, char *argv[]) {
    int maxProcesses = 5, maxSimultaneous = 3, maxTime = 10, interval = 100;
    char logFileName[50] = "oss.log";

    signal(SIGINT, signalHandler);

    shmid = shmget(CLOCK_KEY, sizeof(struct Clock), IPC_CREAT | 0666);
    clockPtr = (struct Clock *)shmat(shmid, NULL, 0);
    clockPtr->seconds = 0;
    clockPtr->nanoseconds = 0;

    msqid = msgget(MSG_KEY, IPC_CREAT | 0666);

    logFile = fopen(logFileName, "w");

    int launched = 0, activeChildren = 0, nextLaunchTime = 0;
    pid_t pid;
    while (launched < maxProcesses || activeChildren > 0) {
        if (activeChildren < maxSimultaneous && launched < maxProcesses && 
            clockPtr->seconds * 1000 + clockPtr->nanoseconds / 1000000 >= nextLaunchTime) {
            for (int i = 0; i < MAX_PROCESSES; i++) {
                if (!processTable[i].occupied) {
                    pid = fork();
                    if (pid == 0) {
                        execl("./worker", "worker", "5", "500000", NULL);
                        exit(1);
                    } else {
                        processTable[i].occupied = 1;
                        processTable[i].pid = pid;
                        processTable[i].startSeconds = clockPtr->seconds;
                        processTable[i].startNano = clockPtr->nanoseconds;
                        processTable[i].messagesSent = 0;
                        launched++;
                        activeChildren++;
                        nextLaunchTime = clockPtr->seconds * 1000 + clockPtr->nanoseconds / 1000000 + interval;
                        break;
                    }
                }
            }
        }

        for (int i = 0; i < MAX_PROCESSES; i++) {
            if (processTable[i].occupied) {
                struct Message msg;
                msg.mtype = processTable[i].pid;
                msg.data = 1;
                msgsnd(msqid, &msg, sizeof(int), 0);

                msgrcv(msqid, &msg, sizeof(int), processTable[i].pid, 0);
                if (msg.data == 0) {
                    waitpid(processTable[i].pid, NULL, 0);
                    processTable[i].occupied = 0;
                    activeChildren--;
                }
            }
        }

        incrementClock(activeChildren);
    }

    cleanup();
    return 0;
}
