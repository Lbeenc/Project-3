/* oss.c - Parent Process */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <string.h>

#define MAX_PROCESSES 20
#define CLOCK_INCREMENT_MS 250

// Define message structure
struct msgbuf {
    long mtype;
    int data;
};

// Define Process Control Block
struct PCB {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
    int messagesSent;
};

struct PCB processTable[MAX_PROCESSES];

// Shared memory for system clock
int shm_id;
int *sysClock;
int msg_id;
FILE *logfile;

void cleanup(int signum) {
    printf("\nTerminating... Cleaning up shared memory and message queue.\n");
    shmctl(shm_id, IPC_RMID, NULL);
    msgctl(msg_id, IPC_RMID, NULL);
    fclose(logfile);
    exit(0);
}

void incrementClock(int activeProcesses) {
    int increment = CLOCK_INCREMENT_MS / (activeProcesses ? activeProcesses : 1);
    *sysClock += increment;
}

int main(int argc, char *argv[]) {
    int n = 5, s = 3, t = 7, i = 100;
    char *log_filename = "oss.log";
    
    signal(SIGINT, cleanup);
    signal(SIGALRM, cleanup);
    alarm(60);
    
    // Parse command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "hn:s:t:i:f:")) != -1) {
        switch (opt) {
            case 'n': n = atoi(optarg); break;
            case 's': s = atoi(optarg); break;
            case 't': t = atoi(optarg); break;
            case 'i': i = atoi(optarg); break;
            case 'f': log_filename = optarg; break;
            case 'h':
                printf("Usage: oss [-n proc] [-s simul] [-t timelimit] [-i interval] [-f logfile]\n");
                exit(0);
        }
    }
    
    logfile = fopen(log_filename, "w");
    if (!logfile) {
        perror("Failed to open log file");
        exit(1);
    }
    
    // Initialize shared memory
    shm_id = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    sysClock = (int *)shmat(shm_id, NULL, 0);
    *sysClock = 0;
    
    // Initialize message queue
    msg_id = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
    
    for (int i = 0; i < MAX_PROCESSES; i++)
        processTable[i].occupied = 0;
    
    int activeProcesses = 0;
    int currentProcess = 0;
    
    while (activeProcesses < n) {
        if (activeProcesses < s) {
            int index = -1;
            for (int j = 0; j < MAX_PROCESSES; j++) {
                if (!processTable[j].occupied) {
                    index = j;
                    break;
                }
            }
            if (index != -1) {
                pid_t pid = fork();
                if (pid == 0) {
                    char sec[10], nano[10];
                    sprintf(sec, "%d", rand() % t + 1);
                    sprintf(nano, "%d", rand() % 1000000000);
                    execl("./worker", "worker", sec, nano, NULL);
                    perror("execl failed");
                    exit(1);
                } else {
                    processTable[index].occupied = 1;
                    processTable[index].pid = pid;
                    processTable[index].startSeconds = *sysClock;
                    processTable[index].messagesSent = 0;
                    activeProcesses++;
                    fprintf(logfile, "OSS: Launched process %d at %d\n", pid, *sysClock);
                }
            }
        }
        
        if (activeProcesses > 0) {
            struct msgbuf message;
            message.mtype = processTable[currentProcess].pid;
            message.data = 1;
            msgsnd(msg_id, &message, sizeof(message.data), 0);
            
            msgrcv(msg_id, &message, sizeof(message.data), 1, 0);
            fprintf(logfile, "OSS: Received message from process %d\n", processTable[currentProcess].pid);
            
            if (message.data == 0) {
                fprintf(logfile, "OSS: Process %d terminated\n", processTable[currentProcess].pid);
                waitpid(processTable[currentProcess].pid, NULL, 0);
                processTable[currentProcess].occupied = 0;
                activeProcesses--;
            }
            
            currentProcess = (currentProcess + 1) % MAX_PROCESSES;
        }
        
        incrementClock(activeProcesses);
    }
    
    cleanup(0);
    return 0;
}
