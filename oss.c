/* oss.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define MAX_PROCESSES 20
#define MSGKEY 1234
#define SHMKEY 5678

/* Structure for message queue */
typedef struct msgbuf {
    long mtype;
    int msg;
} message;

/* Structure for PCB */
typedef struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int messagesSent;
} PCB;

PCB processTable[MAX_PROCESSES];
int shmid, msqid;
int *sysClock;
FILE *logfile;

void cleanup() {
    msgctl(msqid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    fclose(logfile);
}

void signalHandler(int signo) {
    printf("\nReceived signal. Cleaning up and terminating.\n");
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    int opt, n = 5, s = 3, t = 7, i = 100;
    char logFileName[256] = "oss.log";
    
    while ((opt = getopt(argc, argv, "n:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'i': i = atoi(optarg); break;
            case 'f': strncpy(logFileName, optarg, 255); break;
            default: fprintf(stderr, "Usage: %s [-n proc] [-s simul] [-t time] [-i interval] [-f logfile]\n", argv[0]); exit(EXIT_FAILURE);
        }
    }

    signal(SIGINT, signalHandler);
    signal(SIGALRM, signalHandler);
    alarm(60);

    /* Shared memory setup */
    shmid = shmget(SHMKEY, sizeof(int) * 2, IPC_CREAT | 0666);
    sysClock = (int *)shmat(shmid, NULL, 0);
    sysClock[0] = 0; sysClock[1] = 0;
    
    /* Message queue setup */
    msqid = msgget(MSGKEY, IPC_CREAT | 0666);

    logfile = fopen(logFileName, "w");

    int runningProcesses = 0;
    int launchedProcesses = 0;

    while (launchedProcesses < n || runningProcesses > 0) {
        if (runningProcesses < s && launchedProcesses < n) {
            /* Find an available PCB slot */
            int slot = -1;
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (!processTable[j].occupied) { slot = j; break; }
            }
            if (slot != -1) {
                pid_t pid = fork();
                if (pid == 0) {
                    char secArg[10], nanoArg[10];
                    snprintf(secArg, sizeof(secArg), "%d", (rand() % t) + 1);
                    snprintf(nanoArg, sizeof(nanoArg), "%d", rand() % 1000000);
                    execl("./worker", "worker", secArg, nanoArg, NULL);
                    exit(0);
                } else {
                    processTable[slot].occupied = 1;
                    processTable[slot].pid = pid;
                    processTable[slot].startSeconds = sysClock[0];
                    processTable[slot].startNano = sysClock[1];
                    processTable[slot].messagesSent = 0;
                    runningProcesses++;
                    launchedProcesses++;
                }
            }
        }

        /* Message coordination */
        for (int j = 0; j < MAX_PROCESSES; j++) {
            if (processTable[j].occupied) {
                message msg;
                msg.mtype = processTable[j].pid;
                msg.msg = 1;
                msgsnd(msqid, &msg, sizeof(int), 0);
                processTable[j].messagesSent++;

                msgrcv(msqid, &msg, sizeof(int), processTable[j].pid, 0);
                if (msg.msg == 0) {
                    waitpid(processTable[j].pid, NULL, 0);
                    processTable[j].occupied = 0;
                    runningProcesses--;
                }
            }
        }
        
        usleep(100000); /* Increment clock */
        sysClock[1] += 250000 / (runningProcesses > 0 ? runningProcesses : 1);
        if (sysClock[1] >= 1000000000) {
            sysClock[0]++;
            sysClock[1] -= 1000000000;
        }
    }
    cleanup();
    return 0;
}
